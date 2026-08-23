/* 32 bit helpers that avoid watcom runtime calls (__U4M/__U4D) */
#ifndef MATH_H
#define MATH_H

unsigned long mulLong(unsigned long a, unsigned int b);
unsigned long divLong(unsigned long num, unsigned int den);
unsigned long divByDiskCapacity(unsigned long v, unsigned long* remainder);

#endif
