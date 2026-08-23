// HDD saver 2.0 - dumps the whole 60MB MFM/RLL drive to 1.44MB floppies
//
// floppy layout (per disk):
//   LBA 0      : header sector, see HEADER_* defines below
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
// note: no 32 bit division/multiplication anywhere, watcom would emit
// calls to runtime helpers (__U4D etc.) which we dont link against.
// 32 bit add/sub/cmp/constant-shift is fine, it compiles inline.

#define HDD_CYLS 820
#define HDD_HEADS 6
#define HDD_SPT 26
#define HDD_TOTAL_SECTORS ((unsigned long)HDD_CYLS*HDD_HEADS*HDD_SPT)

#define FLPD_CYLS 80
#define FLPD_HEADS 2
#define FLPD_SPT 18
#define FLPD_TOTAL_SECTORS (FLPD_CYLS*FLPD_HEADS*FLPD_SPT)
#define HEADER_LBAS 10
#define DISK_CAPACITY (FLPD_TOTAL_SECTORS-HEADER_LBAS)

#define BATCH_SECTORS 26        /* one full hdd track per ram buffer */
#define RETRY_HDD 16
#define RETRY_FLOPPY 4
#define REWRITE_ROUNDS 5        /* floppy: up to RETRY_FLOPPY*REWRITE_ROUNDS attempts per sector */
#define MAX_BAD 48              /* must match header layout space */
#define MAX_BAD_ALL 64
#define MAX_BAD_FLP 30          /* must match header layout space */
#define DISK_BAD_LIMIT 10       /* this many floppy failures = ask for fresh disk */

/* runtime geometry, initialized from the defines above; the startup
   prompts allow overriding because the bios may translate differently
   (e.g. after the cmos battery died) and reads only line up when we
   use exactly the geometry the data was originally written with */
unsigned int hddCyls = HDD_CYLS;
unsigned char hddHeads = HDD_HEADS;
unsigned char hddSpt = HDD_SPT;
unsigned long hddTotalSectors = HDD_TOTAL_SECTORS;

void printChar (unsigned char inChar, unsigned short pageAndColor);
#pragma aux printChar = \
    "mov ah, 0x0e"   \
    "int 0x10"       \
    modify [ah]      \
    parm   [al][bx]

void print(const char* text){
  char ch;
  while (ch = *text++){
    printChar(ch, 1);
  }
}

static char* hexAlphabet = "0123456789ABCDEF";

void printHex(unsigned char value){
  char ch = hexAlphabet[value >> 4];
  printChar(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  printChar(ch, 1);
}
void printHexShort(unsigned short value){
  printHex(value >> 8);
  printHex(value & 0xFF);
}
void printHexLong(unsigned long value){
  printHexShort(value >> 16);
  printHexShort(value & 0xFFFF);
}

static unsigned long pow10[10] = {1ul,10ul,100ul,1000ul,10000ul,100000ul,
                                  1000000ul,10000000ul,100000000ul,1000000000ul};

void printDecLong(unsigned long v){
  char c;
  unsigned char i;
  unsigned char started;
  started = 0;
  for(i = 9;; i--){
    c = '0';
    while(v >= pow10[i]){
      v -= pow10[i];
      c++;
    }
    if(c > '0' || started || i == 0){
      printChar(c, 1);
      started = 1;
    }
    if(i == 0){
      break;
    }
  }
}

unsigned char getNextKeyPress(){
  volatile unsigned char result;
  _asm{
    mov ah, 0
    int 0x16
    mov result, al
  };
  return result;
}

volatile unsigned char keyFlags;

unsigned char escPressed(){
  /* non blocking: bios AH=01 sets ZF when no key waiting, LAHF grabs it */
  unsigned char k;
  _asm{
    mov ah, 1
    int 0x16
    lahf
    mov keyFlags, ah
  };
  if(keyFlags & 0x40){
    return 0;
  }
  k = getNextKeyPress();
  return (k == 27) ? 1 : 0;
}

void waitForEnter(char* prompt){
  print(prompt);
  while(1){
    if(getNextKeyPress() == 0x0D){
      return;
    }
  }
}

void resetDiskSystem(){
  _asm{
    xor ax,ax
    int 0x13
  };
}

/* INT13 AH=08: what the bios itself thinks the drive looks like.
   ES:DI must be writable, some xt-class bioses copy a parameter table
   there. results are max indices except sectors-per-track */
unsigned char prmTable[16];
volatile unsigned char biosCylLo;
volatile unsigned char biosCylHiSec;
volatile unsigned char biosHeadMax;

void queryBiosDrive(void){
  unsigned short diOff = (unsigned short)(unsigned int)prmTable;
  _asm{
    mov ah, 0x08
    mov dl, 0x80
    mov di, diOff
    push ds
    pop es
    int 0x13
    mov biosCylLo, ch
    mov biosCylHiSec, cl
    mov biosHeadMax, dh
  };
}

unsigned char readFromDrive(unsigned char numSectorsToRead, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* destination){
  volatile unsigned char status;
  unsigned short myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x2
    mov al, numSectorsToRead
    mov cx, myCx
    mov dh, head
    mov dl, driveNumber
    mov bx, destination
    int 0x13
    mov status, ah
  };
  return status;
}

