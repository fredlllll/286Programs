#include "print.h"

static char* hexAlphabet = "0123456789ABCDEF";

void print(const char* text){
  char ch;
  while (ch = *text++){
    printChar(ch, 1);
  }
}

void printHex(unsigned char value){
  char ch = hexAlphabet[value >> 4];
  printChar(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  printChar(ch, 1);
}

void printHexShort(unsigned short value){
  printHex(value >> 8);
  printHex(value & 0xFF);
}

void printHexLong(unsigned long value){
  printHexShort(value >> 16);
  printHexShort(value & 0xFFFF);
}

static unsigned long pow10[10] = {1ul,10ul,100ul,1000ul,10000ul,100000ul,
                                  1000000ul,10000000ul,100000000ul,1000000000ul};

void printDecLong(unsigned long v){
  char c;
  unsigned char i;
  unsigned char started;
  started = 0;
  for(i = 9;; i--){
    c = '0';
    while(v >= pow10[i]){
      v -= pow10[i];
      c++;
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
