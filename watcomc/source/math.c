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
#include "intdef.h"

/* classic shift-and-add multiplication, the way you would do it on
   paper in binary: whenever the current lowest bit of b is set, add
   a (suitably shifted) to the result. example: a*5 = a*100b =
   a<<2 + a<<0 */
uint32_t mulLong(uint32_t a, uint16_t b){
  uint32_t r;
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
uint32_t divLong(uint32_t num, uint16_t den){
  uint32_t q;
  uint32_t rem;
  uint16_t i;
  q = 0;
  rem = 0;
  for(i = 0; i < 32; i++){
    rem <<= 1;                      /* shift remainder left ... */
    if(num & 0x80000000UL){         /* ... and pull in the top bit */
      rem |= 1;                     /* of the remaining dividend */
    }
    num <<= 1;
    q <<= 1;                        /* make room for result bit */
    if(rem >= (uint32_t)den){  /* divisor fits in remainder? */
      rem -= den;
      q |= 1;                       /* this quotient bit is 1 */
    }
  }
  return q;
}

/* 
 * Divides a 32-bit dividend by a 16-bit divisor on a 286.
 * DX:AX holds the 32-bit dividend for the hardware DIV instruction.
 * Returns 16-bit quotient, writes remainder to *rem.
 */
uint16_t div32_16(uint32_t dividend, uint16_t divisor, uint16_t *rem) {
    uint16_t quot;
    uint16_t r;

    _asm {
        mov ax, word ptr [dividend]
        mov dx, word ptr [dividend + 2]
        div divisor
        mov quot, ax
        mov r, dx
    }

    if(rem){
      *rem = r;
    }
    return quot;
}

/*
 * Multiplies a 32-bit multiplicand by a 16-bit multiplier on a 286 CPU.
 * Returns the truncated lower 32 bits of the result (uint32_t).
 */
uint32_t mul32_16(uint32_t multiplicand, uint16_t multiplier) {
    uint16_t result_low;
    uint16_t result_high;

    _asm {
        /* Step 1: Multiply Low Word of multiplicand by multiplier */
        mov ax, word ptr [multiplicand]     ; AX = Low 16 bits of multiplicand
        mul multiplier                      ; DX:AX = AX * multiplier
        mov result_low, ax                  ; Lowest 16 bits of final result
        mov cx, dx                          ; CX = carry over to the high word

        /* Step 2: Multiply High Word of multiplicand by multiplier */
        mov ax, word ptr [multiplicand + 2] ; AX = High 16 bits of multiplicand
        mul multiplier                      ; DX:AX = AX * multiplier
        add ax, cx                          ; Add carried high word from Step 1
        mov result_high, ax                 ; Upper 16 bits of final result
    }

    /* Combine the two 16-bit halves back into a 32-bit return value */
    return ((uint32_t)result_high << 16) | result_low;
}

/* repeated subtraction division. deliberately dumb and slow: it only
   runs twice at startup (resume lba -> suggested floppy number).
   APPROX_DISK_CAPACITY is a compile time constant so the loop needs
   no division either. the result is only ever a label suggestion -
   the v3 format makes real per-disk capacity depend on how many
   sectors get skipped, so exact prediction is neither possible nor
   needed */
uint32_t divByDiskCapacity(uint32_t v, uint32_t* remainder){
  uint32_t n;
  n = 0;
  while(v >= APPROX_DISK_CAPACITY){
    v -= APPROX_DISK_CAPACITY;
    n++;
  }
  *remainder = v;
  return n;
}
