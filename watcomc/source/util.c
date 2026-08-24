#include "util.h"

/* the header sector on each floppy stores multi byte numbers (16 and
   32 bit). memory holds bytes, not numbers, so a value like
   0x12345678 has to be written as four separate bytes - and the
   order matters. convention here (and on all x86 pcs) is "little
   endian": least significant byte first, i.e.
     poke32(p, 0x12345678) -> p[0]=0x78 p[1]=0x56 p[2]=0x34 p[3]=0x12
   writing it byte by byte makes the on-disk layout explicit and
   independent of compiler tricks. the shifts peel off one byte at a
   time: v >> 8 discards the lowest byte, etc. */

void poke16(uint8_t* p, uint16_t v){
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}

/* three byte variant used by the sector descriptors: an hdd lba as
   24 bits little endian. covers drives up to 8 gb with 512 byte
   sectors, which is far beyond anything these mfm/rll controllers
   could ever address */
void poke24(uint8_t* p, uint32_t v){
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
}

void poke32(uint8_t* p, uint32_t v){
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16);
  p[3] = (uint8_t)(v >> 24);
}

/* ---- crc16-ccitt ----
   a crc is a checksum that detects corrupted data: a block of bytes
   is folded through a fixed mathematical recipe and the final 16 bit
   value travels alongside it (in the disk header, in every
   descriptor block). when the pc side re-reads the disk it recomputes
   the crc; any mismatch means silent data damage (bad media that read
   back "successfully"). this variant uses the ccitt polynomial 0x1021
   with initial value 0xffff.

   table driven implementation: crcInit precomputes what one byte
   contributes for all 256 possible byte values, which turns each
   subsequent byte into a single table lookup + xor instead of eight
   shift/xor steps */

static uint16_t crcTable[256];

void crcInit(void){
  uint16_t i;
  uint16_t j;
  uint16_t c;
  for(i = 0; i < 256; i++){
    c = i << 8;
    for(j = 0; j < 8; j++){       /* process one "virtual byte" */
      if(c & 0x8000){             /* top bit set -> xor polynomial */
        c = (c << 1) ^ 0x1021;
      }else{
        c = c << 1;
      }
    }
    crcTable[i] = c;
  }
}

/* folds n bytes into the running checksum */
uint16_t crcBuf(uint16_t crc, uint8_t* p, uint16_t n){
  while(n--){
    crc = (crc << 8) ^ crcTable[((crc >> 8) ^ *p++) & 0xFF];
  }
  return crc;
}

/* sector sized compare, used to verify a floppy write by reading the
   sector back and comparing against what we intended to write */
uint8_t memcmpBuf(uint8_t* a, uint8_t* b){
  uint16_t i;
  for(i = 0; i < 512; i++){
    if(a[i] != b[i]){
      return 1;
    }
  }
  return 0;
}

/* the bios keeps its tick counter in low memory at segment 0x0040,
   offset 0x006c; the hardware timer interrupt increments it about
   18.2 times per second. it is a 32 bit counter stored little
   endian, so we read the two halves and glue them together.
   volatile because the value changes asynchronously behind our back */
uint32_t biosTicks(void){
  volatile uint16_t lo;
  volatile uint16_t hi;
  _asm{
    push es               /* es is clobbered, save it */
    mov ax, 0x0040        ; bios data area segment
    mov es, ax
    xor bx, bx            ; offset base
    mov ax, es:[bx+0x6C]  ; low 16 bits of tick count
    mov lo, ax
    mov ax, es:[bx+0x6E]  ; high 16 bits
    mov hi, ax
    pop es
  };
  return ((uint32_t)hi << 16) | lo;
}

void halt(void){
  while(1){
    _asm{ hlt }; 
  }
}
