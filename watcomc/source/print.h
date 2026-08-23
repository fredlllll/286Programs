/* console output on the bios teletype page 1 */
#ifndef PRINT_H
#define PRINT_H

void printChar(unsigned char inChar, unsigned short pageAndColor);
#pragma aux printChar = \
    "mov ah, 0x0e"   \
    "int 0x10"       \
    modify [ah]      \
    parm   [al][bx]

void print(const char* text);
void printHex(unsigned char value);
void printHexShort(unsigned short value);
void printHexLong(unsigned long value);
void printDecLong(unsigned long value);

#endif
