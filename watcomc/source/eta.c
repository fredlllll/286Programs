#include "definitions.h"
#include "math.h"
#include "print.h"

static uint32_t waitTicksAccumulated = 0;

void addWaitTicks(uint32_t ticks){
  waitTicksAccumulated += ticks;
}

void resetWaitTicks(void){
  waitTicksAccumulated = 0;
}

/* append estimated time until this disk finishes, based on pace so far.
   elapsedTicks = ticks since this floppy started, floppyLba = sectors
   written to this floppy so far. subtracts wait ticks (time spent
   paused for floppy swaps) so the eta only reflects writing time.
   uses the actual floppy fullness instead of a fixed capacity
   estimate, so headmask settings are reflected accurately */
void printEta(uint32_t elapsedTicks, uint16_t floppyLba){
  uint32_t remainT;
  uint16_t m;
  uint16_t s;
  uint16_t rem;
  if(floppyLba == 0){
    return;
  }
  if(elapsedTicks > waitTicksAccumulated){
    elapsedTicks -= waitTicksAccumulated;
  }
  if(elapsedTicks == 0){
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
  print("  ");
}
