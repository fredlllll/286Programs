/* console output.
   everything goes through the bios "teletype" service: interrupt
   10h with ah=0eh prints one character at the cursor position and
   advances the cursor. it is the simplest possible text output on a
   pc - no driver, no memory mapping, works on virtually every bios.

   printChar itself contains no c code. the #pragma aux below tells
   the watcom compiler to replace every call to it with exactly those
   two machine instructions (inline assembly), which keeps the code
   tiny and avoids function call overhead:
     al = character to print
     bx = page number in the high byte, color in the low byte
          (page 0 = visible text page, color 1 = blue, only matters
          in graphics modes)
   because this pragma lives in the header, every file that includes
   print.h gets the same inline expansion */
#ifndef PRINT_H
#define PRINT_H
#include "intdef.h"

void printChar(unsigned char inChar, unsigned short pageAndColor);
#pragma aux printChar = \
    "mov ah, 0x0e"   \
    "int 0x10"       \
    modify [ah]      \
    parm   [al][bx]

/* prints a zero terminated string */
void print(const char* text);

/* hex output, most significant digit first */
void printHex(uint8_t value);         /* 2 digits  */
void printHexShort(uint16_t value);   /* 4 digits  */
void printHexLong(uint32_t value);     /* 8 digits  */

/* decimal output without leading zeros */
void printDecLong(uint32_t value);

#endif
