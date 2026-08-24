/* why these helpers exist:
   this is a freestanding program - there is no operating system and
   no c runtime library. the watcom compiler happily generates code
   for c operators like "/" or "%" on 32 bit values, but on an 80286
   there are no cpu instructions for that; watcom instead emits calls
   to little helper routines (__U4D for division, __U4M for multiply)
   that live in its runtime library. since we link with
   NODEFAULTLIBS those helpers do not exist, and the linker would
   fail (or worse, silently pull in runtime code). so whenever the
   dumper needs 32 bit arithmetic beyond add/subtract/compare/shift
   - which compiles to real instructions - it calls these slow but
   self contained replacements */

#include "math.h"
#include "definitions.h"

/* classic shift-and-add multiplication, the way you would do it on
   paper in binary: whenever the current lowest bit of b is set, add
   a (suitably shifted) to the result. example: a*5 = a*100b =
   a<<2 + a<<0 */
unsigned long mulLong(unsigned long a, unsigned int b){
  unsigned long r;
  r = 0;
  while(b){
    if(b & 1){          /* current lowest bit of b set? */
      r += a;           /* then this power of two contributes */
    }
    a += a;             /* a <<= 1: next bit weighs twice as much */
    b >>= 1;            /* move to the next bit of b */
  }
  return r;
}

/* binary long division, again like on paper but base 2: bring the
   dividend down bit by bit into a remainder register; whenever the
   remainder "fits", subtract the divisor and record a 1 in the
   quotient. 32 iterations because num has 32 bits */
unsigned long divLong(unsigned long num, unsigned int den){
  unsigned long q;
  unsigned long rem;
  unsigned int i;
  q = 0;
  rem = 0;
  for(i = 0; i < 32; i++){
    rem <<= 1;                      /* shift remainder left ... */
    if(num & 0x80000000UL){         /* ... and pull in the top bit */
      rem |= 1;                     /* of the remaining dividend */
    }
    num <<= 1;
    q <<= 1;                        /* make room for result bit */
    if(rem >= (unsigned long)den){  /* divisor fits in remainder? */
      rem -= den;
      q |= 1;                       /* this quotient bit is 1 */
    }
  }
  return q;
}

/* repeated subtraction division. deliberately dumb and slow: it only
   runs twice at startup (resume lba -> suggested floppy number).
   APPROX_DISK_CAPACITY is a compile time constant so the loop needs
   no division either. the result is only ever a label suggestion -
   the v3 format makes real per-disk capacity depend on how many
   sectors get skipped, so exact prediction is neither possible nor
   needed */
unsigned long divByDiskCapacity(unsigned long v, unsigned long* remainder){
  unsigned long n;
  n = 0;
  while(v >= APPROX_DISK_CAPACITY){
    v -= APPROX_DISK_CAPACITY;
    n++;
  }
  *remainder = v;
  return n;
}
