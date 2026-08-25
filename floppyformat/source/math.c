/* 32 bit helpers that avoid watcom runtime calls (__U4M/__U4D) */

#include "math.h"
#include "intdef.h"

uint32_t mulLong(uint32_t a, uint16_t b){
  uint32_t r;
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

uint32_t divLong(uint32_t num, uint16_t den){
  uint32_t q;
  uint32_t rem;
  uint16_t i;
  q = 0;
  rem = 0;
  for(i = 0; i < 32; i++){
    rem <<= 1;
    if(num & 0x80000000UL){
      rem |= 1;
    }
    num <<= 1;
    q <<= 1;
    if(rem >= (uint32_t)den){
      rem -= den;
      q |= 1;
    }
  }
  return q;
}

uint16_t div32_16(uint32_t dividend, uint16_t divisor, uint16_t *rem) {
    uint16_t quot;
    uint16_t r;

    if(divisor == 0 || (dividend >> 16) >= divisor){
      quot = 0xFFFF;
      r = 0xFFFF;
    }else{
      _asm {
          mov ax, word ptr [dividend]
          mov dx, word ptr [dividend + 2]
          div divisor
          mov quot, ax
          mov r, dx
      }
    }

    if(rem){
      *rem = r;
    }
    return quot;
}

uint32_t mul32_16(uint32_t multiplicand, uint16_t multiplier) {
    uint16_t result_low;
    uint16_t result_high;

    _asm {
        mov ax, word ptr [multiplicand]
        mul multiplier
        mov result_low, ax
        mov cx, dx

        mov ax, word ptr [multiplicand + 2]
        mul multiplier
        add ax, cx
        mov result_high, ax
    }

    return ((uint32_t)result_high << 16) | result_low;
}
