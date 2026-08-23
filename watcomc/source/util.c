#include "util.h"

static char* badFillText = "!BAD-SECTOR!";
static char* skipFillText = "!HEAD-SKIP!..";

void poke16(unsigned char* p, unsigned int v){
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
}

void poke32(unsigned char* p, unsigned long v){
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}

static unsigned int crcTable[256];

void crcInit(void){
  unsigned int i;
  unsigned int j;
  unsigned int c;
  for(i = 0; i < 256; i++){
    c = i << 8;
    for(j = 0; j < 8; j++){
      if(c & 0x8000){
        c = (c << 1) ^ 0x1021;
      }else{
        c = c << 1;
      }
    }
    crcTable[i] = c;
  }
}

unsigned int crcBuf(unsigned int crc, unsigned char* p, unsigned int n){
  while(n--){
    crc = (crc << 8) ^ crcTable[((crc >> 8) ^ *p++) & 0xFF];
  }
  return crc;
}

unsigned char memcmpBuf(unsigned char* a, unsigned char* b){
  unsigned int i;
  for(i = 0; i < 512; i++){
    if(a[i] != b[i]){
      return 1;
    }
  }
  return 0;
}

void fillBadPattern(unsigned char* dest){
  unsigned int i;
  for(i = 0; i < 512; i++){
    dest[i] = badFillText[i % 12];
  }
}

/* marks payload slots of sectors whose head is not selected in the
   head bitmask: not read at all, not logged as bad, and the pc side
   leaves the lba uncovered so another pass can fill it in */
void fillSkipPattern(unsigned char* dest){
  unsigned int i;
  for(i = 0; i < 512; i++){
    dest[i] = skipFillText[i % 13];
  }
}

/* bios data area tick counter at 0040:006C, incremented 18.206 times
   per second by the timer interrupt. independent of the cmos battery */
unsigned long biosTicks(void){
  volatile unsigned short lo;
  volatile unsigned short hi;
  _asm{
    push es
    mov ax, 0x0040
    mov es, ax
    xor bx, bx
    mov ax, es:[bx+0x6C]
    mov lo, ax
    mov ax, es:[bx+0x6E]
    mov hi, ax
    pop es
  };
  return ((unsigned long)hi << 16) | lo;
}
