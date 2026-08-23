// HDD saver 2.0 - dumps the whole 60MB MFM/RLL drive to 1.44MB floppies
//
// floppy layout (per disk):
//   LBA 0      : header sector, see definitions.h and buildHeader()
//   LBA 1..9   : reserved (zeros)
//   LBA 10..2879: payload = consecutive sectors of the hdd image
//
// header sector layout (all multi byte values little endian):
//   off    0 : magic 'HDSV'
//   off    4 : version, currently 2
//   off    5 : flags, bit0 = this is the last disk of the dump
//   off    6 : disk index, unsigned long
//   off   10 : start LBA on hdd of this disks payload, unsigned long
//   off   14 : payload sector count, unsigned int
//   off   16 : crc16-ccitt (init FFFF) over the payload as written to disk
//   off   18 : bad hdd sector count for this disk, unsigned int
//   off   20 : total hdd sectors, unsigned long
//   off   24 : bad sector LBAs of this disk, up to MAX_BAD unsigned longs
//   off  216 : tool id string
//   off  254 : bad floppy sector count for this disk, unsigned int
//   off  256 : bad floppy payload offsets, up to MAX_BAD_FLP unsigned ints
//   off  500 : crc16 over header bytes 0..499
//
// fully unattended operation:
// - unreadable hdd sectors: retried RETRY_HDD times, then filled with the
//   BADFILL pattern, LBA logged in the header, copying continues
// - floppy writes: verified by reading back; on repeated failure the sector
//   is given up, filled with BADFILL, its payload offset logged in the
//   header and copying continues. too many failures = tool asks for a
//   fresh disk and the same disk content is dumped again
// payload crc therefore covers the stream with every untrustworthy sector
// replaced by the BADFILL pattern
//
// this file holds the application logic: session state, batching,
// per-disk flow and the startup prompts. hardware access lives in
// int13.c/hdd.c/floppy.c, output in print.c, input in keyboard.c

#include "definitions.h"
#include "math.h"
#include "print.h"
#include "keyboard.h"
#include "int13.h"
#include "util.h"
#include "hdd.h"
#include "floppy.h"

/* ------------------------- buffers and batch state */

static unsigned char batchBuf[BATCH_SECTORS*512]; /* one full hdd track */
static unsigned char headerBuf[512];
static unsigned char batchFill;

/* ------------------------- session state */

static unsigned long diskIndex;
static unsigned long diskStartLBA;
static unsigned short hddCylSaved;
static unsigned char hddHeadSaved;
static unsigned char hddSecSaved;
static unsigned int sectorCount;
static unsigned int payloadCrc;
static unsigned char escFlag;
static unsigned long diskStartTicks;

/* append estimated time until this disk finishes, based on pace so far.
   stays in tick units as long as possible; minutes extracted by
   subtraction (1092 ticks ~= 60 s), seconds by native 16-bit division */
static void printEta(unsigned long elapsedTicks, unsigned int done){
  unsigned long totalT;
  unsigned long remainT;
  unsigned long m;
  unsigned int s;
  if(done == 0){
    return;
  }
  totalT = divLong(mulLong(elapsedTicks, DISK_CAPACITY), done);
  if(totalT <= elapsedTicks){
    return;
  }
  remainT = totalT - elapsedTicks;
  m = 0;
  while(remainT >= 1092){
    remainT -= 1092;
    m++;
  }
  s = (unsigned int)remainT / 18;
  print(" eta ");
  printDecLong(m);
  printChar(':', 1);
  if(s < 10){
    printChar('0', 1);
  }
  printDecLong(s);
}

/* writes batchBuf[0..batchFill) to floppy, updates crc/count/position.
   returns 0 ok, 2 = too many floppy failures, disk should be redone
   on fresh media. the crc covers every sector as buffered here, with
   given-up sectors already replaced by BADFILL */
static unsigned char flushBatch(void){
  unsigned int i;
  for(i = 0; i < batchFill; i++){
    writeFloppyAuto(batchBuf + i * 512, sectorCount);
    payloadCrc = crcBuf(payloadCrc, batchBuf + i * 512, 512);
    sectorCount++;
    advanceCHSFloppy();
    flpLBA++;
    if(badFlpCount >= DISK_BAD_LIMIT){
      batchFill = 0;
      return 2;
    }
  }
  batchFill = 0;
  return 0;
}

static void progressLine(void){
  unsigned long now;
  print("\r\nD:");
  printDecLong(diskIndex);
  print(" LBA:");
  printDecLong(hddLBA);
  print("/");
  printDecLong(hddTotalSectors);
  print(" n:");
  printDecLong(sectorCount);
  print(" bad:");
  printDecLong(diskBadCount);
  now = biosTicks();
  if(now >= diskStartTicks){
    printEta(now - diskStartTicks, sectorCount + batchFill);
  }
}

