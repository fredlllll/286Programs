#include "hdd.h"
#include "int13.h"
#include "print.h"

/* runtime geometry, initialized from the defines in definitions.h; the
   startup prompts allow overriding because the bios may translate
   differently (e.g. after the cmos battery died) and reads only line
   up when we use exactly the geometry the data was originally written
   with */
struct Geometry hddGeom = { HDD_CYLS, HDD_HEADS, HDD_SPT, HDD_TOTAL_SECTORS };
unsigned char hddRetries = RETRY_HDD;
unsigned char headMask = 0xFF;

struct Chs hddPos = {0, 0, 1};
unsigned long hddLBA;

/* lbas of sectors that could not be read, whole session, printed in
   the end-of-run summary. the per-disk record of the same information
   is the sector descriptor stream on the floppies themselves */
unsigned long badLbasAll[MAX_BAD_ALL];
unsigned int allBadCount;

void advanceCHSHdd(void){
  stepChs(&hddPos, &hddGeom);
}

/* position hdd chs at lba without any 32 bit division:
   just step forward from 0/0/1, cheap enough for these drive sizes */
void seekToLBA(unsigned long target){
  hddPos.cyl = 0;
  hddPos.head = 0;
  hddPos.sec = 1;
  while(target){
    advanceCHSHdd();
    target--;
  }
}

/* reads one hdd sector. retries up to hddRetries times (configurable,
   0 = single attempt). resets only kick in when retrying is enabled:
   a bios disk reset recalibrates the drive (loud seek to cylinder 0
   and back), which we avoid on a drive with weak heads.

   returns the final bios status so the caller can store it in the
   sector descriptor: 0 = clean read, 0x11 = "ecc corrected it, data
   is fine" (both count as good, data gets dumped), anything else =
   given up after every attempt. a dead sector does NOT get dumped:
   its lba and the error code land in the next descriptor block and
   the dump carries on - one bad sector costs 5 bytes, not 512 */
unsigned char readHddResilient(unsigned char* dest){
  unsigned char tries;
  unsigned char status;

  status = readFromDrive(1, hddPos.cyl, hddPos.head, hddPos.sec, 0x80, dest);
  tries = 0;
  while(status != 0 && status != 0x11 && tries < hddRetries){
    if(hddRetries >= 2 && tries == hddRetries / 2){
      resetDiskSystem();     /* halfway through, try with a reset */
    }
    status = readFromDrive(1, hddPos.cyl, hddPos.head, hddPos.sec, 0x80, dest);
    tries++;
  }
  if(status != 0 && status != 0x11 && hddRetries > 0){
    resetDiskSystem();       /* clean up controller state for next sector */
  }
  if(status == 0 || status == 0x11){
    /* 0x11 = recoverable ECC error, bios already corrected the data.
       the distinction is preserved: the restorer may want to know
       which sectors only survived via ecc correction */
    return status;
  }

  print("\r\nHDD read fail CHS ");
  printDecLong(hddPos.cyl);
  printChar('/', 1);
  printDecLong(hddPos.head);
  printChar('/', 1);
  printDecLong(hddPos.sec);
  print(" LBA ");
  printDecLong(hddLBA);
  print(": ");
  printInt13Status(status);
  print(", logging in descriptor\r\n");

  /* session-wide log for the end-of-run summary. the per-disk
     record is the sector descriptor itself now */
  if(allBadCount < MAX_BAD_ALL){
    badLbasAll[allBadCount++] = hddLBA;
  }
  return status;
}
