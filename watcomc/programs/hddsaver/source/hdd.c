#include "hdd.h"
#include "int13.h"
#include "print.h"

/* runtime geometry, initialized from the defines in definitions.h; the
   startup prompts allow overriding because the bios may translate
   differently (e.g. after the cmos battery died) and reads only line
   up when we use exactly the geometry the data was originally written
   with */
const struct Geometry hddGeom = {HDD_CYLS, HDD_HEADS, HDD_SPT, HDD_TOTAL_SECTORS};
struct ChsWithLBA hddPos = {0, 0, 1, 0};
uint8_t hddRetries = RETRY_HDD;
uint8_t headMask = 0xFF;

void advanceHddPosition(void)
{
  stepChs(&hddPos, &hddGeom);
}

void seekHdd(uint32_t target)
{
  hddPos = LbaToChsWithLba(target, &hddGeom);
}

uint8_t isStatusSuccess(uint8_t status)
{
  return status == 0 || status == 0x11;
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
uint8_t readHddResilient(void __far *dest)
{
  uint8_t tries;
  uint8_t status;

  tries = 0;
  do
  {
    if (hddRetries > 1 && tries == hddRetries / 2)
    {
      resetDiskSystem(0x80); /* halfway through, try with a reset */
    }
    status = readFromDrive(1, hddPos.cyl, hddPos.head, hddPos.sec, 0x80, dest);
    tries++;
  } while (!isStatusSuccess(status) && tries < hddRetries);

  if (isStatusSuccess(status))
  {
    /* 0x11 = recoverable ECC error, bios already corrected the data.
       the distinction is preserved: the restorer may want to know
       which sectors only survived via ecc correction */
    return status;
  }
  else
  {
    resetDiskSystem(0x80); /* clean up controller state for next sector */
  }

  print("HDD read fail CHS ");
  printDecLong(hddPos.cyl);
  printChar('/', 1);
  printDecLong(hddPos.head);
  printChar('/', 1);
  printDecLong(hddPos.sec);
  print(" LBA: ");
  printDecLong(hddPos.lba);
  print(" Status: ");
  printInt13Status(status);
  print("\r");

  return status;
}
