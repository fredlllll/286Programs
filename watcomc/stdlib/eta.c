#include "eta.h"
#include "math.h"
#include "print.h"

static uint32_t waitTicksAccumulated = 0;

void addWaitTicks(uint32_t ticks){
  waitTicksAccumulated += ticks;
}

void resetWaitTicks(void){
  waitTicksAccumulated = 0;
}

void printEta(uint32_t elapsedTicks, uint16_t current, uint16_t total){
  uint32_t remainT;
  uint16_t m;
  uint16_t s;
  uint16_t rem;
  if(current == 0){
    return;
  }
  if(elapsedTicks > waitTicksAccumulated){
    elapsedTicks -= waitTicksAccumulated;
  }
  if(elapsedTicks == 0){
    return;
  }
  remainT = divLong(mulLong(elapsedTicks, total - current), current);
  if(remainT < 18){
    return;
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
