/* small shared helpers: byte poking into raw sectors, crc16, buffer fills,
   bios data area access */
#ifndef UTIL_H
#define UTIL_H

void poke16(unsigned char* p, unsigned int v);
void poke32(unsigned char* p, unsigned long v);
void crcInit(void);
unsigned int crcBuf(unsigned int crc, unsigned char* p, unsigned int n);
unsigned char memcmpBuf(unsigned char* a, unsigned char* b);
void fillBadPattern(unsigned char* dest);
void fillSkipPattern(unsigned char* dest);
unsigned long biosTicks(void);

#endif
