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

static unsigned char verifyBuf[512];

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

/* best effort write with read-back verify, no prompting: up to
   RETRY_FLOPPY*REWRITE_ROUNDS attempts, then gives up on this sector:
   buffer is replaced with BADFILL, payload offset logged, copying continues */
unsigned char writeFloppyAuto(unsigned char* src, unsigned int payloadOffset){
  unsigned char rounds;
  unsigned char tries;
  unsigned char status;

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
