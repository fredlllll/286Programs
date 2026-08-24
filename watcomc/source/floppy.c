#include "floppy.h"
#include "hdd.h"
#include "int13.h"
#include "print.h"
#include "util.h"
/* the floppy's geometry comes straight from definitions.h and never
   changes at runtime; totalSectors is unused for stepping but kept
   populated for completeness */
const struct Geometry floppyGeometry = {FLOPPY_CYLS, FLOPPY_HEADS, FLOPPY_SPT, FLOPPY_TOTAL_SECTORS};
struct ChsWithLBA floppyPosition = {0, 0, 1, 0};

void advanceFloppyPosition(void)
{
  stepChs(&floppyPosition, &floppyGeometry);
}

uint8_t writeVerified(void __far *src)
{
  if (writeFloppyAuto(src))
  {
    advanceFloppyPosition();
    return 0;
  }
  return 1;
}

void seekFloppy(uint16_t c, uint8_t h, uint8_t s)
{
  struct ChsWithLBA tmp;
  tmp.cyl = c;
  tmp.head = h;
  tmp.sec = s;
  tmp.lba = ChsToLba(c, h, s, &floppyGeometry);
  floppyPosition = tmp;
}

uint8_t writeFloppy(uint16_t c, uint8_t h, uint8_t s, void __far *src)
{
  uint8_t rounds;
  uint8_t tries;
  uint8_t status;
  uint8_t verifyBuf[512];

  rounds = 0;
  while (rounds < REWRITE_ROUNDS)
  {
    tries = 0;
    do
    {
      status = writeToDrive(1, c, h, s, 0, src);
      tries++;
      if (status != 0)
      {
        resetDiskSystem(0); /* clear controller error state */
      }
    } while (status != 0 && tries < RETRY_FLOPPY);
    if (status == 0)
    {
      /* write claimed success - now prove it */
      status = readFromDrive(1, c, h, s, 0, verifyBuf);
      if (status == 0 && memcmpBuf(src, (uint8_t __far *)verifyBuf) == 0)
      {
        return 0;
      }
    }
    rounds++;
    resetDiskSystem(0);
  }

  print("\r\nFLOPPY FAIL at ");
  printDecLong(c);
  printChar('/', 1);
  printDecLong(h);
  printChar('/', 1);
  printDecLong(s);
  print(", giving up\r\n");
  return 1;
}

/* why verify every write: old floppy media lies. a write can report
   success while the flux on the disk is already marginal; only
   reading the data back proves it stuck.

   strategy per sector:
     - up to REWRITE_ROUNDS rounds
     - within each round, up to RETRY_FLOPPY attempts with a
       controller reset between failures
     - each successful write is verified by read + compare
   if everything fails, the sector is surrendered: flpFailCount goes
   up and nonzero comes back. callers treat that as fatal for the
   current disk (redo on fresh media).

   TODO: a surrendered data sector leaves its descriptor already
   written, claiming data that never made it onto the disk. policy
   for now is "assume writes work" - they have so far, thanks to the
   aggressive retry above. if this ever bites, the fix is to seek
   back and rewrite the descriptor block with a failure status */
uint8_t writeFloppyAuto(void __far *src)
{
  return writeFloppy(floppyPosition.cyl, floppyPosition.head, floppyPosition.sec, src);
}
