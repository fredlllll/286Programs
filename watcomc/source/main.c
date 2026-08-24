// HDD saver 3.0 - dumps the whole 60MB MFM/RLL drive to 1.44MB floppies
//
// this file is the application logic: session state, batching, the
// per-disk flow and the startup prompts. everything below it is
// plumbing that lives in its own module:
//
//   int13.c    raw bios disk calls (read/write/reset/query),
//              chs addressing types + stepping helper
//   hdd.c      hdd geometry, position tracking, resilient reads
//   floppy.c   position tracking, verified writes, format structs,
//              descriptor block builder
//   print.c    console output
//   keyboard.c key input
//   math.c     32 bit mul/div without runtime helpers
//   util.c     crc, byte poking, fill patterns, tick counter
//   definitions.h all tunable constants
//
// floppy layout (per disk), version 3:
//   LBA 0      : header sector, see struct FloppyHeader in floppy.h
//   LBA 1..9   : reserved (zeros)
//   LBA 10..2879: repeating groups, one group per hdd track batch:
//                  [descriptor block: 1 sector, struct DescBlock]
//                  [the data sectors its entries refer to]
//
// every dumped hdd sector carries its own identification in the
// descriptor: lba (3 bytes), the int13 error/status code of the read
// attempt (1 byte) and the index of its 512-byte data slot within the
// group (1 byte). statuses 0/0x11 mean "good data follows"; anything
// else means the sector could not be read (or the head was masked
// out) and NO data was written for it - it costs 5 bytes instead of
// 512. consequences:
//   - disks are self-describing, the restorer scatters by lba, so
//     disk order no longer matters and gaps between lbas are legal
//   - diskIndex survives only as a human-friendly label
//   - the restorer sees the exact bios error for every hole
//
// fully unattended operation:
// - unreadable hdd sectors: retried RETRY_HDD times, then logged as
//   a descriptor carrying the error code; copying continues
// - floppy writes: verified by reading back. a surrendered write is
//   treated as fatal for the current disk: it gets redone on fresh
//   media (see TODO in floppy.c)
//
// note: no 32 bit division/multiplication anywhere, watcom would emit
// calls to runtime helpers (__U4D etc.) which we dont link against.
// 32 bit add/sub/cmp/constant-shift is fine, it compiles inline.
// see math.c for the shift-add helpers that stand in for them

#include "definitions.h"
#include "math.h"
#include "print.h"
#include "keyboard.h"
#include "int13.h"
#include "util.h"
#include "hdd.h"
#include "floppy.h"

/* ------------------------- buffers and batch state */

/* sectors travel hdd -> batchBuf -> floppy in whole tracks, which
   keeps bios transfer counts high (fast) while still giving every
   sector individual retry handling at write time. one track is also
   exactly one descriptor group, so no extra buffering is needed to
   get the descriptor block physically ahead of its data */
static unsigned char batchBuf[BATCH_SECTORS*512]; /* one full hdd track */
static unsigned char headerBuf[512];
static unsigned char descBuf[512];   /* staging for struct DescBlock */
static unsigned char batchStat[BATCH_SECTORS]; /* read status per batch sector */
static unsigned char batchFill;      /* descriptors currently in the batch */
static unsigned char batchGood;      /* data slots used in batchBuf */

/* ------------------------- session state
   everything here describes progress across the entire dump, not
   just one sector or one disk */

static unsigned long diskIndex;      /* current floppy number (label) */
static unsigned long diskStartLBA;   /* first hdd lba attempted on this disk */
static struct Chs hddPosSaved;       /* position snapshot taken before */
                                     /* resetForDiskStart() rewinds; */
                                     /* lets a failed disk be redone */
static unsigned int descCountDisk;   /* descriptors written this disk */
static unsigned int dataCountDisk;   /* data sectors written this disk */
static unsigned char escFlag;        /* set when user pressed esc */
static unsigned long diskStartTicks; /* timer value when this disk began */

/* append estimated time until this disk finishes, based on pace so far.
   done = hdd sectors attempted on this disk. stays in tick units as
   long as possible; minutes extracted by subtraction (1092 ticks ~=
   60 s), seconds by native 16-bit division */
