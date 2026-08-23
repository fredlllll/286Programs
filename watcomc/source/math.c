#include "math.h"

/* 32x16 bit multiply without the watcom runtime helper (__U4M):
   plain shift-add, called once at startup so speed is irrelevant */
unsigned long mulLong(unsigned long a, unsigned int b){
  unsigned long r;
  r = 0;
  while(b){
    if(b & 1){
      r += a;
    }
    a += a;
    b >>= 1;
  }
  return r;
}

/* 32/16 bit binary long division without the watcom __U4D helper */
unsigned long divLong(unsigned long num, unsigned int den){
  unsigned long q;
  unsigned long rem;
  unsigned int i;
  q = 0;
  rem = 0;
  for(i = 0; i < 32; i++){
    rem <<= 1;
    if(num & 0x80000000UL){
      rem |= 1;
    }
    num <<= 1;
    q <<= 1;
    if(rem >= (unsigned long)den){
      rem -= den;
      q |= 1;
    }
  }
  return q;
}
