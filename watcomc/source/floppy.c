#include "floppy.h"
#include "hdd.h"
#include "int13.h"
#include "print.h"
#include "util.h"
/* the floppy's geometry comes straight from definitions.h and never
   changes at runtime; totalSectors is unused for stepping but kept
   populated for completeness */
const struct Geometry floppyGeometry = {
    FLOPPY_CYLS, FLOPPY_HEADS, FLOPPY_SPT, FLOPPY_TOTAL_SECTORS};
struct ChsWithLBA floppyPosition = {0, 0, 1, 0};

/* scratch space for read-back verification of written sectors */
static unsigned char verifyBuf[512];

void advanceFloppyPosition(void)
{
  stepChs(&floppyPosition, &floppyGeometry);
}

void seekFloppy(unsigned int c, unsigned char h, unsigned char s)
{
  struct ChsWithLBA tmp = {c, h, s, ChsToLba(c, h, s)};
  floppyPosition = tmp;
}

uint8_t writeFloppy(uint16_t c, uint8_t h, uint8_t s, uint8_t *src)
{
  unsigned char rounds;
  unsigned char tries;
  unsigned char status;

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
        resetDiskSystem(); /* clear controller error state */
      }
    } while (status != 0 && tries < RETRY_FLOPPY);
    if (status == 0)
    {
      /* write claimed success - now prove it */
      status = readFromDrive(1, c, h, s, 0, verifyBuf);
      if (status == 0 && memcmpBuf(src, verifyBuf) == 0)
      {
        return 0;
      }
    }
    rounds++;
    resetDiskSystem();
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

uint8_t writeFloppy(uint32_t lba, uint8_t *src)
{
  struct Chs pos = LbaToChs(lba, &floppyGeometry);
  return writeFloppy(pos.cyl, pos.head, pos.sec, src);
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
unsigned char writeFloppyAuto(unsigned char *src)
{
  return writeFloppy(floppyPosition.cyl, floppyPosition.head, floppyPosition.sec, src);
}

/* builds one descriptor block for a batch of n consecutive hdd
   sectors starting at firstLba.

   statuses[i] is the read result of batch sector i; sectors with
   data get consecutive dataIdx values in encounter order, which is
   exactly how main.c packs their buffers (a failed read does not
   consume a buffer slot). skipped/failed ones are recorded with
   their error code but no data index.

   the whole block is zeroed first, so unused tail descriptors are
   well defined, then the crc seals bytes 0..509 */
void fillDescBlock(struct DescBlock *blk, unsigned long firstLba,
                   const unsigned char *statuses, unsigned char n)
{
  unsigned int i;
  unsigned char good;
  unsigned char *p;

  p = (unsigned char *)blk;
  for (i = 0; i < 512; i++)
  {
    p[i] = 0;
  }
  blk->count = n;
  good = 0;
  for (i = 0; i < (unsigned int)n; i++)
  {
    poke24(blk->desc[i].lba, firstLba + i);
    blk->desc[i].status = statuses[i];
    if (SECTOR_HAS_DATA(statuses[i]))
    {
      blk->desc[i].dataIdx = good;
      good++;
    }
    else
    {
      blk->desc[i].dataIdx = 0;
    }
  }
  blk->crc = crcBuf(0xFFFF, p, 510);
}
