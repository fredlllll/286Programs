/* small shared helpers: byte poking into raw sectors, crc16, buffer
   fills, bios data area access */
#ifndef UTIL_H
#define UTIL_H

/* store a 16/24/32 bit value into a byte buffer, least significant
   byte first ("little endian", the native byte order of x86 and of
   the on-disk header format). see util.c for why this exists.
   poke24 exists because the sector descriptors store hdd lbas in
   3 bytes - enough for 16 million sectors, half the size of a 32
   bit field */
void poke16(unsigned char* p, unsigned int v);
void poke24(unsigned char* p, unsigned long v);
void poke32(unsigned char* p, unsigned long v);

/* crc16-ccitt checksum. call crcInit once at startup, then feed any
   amount of data through crcBuf (the returned value is the new
   running checksum that the next call continues from) */
void crcInit(void);
unsigned int crcBuf(unsigned int crc, unsigned char* p, unsigned int n);

/* compares two 512 byte sector buffers, returns 0 if identical,
   nonzero otherwise */
unsigned char memcmpBuf(unsigned char* a, unsigned char* b);

/* reads the bios tick counter: memory at 0040:006C that the timer
   interrupt increments ~18.2 times per second since midnight.
   returns it as one 32 bit number */
unsigned long biosTicks(void);

#endif
