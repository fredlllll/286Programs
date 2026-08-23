#include "floppy.h"
#include "hdd.h"
#include "int13.h"
#include "print.h"
#include "util.h"

unsigned short flpCyl;
unsigned char flpHead;
unsigned char flpSec;
unsigned long flpLBA;

unsigned int badFlpOffsets[MAX_BAD_FLP];
unsigned int badFlpCount;

/* scratch space for read-back verification of written sectors */
static unsigned char verifyBuf[512];

void advanceCHSFloppy(void){
  flpSec += 1;
  if(flpSec > FLPD_SPT){      /* past last sector -> next head */
    flpSec = 1;
    flpHead += 1;
  }
  if(flpHead >= FLPD_HEADS){  /* past last head -> next cylinder */
    flpHead = 0;
    flpCyl += 1;
  }
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
    status = writeToDrive(1, flpCyl, flpHead, flpSec, 0, src);
    while(status != 0 && tries < RETRY_FLOPPY){
      resetDiskSystem();       /* clear controller error state */
      status = writeToDrive(1, flpCyl, flpHead, flpSec, 0, src);
      tries++;
    }
    if(status == 0){
      /* write claimed success - now prove it */
      status = readFromDrive(1, flpCyl, flpHead, flpSec, 0, verifyBuf);
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
