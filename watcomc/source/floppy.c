#include "floppy.h"
#include "hdd.h"
#include "int13.h"
#include "print.h"
#include "util.h"

struct Chs flpPos = {0, 0, 1};
unsigned long flpLBA;

unsigned int badFlpOffsets[MAX_BAD_FLP];
unsigned int badFlpCount;

/* the floppy's geometry comes straight from definitions.h and never
   changes at runtime; totalSectors is unused for stepping but kept
   populated for completeness */
static const struct Geometry flpGeom = {
  FLPD_CYLS, FLPD_HEADS, FLPD_SPT, FLPD_TOTAL_SECTORS };

/* scratch space for read-back verification of written sectors */
static unsigned char verifyBuf[512];

void advanceCHSFloppy(void){
  stepChs(&flpPos, &flpGeom);
}

/* why verify every write: old floppy media lies. a write can report
   success while the flux on the disk is already marginal; only
   reading the data back proves it stuck.

   strategy per sector:
     - up to REWRITE_ROUNDS rounds
     - within each round, up to RETRY_FLOPPY attempts with a
       controller reset between failures
     - each successful write is verified by read + compare
   if everything fails, the sector is surrendered: src is overwritten
   with BADFILL so the crc computed later covers what actually ends
   up readable, and the offset is logged into the header */
unsigned char writeFloppyAuto(unsigned char* src, unsigned int payloadOffset){
  unsigned char rounds;
  unsigned char tries;
  unsigned char status;

  rounds = 0;
  while(rounds < REWRITE_ROUNDS){
    tries = 0;
    status = writeToDrive(1, flpPos.cyl, flpPos.head, flpPos.sec, 0, src);
    while(status != 0 && tries < RETRY_FLOPPY){
      resetDiskSystem();       /* clear controller error state */
      status = writeToDrive(1, flpPos.cyl, flpPos.head, flpPos.sec, 0, src);
      tries++;
    }
    if(status == 0){
      /* write claimed success - now prove it */
      status = readFromDrive(1, flpPos.cyl, flpPos.head, flpPos.sec, 0, verifyBuf);
      if(status == 0 && memcmpBuf(src, verifyBuf) == 0){
        return 0;
      }
    }
    rounds++;
    resetDiskSystem();
  }

  if(badFlpCount < MAX_BAD_FLP){
    badFlpOffsets[badFlpCount++] = payloadOffset;
  }
  fillBadPattern(src);
  print("\r\nFLOPPY FAIL off ");
  printDecLong(payloadOffset);
  print(" (hdd LBA ");
  printDecLong(hddLBA);
  print("), marked bad\r\n");
  return 1;
}
