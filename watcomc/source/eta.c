#include "definitions.h"
#include "math.h"
#include "print.h"

/* append estimated time until this disk finishes, based on pace so far.
   done = hdd sectors attempted on this disk. stays in tick units as
   long as possible; minutes extracted by subtraction (1092 ticks ~=
   60 s), seconds by native 16-bit division */
static void printEta(unsigned long elapsedTicks, unsigned int done){
  unsigned long totalT;
  unsigned long remainT;
  unsigned long m;
  unsigned int s;
  if(done == 0){
    return;
  }
  /* rule of three: elapsed/done == total/capacity -> extrapolate onto
     the rough per-disk capacity. both multiplies go through mulLong
     because * would need the missing runtime helper */
  totalT = divLong(mulLong(elapsedTicks, APPROX_DISK_CAPACITY), done);
  if(totalT <= elapsedTicks){
    return;                    /* estimate not ahead: show nothing */
  }
  remainT = totalT - elapsedTicks;
  m = 0;
  while(remainT >= 1092){      /* 1092 ticks ~ 60 seconds */
    remainT -= 1092;
    m++;
  }
  s = (unsigned int)remainT / 18;   /* leftover ticks -> seconds */
  print(" eta ");
  printDecLong(m);
  printChar(':', 1);
  if(s < 10){
    printChar('0', 1);
  }
  printDecLong(s);
}