static void printEta(unsigned long elapsedTicks, unsigned int done){
  unsigned long totalT;
  unsigned long remainT;
  unsigned long m;
  unsigned int s;
  if(done == 0){
    return;
  }
  /* rule of three: elapsed/done == total/capacity -> extrapolate onto
     the rough per-disk capacity. both multiplies go through mulLong
     because * would need the missing runtime helper */
  totalT = divLong(mulLong(elapsedTicks, APPROX_DISK_CAPACITY), done);
  if(totalT <= elapsedTicks){
    return;                    /* estimate not ahead: show nothing */
  }
  remainT = totalT - elapsedTicks;
  m = 0;
  while(remainT >= 1092){      /* 1092 ticks ~ 60 seconds */
    remainT -= 1092;
    m++;
  }
  s = (unsigned int)remainT / 18;   /* leftover ticks -> seconds */
  print(" eta ");
  printDecLong(m);
  printChar(':', 1);
  if(s < 10){
    printChar('0', 1);
  }
  printDecLong(s);
}

/* writes one 512 byte sector at the current floppy position and, on
   success, steps the position forward. every caller goes through here
   so position tracking cannot drift from what was actually written.
   returns 0 ok, nonzero = write surrendered */
static unsigned char writeVerified(unsigned char* src){
  if(writeFloppyAuto(src) != 0){
    return 1;
  }
  advanceCHSFloppy();
  flpLBA++;
  return 0;
}

/* room on the current disk for one more full group? a group costs
   its descriptor block plus up to BATCH_SECTORS data sectors. when
   a full group no longer fits the disk is declared full - up to
   BATCH_SECTORS slots may sit unused as a result, that is the price
   for never splitting a batch across disks */
static unsigned char floppyFitsGroup(void){
  return (FLPD_TOTAL_SECTORS - flpLBA) >= (BATCH_SECTORS + 1);
}

/* turns the buffered track into one descriptor group: block first,
   then the data of every sector whose read produced trustworthy
   content. skipped/failed sectors contribute only their descriptor,
   no data slot.

   returns 0 ok, 2 = a floppy write surrendered; the whole disk gets
   redone on fresh media because the descriptors already written can
   no longer be trusted to match new data */
static unsigned char flushBatch(void){
  unsigned int i;
  unsigned int slot;
  struct DescBlock* db = (struct DescBlock*)descBuf;

  /* all batch sectors are consecutive hdd lbas ending at hddLBA-1 */
  fillDescBlock(db, hddLBA - batchFill, batchStat, batchFill);
  if(writeVerified(descBuf) != 0){
    return 2;                /* without its block the group is garbage */
  }
  slot = 0;
  for(i = 0; i < batchFill; i++){
    if(SECTOR_HAS_DATA(batchStat[i])){
      if(writeVerified(batchBuf + slot * 512) != 0){
        return 2;            /* see TODO in floppy.c */
      }
      slot++;
    }
  }
  descCountDisk += batchFill;
  dataCountDisk += slot;
  batchFill = 0;
  batchGood = 0;
  return 0;
}

/* one status line per flushed group: disk label, absolute hdd
   position, descriptor/data counts, floppy failures and a rolling
   time estimate */
static void progressLine(void){
  unsigned long now;
  print("\r\nD:");
  printDecLong(diskIndex);
  print(" LBA:");
  printDecLong(hddLBA);
  print("/");
  printDecLong(hddGeom.totalSectors);
  print(" d:");
  printDecLong((unsigned long)(descCountDisk + batchFill));
  print(" n:");
  printDecLong((unsigned long)dataCountDisk);
  print(" fail:");
  printDecLong((unsigned long)flpFailCount);
  now = biosTicks();
  if(now >= diskStartTicks){
    printEta(now - diskStartTicks,
             (unsigned int)(hddLBA - diskStartLBA));
  }
}

/* ------------------------- header handling */

/* fills headerBuf with the identity sector for the current disk.
   written to lba 0 of each floppy so the pc side knows what it got.
   deliberately thin: which lbas made it onto this disk and why any
   others did not is recorded per sector in the descriptor blocks -
   repeating it here would just be a second copy to keep in sync.

   the FloppyHeader struct is mapped straight over headerBuf, so its
   members land at their documented on-disk offsets (see floppy.h).
   multi byte values are stored little endian implicitly because x86
   is little endian */
static void buildHeader(unsigned char finalFlag){
  unsigned int i;
  struct FloppyHeader* h = (struct FloppyHeader*)headerBuf;
  static char* id = "HDDSAVER 3.0";
  for(i = 0; i < 512; i++){
    headerBuf[i] = 0;
  }
  h->magic[0] = 'H';
  h->magic[1] = 'D';
  h->magic[2] = 'S';
  h->magic[3] = 'V';           /* magic 'HDSV' identifies our disks */
  h->version = 3;              /* format version */
  h->flags = finalFlag;        /* bit0 = last disk of the dump */
  h->diskIndex = diskIndex;    /* label only, restore order is free */
  h->hddTotalSectors = hddGeom.totalSectors;
  h->descCount = (unsigned short)descCountDisk;
  h->dataCount = (unsigned short)dataCountDisk;
  h->flpFailCount = (unsigned short)flpFailCount;
  for(i = 0; id[i]; i++){
    h->toolId[i] = id[i];
  }
  /* checksum over the header itself (excluding the checksum slot),
     protects against a corrupted header lying about everything else */
  h->crc = crcBuf(0xFFFF, headerBuf, 510);
}

