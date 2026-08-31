/* 32 bit helpers that avoid watcom runtime calls (__U4M/__U4D) */
#ifndef MATH_H
#define MATH_H
#include "intdef.h"

/* multiplies a 32 bit by a 16 bit value, returns 32 bit result.
   hand rolled because the compiler would otherwise call a helper
   from the c runtime, which we do not link against (see below) */
uint32_t mulLong(uint32_t a, uint16_t b);

/* divides a 32 bit value by a 16 bit value, returns the quotient.
   same runtime-helper reason as mulLong */
uint32_t divLong(uint32_t num, uint16_t den);

uint16_t div32_16(uint32_t dividend, uint16_t divisor, uint16_t *rem);

/*
 * Multiplies a 32-bit multiplicand by a 16-bit multiplier on a 286 CPU.
 * Returns the truncated lower 32 bits of the result (uint32_t).
 */
uint32_t mul32_16(uint32_t multiplicand, uint16_t multiplier);

/* crc-8/ccitt over a short buffer. polynomial 0x07, init 0xff.
   used for per-descriptor integrity checking */
uint8_t crc8(uint8_t *buf, uint8_t len);

#endif
