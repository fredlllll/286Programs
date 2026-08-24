#include "print.h"
#include "intdef.h"

/* lookup table for hex nibbles: index 0..15 -> character '0'..'F'.
   a nibble is half a byte, i.e. one hex digit */
static char* hexAlphabet = "0123456789ABCDEF";

void print(const char* text){
  char ch;
  while (ch = *text++){     /* read char, advance pointer, stop at 0 */
    printChar(ch, 1);
  }
}

/* value >> 4 throws away the low nibble leaving the high one,
   value & 0x0F masks away everything but the low nibble */
void printHex(uint8_t value){
  char ch = hexAlphabet[value >> 4];
  printChar(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  printChar(ch, 1);
}

void printHexShort(uint16_t value){
  printHex(value >> 8);         /* high byte first */
  printHex(value & 0xFF);       /* then low byte   */
}

void printHexLong(uint32_t value){
  printHexShort(value >> 16);
  printHexShort(value & 0xFFFF);
}

/* powers of ten up to 10^9; the largest unsigned long is about
   4.29 * 10^9, so ten entries are enough */
static unsigned long pow10[10] = {1ul,10ul,100ul,1000ul,10000ul,100000ul,
                                  1000000ul,10000000ul,100000000ul,1000000000ul};

/* prints v in decimal without any division: for each power of ten,
   count how often it fits by subtracting over and over. the "started"
   flag suppresses leading zeros but makes sure at least one digit
   (the last) always comes out */
void printDecLong(unsigned long v){
  char c;
  unsigned char i;
  unsigned char started;
  started = 0;
  for(i = 9;; i--){
    c = '0';
    while(v >= pow10[i]){
      v -= pow10[i];
      c++;                  /* count subtractions = next digit */
    }
    if(c > '0' || started || i == 0){
      printChar(c, 1);
      started = 1;
    }
    if(i == 0){
      break;
    }
  }
}