/* returns 0 = header written+verified, nonzero = header could not be
   written, disk is useless and must be redone.
   without a valid header the pc side cannot tell what is on the
   disk, so a failed header write dooms the disk even if the payload
   is perfect */
static unsigned char writeHeaderSector(void){
  struct Chs save;
  unsigned char r;
  buildHeader(hddLBA >= hddGeom.totalSectors ? 1 : 0);
  /* header always goes to LBA 0, payload position is saved/restored
     but not advanced: the header sits outside the group stream.
     assigning the whole struct copies all three fields at once */
  save = flpPos;
  flpPos.cyl = 0;
  flpPos.head = 0;
  flpPos.sec = 1;
  r = writeFloppyAuto(headerBuf);
  flpPos = save;
  return r;
}

/* ------------------------- per disk body */

/* rewinds everything to the start of "current" disk. called when a
   fresh disk is inserted and again after a media-failure retry -
   hence the position snapshot main() takes before each attempt */
static void resetForDiskStart(void){
  hddPos = hddPosSaved;        /* back to the snapshotted hdd spot... */
  hddLBA = diskStartLBA;
  descCountDisk = 0;
  dataCountDisk = 0;
  flpFailCount = 0;
  batchFill = 0;
  batchGood = 0;
  flpPos.cyl = 0;              /* groups start right after the */
  flpPos.head = 0;             /* reserved header area at lba 10 */
  flpPos.sec = HEADER_LBAS + 1;
  flpLBA = HEADER_LBAS;
}

/* copies until end of hdd or ESC or full floppy. returns 0 =
   finalized ok, 2 = restart requested (state was rewound by caller
   via resetForDiskStart) */
