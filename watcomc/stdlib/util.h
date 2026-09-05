/* small shared helpers: byte poking into raw sectors, crc16, buffer
   fills, bios data area access */
#ifndef UTIL_H
#define UTIL_H
#include "intdef.h"

#define SECONDS * 18

// Function pointer typedef for Real Mode 16-bit ISRs
typedef void(__interrupt __far *isrPtr)(void);
#define MK_FP(seg, off) ((void __far *)(((unsigned long)(seg) << 16) | (unsigned short)(off)))

/* store a 16/24/32 bit value into a byte buffer, least significant
   byte first ("little endian", the native byte order of x86 and of
   the on-disk header format). see util.c for why this exists.
   poke24 exists because the sector descriptors store hdd lbas in
   3 bytes - enough for 16 million sectors, half the size of a 32
   bit field */
void poke16(uint8_t* p, uint16_t v);
void poke24(uint8_t* p, uint32_t v);
void poke32(uint8_t* p, uint32_t v);

/* crc16-ccitt checksum. call crcInit once at startup, then feed any
   amount of data through crcBuf (the returned value is the new
   running checksum that the next call continues from) */
void crcInit(void);
uint16_t crcBuf(uint16_t crc, uint8_t* p, uint16_t n);

/* one-shot crc16 over a buffer (calls crcInit internally) */
uint16_t crc16(const uint8_t* p, uint16_t n);

/* compares two 512 byte sector buffers, returns 0 if identical,
   nonzero otherwise. far pointers so a floppy verify can compare a
   dgroup stack buffer against data in the far sector arena */
uint8_t memcmpBuf(uint8_t __far *a, uint8_t __far *b);

/* reads the bios tick counter: memory at 0040:006C that the timer
   interrupt increments ~18.2 times per second since midnight.
   returns it as one 32 bit number */
uint32_t biosTicks(void);

void halt(void);

/* read/write a 32-bit far pointer (segment:offset) from arbitrary
   real-mode memory. thin wrapper around a far pointer dereference,
   useful for IVT entries and similar fixed absolute addresses. */
uint32_t readFar(uint16_t seg, uint16_t off);
void writeFar(uint16_t seg, uint16_t off, uint32_t ptr);

/* returns the current code segment register. used when a vector (isr)
   lives in a program that runs in its own (possibly non-zero) segment,
   so the interrupt vector can store the segment explicitly. */
uint16_t getCs(void);

#endif