/* ------------------------- header handling */

static void buildHeader(unsigned char finalFlag){
  unsigned int i;
  unsigned int hc;
  static char* id = "HDDSAVER 2.5";
  for(i = 0; i < 512; i++){
    headerBuf[i] = 0;
  }
  headerBuf[0] = 'H';
  headerBuf[1] = 'D';
  headerBuf[2] = 'S';
  headerBuf[3] = 'V';
  headerBuf[4] = 2;
  headerBuf[5] = finalFlag;
  poke32(headerBuf + 6, diskIndex);
  poke32(headerBuf + 10, diskStartLBA);
  poke16(headerBuf + 14, sectorCount);
  poke16(headerBuf + 16, payloadCrc);
  poke16(headerBuf + 18, diskBadCount);
  poke32(headerBuf + 20, hddTotalSectors);
  poke16(headerBuf + 254, badFlpCount);
  for(i = 0; i < diskBadCount && i < MAX_BAD; i++){
    poke32(headerBuf + 24 + i * 4, badLbasDisk[i]);
  }
  for(i = 0; i < badFlpCount && i < MAX_BAD_FLP; i++){
    poke16(headerBuf + 256 + i * 2, badFlpOffsets[i]);
  }
  for(i = 0; id[i]; i++){
    headerBuf[216 + i] = id[i];
  }
  hc = crcBuf(0xFFFF, headerBuf, 500);
  poke16(headerBuf + 500, hc);
}

/* returns 0 = header written+verified, nonzero = header could not be
   written, disk is useless and must be redone */
static unsigned char writeHeaderSector(void){
  unsigned short cylSave;
  unsigned char headSave;
  unsigned char secSave;
  unsigned char r;
  buildHeader(hddLBA >= hddTotalSectors ? 1 : 0);
  /* header always goes to LBA 0, payload position is saved/restored */
  cylSave = flpCyl;
  headSave = flpHead;
  secSave = flpSec;
  flpCyl = 0;
  flpHead = 0;
  flpSec = 1;
  r = writeFloppyAuto(headerBuf, sectorCount);
  flpCyl = cylSave;
  flpHead = headSave;
  flpSec = secSave;
  return r;
}

/* ------------------------- per disk body */

static void resetForDiskStart(void){
  hddCyl = hddCylSaved;
  hddHead = hddHeadSaved;
  hddSec = hddSecSaved;
  hddLBA = diskStartLBA;
  sectorCount = 0;
  payloadCrc = 0xFFFF;
  diskBadCount = 0;
  badFlpCount = 0;
  batchFill = 0;
  flpCyl = 0;
  flpHead = 0;
  flpSec = HEADER_LBAS + 1;
  flpLBA = HEADER_LBAS;
}

/* copies until end of hdd or ESC. returns 0 = finalized ok,
   2 = restart requested (state was rewound by caller via resetForDiskStart) */
static unsigned char doOneDisk(void){
  unsigned char r;
  while(hddLBA < hddTotalSectors &&
        (sectorCount + batchFill) < DISK_CAPACITY){
    if(escPressed()){
      escFlag = 1;
      break;
    }
    if(headMask & (1 << hddHead)){
      readHddResilient(batchBuf + batchFill * 512);
    }else{
      fillSkipPattern(batchBuf + batchFill * 512);
    }
    advanceCHSHdd();
    hddLBA++;
    batchFill++;
    if(batchFill == BATCH_SECTORS){
      r = flushBatch();
      if(r != 0){
        return r;
      }
      progressLine();
    }
  }
  /* flush whatever is buffered: end of disk, esc stop, or end of drive */
  if(batchFill > 0){
    r = flushBatch();
    if(r != 0){
      return r;
    }
  }
  if(sectorCount > 0 || hddLBA >= hddTotalSectors){
    r = writeHeaderSector();
    if(r != 0){
      return r;
    }
    print("\r\nDisk ");
    printDecLong(diskIndex);
    print(" done: ");
    printDecLong(sectorCount);
    print(" sectors, crc ");
    printHexShort(payloadCrc);
    print(", bad ");
    printDecLong(diskBadCount);
    print("\r\n");
  }
  return 0;
}

/* ------------------------- entry */

void _cstart(void){
  /*shut up linker who cant find _cstart_ that it doesnt need*/
}