unsigned char writeToDrive(unsigned char numSectorsToWrite, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* source){
  volatile unsigned char status;
  unsigned short myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x3
    mov al, numSectorsToWrite
    mov cx, myCx
    mov dh, head
    mov dl, driveNumber
    mov bx, source
    int 0x13
    mov status, ah
  };
  return status;
}

void printInt13Status(unsigned char status){
  switch(status){
    case 0x00:
      print("no error");
      break;
    case 0x01:
      print("bad command passed to driver");
      break;
    case 0x02:
      print("address mark not found or bad sector");
      break;
    case 0x03:
      print("diskette write protect error");
      break;
    case 0x04:
      print("sector not found");
      break;
    case 0x05:
      print("fixed disk reset failed");
      break;
    case 0x06:
      print("diskette changed or removed");
      break;
    case 0x07:
      print("bad fixed disk parameter table");
      break;
    case 0x08:
      print("DMA overrun");
      break;
    case 0x09:
      print("DMA access across 64k boundary");
      break;
    case 0x0a:
      print("bad fixed disk sector flag");
      break;
    case 0x0b:
      print("bad fixed disk cylinder");
      break;
    case 0x0c:
      print("unsupported track/invalid media");
      break;
    case 0x0d:
      print("invalid number of sectors on fixed disk format");
      break;
    case 0x0e:
      print("fixed disk controller data address mark detected");
      break;
    case 0x0f:
      print("fixed disk DMA arbitration level out of range");
      break;
    case 0x10:
      print("ECC error on disk read, data bad");
      break;
    case 0x11:
      print("data recovered by ECC");
      break;
    case 0x20:
      print("controller error");
      break;
    case 0x40:
      print("seek failure");
      break;
    case 0x80:
      print("time out, drive not ready");
      break;
    case 0xAA:
      print("fixed disk drive not ready");
      break;
    case 0xBB:
      print("fixed disk undefined error");
      break;
    case 0xCC:
      print("fixed disk write fault on selected drive");
      break;
    case 0xE0:
      print("fixed disk status error");
      break;
    case 0xFF:
      print("sense operation failed");
      break;
    default:
      print("unknown error ");
      printHex(status);
  }
}

/* ------------------------- buffers, declared first so they sit low */

static unsigned char batchBuf[BATCH_SECTORS*512];
static unsigned char verifyBuf[512];
static unsigned char headerBuf[512];
static unsigned long badLbasDisk[MAX_BAD];
static unsigned long badLbasAll[MAX_BAD_ALL];
static unsigned int badFlpOffsets[MAX_BAD_FLP];
static unsigned int crcTable[256];

/* ------------------------- state */

static unsigned short hddCyl;
static unsigned char hddHead;
static unsigned char hddSec;
static unsigned long hddLBA;

static unsigned short flpCyl;
static unsigned char flpHead;
static unsigned char flpSec;
static unsigned long flpLBA;

static unsigned long diskIndex;
static unsigned long diskStartLBA;
static unsigned short hddCylSaved;
static unsigned char hddHeadSaved;
static unsigned char hddSecSaved;
static unsigned int sectorCount;
static unsigned int payloadCrc;
static unsigned int diskBadCount;
static unsigned int badFlpCount;
static unsigned int allBadCount;
static unsigned char batchFill;
static unsigned char escFlag;

static char* badFillText = "!BAD-SECTOR!";

/* ------------------------- small helpers */

unsigned char memcmpBuf(unsigned char* a, unsigned char* b){
  unsigned int i;
  for(i = 0; i < 512; i++){
    if(a[i] != b[i]){
      return 1;
    }
  }
  return 0;
}

