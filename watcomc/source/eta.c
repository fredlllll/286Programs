#include "definitions.h"
#include "math.h"
#include "print.h"

/* append estimated time until this disk finishes, based on pace so far.
   elapsedTicks = ticks since this floppy started, floppyLba = sectors
   written to this floppy so far. uses the actual floppy fullness
   instead of a fixed capacity estimate, so headmask settings are
   reflected accurately */
void printEta(uint32_t elapsedTicks, uint16_t floppyLba){
  uint32_t remainT;
  uint16_t m;
  uint16_t s;
  uint16_t rem;
  if(floppyLba == 0){
    return;
  }
  /* elapsed/floppyLba = time per sector; remaining sectors =
     FLOPPY_TOTAL_SECTORS - floppyLba; multiply to get remaining time */
  remainT = divLong(mulLong(elapsedTicks,
                            FLOPPY_TOTAL_SECTORS - floppyLba), floppyLba);
  if(remainT < 18){
    return;                    /* less than a second left: show nothing */
  }
  m = div32_16(remainT, 1092, &rem);
  s = div32_16(rem, 18, 0);
  print(" eta ");
  printDecLong(m);
  printChar(':', 1);
  if(s < 10){
    printChar('0', 1);
  }
  printDecLong(s);
}