static unsigned char doOneDisk(void){
  unsigned char r;
  unsigned char st;
  while(hddLBA < hddGeom.totalSectors){
    if(escPressed()){
      escFlag = 1;
      break;
    }
    if(batchFill == 0 && !floppyFitsGroup()){
      break;                   /* no room for another full group */
    }
    if(headMask & (1 << hddPos.head)){
      /* read straight into the next FREE data slot: failed sectors
         do not consume one, so good data stays packed and the
         descriptor indices come out in encounter order */
      st = readHddResilient(batchBuf + batchGood * 512);
    }else{
      /* head deselected via bitmask: record it as such, dump
         nothing. a later pass with that head enabled can fill the
         hole - the restorer simply leaves this lba uncovered */
      st = SECTOR_STATUS_HEADSKIPPED;
    }
    batchStat[batchFill] = st;
    if(SECTOR_HAS_DATA(st)){
      batchGood++;
    }
    advanceCHSHdd();
    hddLBA++;
    batchFill++;
    if(batchFill == BATCH_SECTORS){
      r = flushBatch();        /* buffer full: write the group out */
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
  if(descCountDisk > 0 || hddLBA >= hddGeom.totalSectors){
    r = writeHeaderSector();
    if(r != 0){
      return r;
    }
    print("\r\nDisk ");
    printDecLong(diskIndex);
    print(" done: ");
    printDecLong((unsigned long)descCountDisk);
    print(" descriptors, ");
    printDecLong((unsigned long)dataCountDisk);
    print(" data sectors, fail ");
    printDecLong((unsigned long)flpFailCount);
    print("\r\n");
  }
  return 0;
}

/* ------------------------- entry */

void _cstart(void){
  /*shut up linker who cant find _cstart_ that it doesnt need*/
}

/* main() runs the interactive part once (bios query, geometry
   prompts), then loops over floppies forever until the drive is
   fully dumped or the user presses esc. it never returns; finished
   states halt the cpu */
#pragma code_seg ( "start_segment" )
void main(void){
  unsigned long startLBA;
  unsigned long rem;
  unsigned long v;
  unsigned int i;
  unsigned int biosCyls;

  crcInit();

  print("\r\n\r\nHDD saver 3.0\r\n");
  print("Dumps ");
  printDecLong(hddGeom.totalSectors);
  print(" hdd sectors (");
  printDecLong(hddGeom.totalSectors >> 11);  /* >>11 = /2048 sectors = MB */
  print(" MB) to floppies, up to ");
  printDecLong(APPROX_DISK_CAPACITY);
  print(" hdd sectors per disk.\r\n");
  print("Every sector is stored with its lba+status, so unreadable\r\n");
  print("sectors are just logged (with the drive's error code) and\r\n");
  print("cost no floppy space. Disk order does not matter.\r\n");
  print("Floppy writes are verified and retried automatically.\r\n");
  print("ESC stops cleanly after the current transfer.\r\n\r\n");

  /* ask the bios what drive 0x80 looks like and show it, purely
     informational: the defaults usually match but a translated
     bios will report different numbers than the drive really has */
  queryBiosDrive();
  biosCyls = ((biosCylHiSec & 0xC0) << 2) | biosCylLo;  /* unpack cyl */
  if(biosCyls || biosHeadMax){
    print("BIOS drive 0x80: ");
    printDecLong(biosCyls + 1);          /* bios reports max indices */
    print(" cyl, ");
    printDecLong(biosHeadMax + 1);
    print(" heads, ");
    printDecLong(biosCylHiSec & 0x3F);   /* cl bits 0..5 = spt */
    print(" spt\r\n");
  }else{
    print("BIOS geometry query failed, using defaults.\r\n");
  }
  print("Geometry must match how the data was written!\r\n");

  /* every prompt shows the compiled-in default in brackets; typing
     nothing (just enter) accepts it */
  v = decInput("Cylinders", hddGeom.cyls);
  if(v && v <= 1024){
    hddGeom.cyls = (unsigned int)v;
  }
  v = decInput("Heads", hddGeom.heads);
  if(v && v <= 255){
    hddGeom.heads = (unsigned char)v;
  }
  v = decInput("Sectors per track", hddGeom.spt);
  if(v && v <= 63){
    hddGeom.spt = (unsigned char)v;
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
  hddGeom.totalSectors = mulLong(mulLong(hddGeom.cyls, hddGeom.heads), hddGeom.spt);
  print("Using: ");
  printDecLong(hddGeom.cyls);
  print("/");
  printDecLong(hddGeom.heads);
  print("/");
  printDecLong(hddGeom.spt);
  print(", retries ");
  printDecLong(hddRetries);
  print(", head mask ");
  printHex(headMask >> 4);
  printHex(headMask & 0x0F);
  print(", total ");
  printDecLong(hddGeom.totalSectors);
  print(" sectors (");
  printDecLong(hddGeom.totalSectors >> 11);
  print(" MB).\r\n");

  /* resuming: the lba where the previous run stopped becomes the
     starting point; its disk number is derived from how many full
     disks worth of sectors fit before it */
  startLBA = decInput("Resume at hdd LBA", 0);
  diskStartLBA = startLBA;
  rem = 0;
  diskIndex = divByDiskCapacity(startLBA, &rem);
  diskIndex = decInput("Floppy disk number", diskIndex);
  print("\r\nStarting. Everything else runs by itself.\r\n");

  if(startLBA >= hddGeom.totalSectors){
    print("Start LBA beyond end of drive.\r\nPower off.\r\n");
    while(1){
      _asm{ hlt };             /* hlt stops the cpu until interrupt */
    }
  }

  seekToLBA(startLBA);
  allBadCount = 0;
  escFlag = 0;

  while(1){
    /* remember where we are on the hdd: if this floppy fails late,
       resetForDiskStart() can rewind to exactly this spot and the
       whole content gets re-dumped onto fresh media. one assignment
       copies the whole cylinder/head/sector triple */
    hddPosSaved = hddPos;
    resetForDiskStart();

    print("=== Disk ");
    printDecLong(diskIndex);
    print(": insert disk and press Enter ===\r\n");
    waitForEnter("");
    diskStartTicks = biosTicks();

    /* return code 2 = media gave up too often; loop asks for a
       fresh disk and redoes the identical range */
    while(doOneDisk() == 2){
      print("This floppy media is failing too often.\r\n");
      print("Label a FRESH disk with number ");
      printDecLong(diskIndex);
      print(", insert it and press Enter ===\r\n");
      waitForEnter("");
      resetForDiskStart();
      diskStartTicks = biosTicks();
    }

    if(hddLBA >= hddGeom.totalSectors){
      /* every sector of the drive has been through the pipeline */
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
      /* clean stop: report where to resume next run. if some data
         had already been written, the current disk was finalized
         with a valid header and is usable; otherwise it should be
         overwritten by reusing the same disk number */
      print("\r\nStopped early.\r\nResume next time at LBA ");
      printDecLong(hddLBA);
      if(descCountDisk > 0){
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