void fillBadPattern(unsigned char* dest){
  unsigned int i;
  for(i = 0; i < 512; i++){
    dest[i] = badFillText[i % 12];
  }
}

void poke16(unsigned char* p, unsigned int v){
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
}

void poke32(unsigned char* p, unsigned long v){
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}

void crcInit(void){
  unsigned int i;
  unsigned int j;
  unsigned int c;
  for(i = 0; i < 256; i++){
    c = i << 8;
    for(j = 0; j < 8; j++){
      if(c & 0x8000){
        c = (c << 1) ^ 0x1021;
      }else{
        c = c << 1;
      }
    }
    crcTable[i] = c;
  }
}

unsigned int crcBuf(unsigned int crc, unsigned char* p, unsigned int n){
  while(n--){
    crc = (crc << 8) ^ crcTable[((crc >> 8) ^ *p++) & 0xFF];
  }
  return crc;
}

/* decimal input: digits appended with echo, backspace edits,
   enter submits, empty input returns default */
unsigned long decInput(char* prompt, unsigned long defVal){
  char buf[11];
  unsigned char len;
  unsigned char k;
  unsigned char i;
  unsigned long v;
  len = 0;
  print(prompt);
  print(" [");
  printDecLong(defVal);
  print("]: ");
  while(1){
    k = getNextKeyPress();
    if(k == 0x0D){
      if(len == 0){
        print("\r\n");
        return defVal;
      }
      buf[len] = 0;
      v = 0;
      for(i = 0; i < len; i++){
        v = (v << 3) + (v << 1) + (buf[i] - '0');
      }
      print("\r\n");
      return v;
    }
    if(k == 0x08){
      if(len){
        len--;
        printChar(0x08, 1);
        printChar(' ', 1);
        printChar(0x08, 1);
      }
    }
    if(k >= '0' && k <= '9'){
      if(len < 10){
        buf[len++] = k;
        printChar(k, 1);
      }
    }
  }
}

unsigned long divByDiskCapacity(unsigned long v, unsigned long* remainder){
  unsigned long n;
  n = 0;
  while(v >= DISK_CAPACITY){
    v -= DISK_CAPACITY;
    n++;
  }
  *remainder = v;
  return n;
}

/* 32x16 bit multiply without the watcom runtime helper (__U4M):
   plain shift-add, called once at startup so speed is irrelevant */
unsigned long mulLong(unsigned long a, unsigned int b){
  unsigned long r;
  r = 0;
  while(b){
    if(b & 1){
      r += a;
    }
    a += a;
    b >>= 1;
  }
  return r;
}

unsigned char advanceCHSHdd(void){
  hddSec += 1;
  if(hddSec > hddSpt){
    hddSec = 1;
    hddHead += 1;
  }
  if(hddHead >= hddHeads){
    hddHead = 0;
    hddCyl += 1;
  }
  return hddCyl < hddCyls;
}

void advanceCHSFloppy(void){
  flpSec += 1;
  if(flpSec > FLPD_SPT){
    flpSec = 1;
    flpHead += 1;
  }
  if(flpHead >= FLPD_HEADS){
    flpHead = 0;
    flpCyl += 1;
  }
}

/* position hdd chs at lba without any 32 bit division:
   just step forward from 0/0/1, cheap enough for these drive sizes */
void seekToLBA(unsigned long target){
  hddCyl = 0;
  hddHead = 0;
  hddSec = 1;
  while(target){
    advanceCHSHdd();
    target--;
  }
}

/* ------------------------- hdd reading */

/* retries RETRY_HDD times, then fills dest with BADFILL and logs the LBA.
   never prompts, copying always continues */
void readHddResilient(unsigned char* dest){
  unsigned char tries;
  unsigned char status;

  status = readFromDrive(1, hddCyl, hddHead, hddSec, 0x80, dest);
  tries = 0;
  while(status != 0 && status != 0x11 && tries < RETRY_HDD){
    resetDiskSystem();
    status = readFromDrive(1, hddCyl, hddHead, hddSec, 0x80, dest);
    tries++;
  }
  if(status == 0 || status == 0x11){
    /* 0x11 = recoverable ECC error, bios already corrected the data */
    return;
  }

  print("\r\nHDD read fail CHS ");
  printDecLong(hddCyl);
  printChar('/', 1);
  printDecLong(hddHead);
  printChar('/', 1);
  printDecLong(hddSec);
  print(" LBA ");
  printDecLong(hddLBA);
  print(": ");
  printInt13Status(status);
  print(", skipping\r\n");

  if(diskBadCount < MAX_BAD){
    badLbasDisk[diskBadCount++] = hddLBA;
  }
  if(allBadCount < MAX_BAD_ALL){
    badLbasAll[allBadCount++] = hddLBA;
  }
  fillBadPattern(dest);
}

