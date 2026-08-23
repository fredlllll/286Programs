#include "hdd.h"
#include "int13.h"
#include "print.h"
#include "util.h"

/* runtime geometry, initialized from the defines in definitions.h; the
   startup prompts allow overriding because the bios may translate
   differently (e.g. after the cmos battery died) and reads only line
   up when we use exactly the geometry the data was originally written
   with */
unsigned int hddCyls = HDD_CYLS;
unsigned char hddHeads = HDD_HEADS;
unsigned char hddSpt = HDD_SPT;
unsigned long hddTotalSectors = HDD_TOTAL_SECTORS;
unsigned char hddRetries = RETRY_HDD;
unsigned char headMask = 0xFF;

unsigned short hddCyl;
unsigned char hddHead;
unsigned char hddSec;
unsigned long hddLBA;

unsigned long badLbasDisk[MAX_BAD];
unsigned int diskBadCount;
unsigned long badLbasAll[MAX_BAD_ALL];
unsigned int allBadCount;

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

/* reads one hdd sector. retries up to hddRetries times (configurable,
   0 = single attempt). resets only kick in when retrying is enabled:
   a bios disk reset recalibrates the drive (loud seek to cylinder 0
   and back), which we avoid on a drive with weak heads */
void readHddResilient(unsigned char* dest){
  unsigned char tries;
  unsigned char status;

  status = readFromDrive(1, hddCyl, hddHead, hddSec, 0x80, dest);
  tries = 0;
  while(status != 0 && status != 0x11 && tries < hddRetries){
    if(hddRetries >= 2 && tries == hddRetries / 2){
      resetDiskSystem();
    }
    status = readFromDrive(1, hddCyl, hddHead, hddSec, 0x80, dest);
    tries++;
  }
  if(status != 0 && status != 0x11 && hddRetries > 0){
    resetDiskSystem();
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