#pragma code_seg ( "start_segment" )
void main(void){
  unsigned long startLBA;
  unsigned long rem;
  unsigned long v;
  unsigned int i;
  unsigned int biosCyls;

  crcInit();

  print("\r\n\r\nHDD saver 2.5\r\n");
  print("Dumps ");
  printDecLong(hddTotalSectors);
  print(" hdd sectors (");
  printDecLong(hddTotalSectors >> 11);
  print(" MB) to floppies, ");
  printDecLong(DISK_CAPACITY);
  print(" sectors per disk.\r\n");
  print("Unattended: unreadable hdd sectors are skipped+logged,\r\n");
  print("floppy writes are verified and retried automatically.\r\n");
  print("ESC stops cleanly after the current transfer.\r\n\r\n");

  queryBiosDrive();
  biosCyls = ((biosCylHiSec & 0xC0) << 2) | biosCylLo;
  if(biosCyls || biosHeadMax){
    print("BIOS drive 0x80: ");
    printDecLong(biosCyls + 1);
    print(" cyl, ");
    printDecLong(biosHeadMax + 1);
    print(" heads, ");
    printDecLong(biosCylHiSec & 0x3F);
    print(" spt\r\n");
  }else{
    print("BIOS geometry query failed, using defaults.\r\n");
  }
  print("Geometry must match how the data was written!\r\n");

  v = decInput("Cylinders", hddCyls);
  if(v && v <= 1024){
    hddCyls = (unsigned int)v;
  }
  v = decInput("Heads", hddHeads);
  if(v && v <= 255){
    hddHeads = (unsigned char)v;
  }
  v = decInput("Sectors per track", hddSpt);
  if(v && v <= 63){
    hddSpt = (unsigned char)v;
  }
  v = decInput("Hdd read retries", hddRetries);
  if(v <= 255){
    hddRetries = (unsigned char)v;
  }
  print("Head select: decimal bitmask, bit N = head N.\r\n");
  v = decInput("Head bitmask", 0xFF);
  if(v){
    headMask = (unsigned char)v;
  }
  hddTotalSectors = mulLong(mulLong(hddCyls, hddHeads), hddSpt);
  print("Using: ");
  printDecLong(hddCyls);
  print("/");
  printDecLong(hddHeads);
  print("/");
  printDecLong(hddSpt);
  print(", retries ");
  printDecLong(hddRetries);
  print(", head mask ");
  printHex(headMask >> 4);
  printHex(headMask & 0x0F);
  print(", total ");
  printDecLong(hddTotalSectors);
  print(" sectors (");
  printDecLong(hddTotalSectors >> 11);
  print(" MB).\r\n");

  startLBA = decInput("Resume at hdd LBA", 0);
  diskStartLBA = startLBA;
  rem = 0;
  diskIndex = divByDiskCapacity(startLBA, &rem);
  diskIndex = decInput("Floppy disk number", diskIndex);
  print("\r\nStarting. Everything else runs by itself.\r\n");

  if(startLBA >= hddTotalSectors){
    print("Start LBA beyond end of drive.\r\nPower off.\r\n");
    while(1){
      _asm{ hlt };
    }
  }

  seekToLBA(startLBA);
  allBadCount = 0;
  escFlag = 0;

  while(1){
    hddCylSaved = hddCyl;
    hddHeadSaved = hddHead;
    hddSecSaved = hddSec;
    resetForDiskStart();

    print("=== Disk ");
    printDecLong(diskIndex);
    print(": insert disk and press Enter ===\r\n");
    waitForEnter("");
    diskStartTicks = biosTicks();

    while(doOneDisk() == 2){
      print("This floppy media is failing too often.\r\n");
      print("Label a FRESH disk with number ");
      printDecLong(diskIndex);
      print(", insert it and press Enter ===\r\n");
      waitForEnter("");
      resetForDiskStart();
      diskStartTicks = biosTicks();
    }

    if(hddLBA >= hddTotalSectors){
      print("\r\n=== DUMP COMPLETE ===\r\n");
      print("Disks used: ");
      printDecLong(diskIndex + 1);
      print("\r\nBad hdd sectors this session:\r\n");
      if(allBadCount == 0){
        print("(none)\r\n");
      }
      for(i = 0; i < allBadCount; i++){
        printDecLong(badLbasAll[i]);
        print("\r\n");
      }
      print("Safe to power off.\r\n");
      while(1){
        _asm{ hlt };
      }
    }

    if(escFlag){
      print("\r\nStopped early.\r\nResume next time at LBA ");
      printDecLong(hddLBA);
      if(sectorCount > 0){
        print(", disk number ");
        printDecLong(diskIndex + 1);
        print(" (current disk was finalized and is usable)");
      }else{
        print(", reuse disk number ");
        printDecLong(diskIndex);
        print(" (current disk was left blank)");
      }
      print("\r\nSafe to power off.\r\n");
      while(1){
        _asm{ hlt };
      }
    }

    /* anchor the next disk at the position this one reached;
       resetForDiskStart() rewinds hddLBA here between disks */
    diskStartLBA = hddLBA;

    waitForEnter("Swap in NEXT blank disk, then press Enter");
    diskIndex++;
  }
}
#pragma code_seg ()
