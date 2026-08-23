/* 32 bit helpers that avoid watcom runtime calls (__U4M/__U4D) */
#ifndef MATH_H
#define MATH_H

/* multiplies a 32 bit by a 16 bit value, returns 32 bit result.
   hand rolled because the compiler would otherwise call a helper
   from the c runtime, which we do not link against (see below) */
unsigned long mulLong(unsigned long a, unsigned int b);

/* divides a 32 bit value by a 16 bit value, returns the quotient.
   same runtime-helper reason as mulLong */
unsigned long divLong(unsigned long num, unsigned int den);

/* divides v by the payload capacity of one floppy, returns quotient,
   remainder via pointer. used to translate "hdd lba" into "which
   floppy disk number" */
unsigned long divByDiskCapacity(unsigned long v, unsigned long* remainder);

#endif