/* ------------------------- floppy writing */

/* best effort write with read-back verify, no prompting: up to
   RETRY_FLOPPY*REWRITE_ROUNDS attempts, then gives up on this sector:
   buffer is replaced with BADFILL, payload offset logged, copying continues.
   returns 0 = written+verified, 1 = gave up */
unsigned char writeFloppyAuto(unsigned char* src){
  unsigned char rounds;
  unsigned char tries;
  unsigned char status;
  unsigned int off;

  rounds = 0;
  while(rounds < REWRITE_ROUNDS){
    tries = 0;
    status = writeToDrive(1, flpCyl, flpHead, flpSec, 0, src);
    while(status != 0 && tries < RETRY_FLOPPY){
      resetDiskSystem();
      status = writeToDrive(1, flpCyl, flpHead, flpSec, 0, src);
      tries++;
    }
    if(status == 0){
      status = readFromDrive(1, flpCyl, flpHead, flpSec, 0, verifyBuf);
      if(status == 0 && memcmpBuf(src, verifyBuf) == 0){
        return 0;
      }
    }
    rounds++;
    resetDiskSystem();
  }

  off = sectorCount;
  if(badFlpCount < MAX_BAD_FLP){
    badFlpOffsets[badFlpCount++] = off;
  }
  fillBadPattern(src);
  print("\r\nFLOPPY FAIL off ");
  printDecLong(off);
  print(" (hdd LBA ");
  printDecLong(hddLBA);
  print("), marked bad\r\n");
  return 1;
}

/* writes batchBuf[0..batchFill) to floppy, updates crc/count/position.
   returns 0 ok, 2 = too many floppy failures, disk should be redone
   on fresh media. the crc covers every sector as buffered here, with
   given-up sectors already replaced by BADFILL */
unsigned char flushBatch(void){
  unsigned int i;
  for(i = 0; i < batchFill; i++){
    writeFloppyAuto(batchBuf + i * 512);
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

void progressLine(void){
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
}

/* ------------------------- header handling */

void buildHeader(unsigned char finalFlag){
  unsigned int i;
  unsigned int hc;
  static char* id = "HDDSAVER 2.2";
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
unsigned char writeHeaderSector(void){
  unsigned short cylSave;
  unsigned char headSave;
  unsigned char secSave;
  unsigned char r;
  buildHeader(hddLBA >= HDD_TOTAL_SECTORS ? 1 : 0);
  /* header always goes to LBA 0, payload position is saved/restored */
  cylSave = flpCyl;
  headSave = flpHead;
  secSave = flpSec;
  flpCyl = 0;
  flpHead = 0;
  flpSec = 1;
  r = writeFloppyAuto(headerBuf);
  flpCyl = cylSave;
  flpHead = headSave;
  flpSec = secSave;
  return r;
}

/* ------------------------- per disk body */

void resetForDiskStart(void){
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
unsigned char doOneDisk(void){
  unsigned char r;
  while(hddLBA < hddTotalSectors &&
        (sectorCount + batchFill) < DISK_CAPACITY){
    if(escPressed()){
      escFlag = 1;
      break;
    }
    readHddResilient(batchBuf + batchFill * 512);
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

  print("\r\n\r\nHDD saver 2.2\r\n");
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
  hddTotalSectors = mulLong(mulLong(hddCyls, hddHeads), hddSpt);
  print("Using: ");
  printDecLong(hddCyls);
  print("/");
  printDecLong(hddHeads);
  print("/");
  printDecLong(hddSpt);
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

  if(startLBA >= HDD_TOTAL_SECTORS){
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

    while(doOneDisk() == 2){
      print("This floppy media is failing too often.\r\n");
      print("Label a FRESH disk with number ");
      printDecLong(diskIndex);
      print(", insert it and press Enter ===\r\n");
      waitForEnter("");
      resetForDiskStart();
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

    waitForEnter("Swap in NEXT blank disk, then press Enter");
    diskIndex++;
  }
}
#pragma code_seg ()
