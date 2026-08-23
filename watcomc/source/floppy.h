/* floppy side: chs stepping and best effort verified sector writing */
#ifndef FLOPPY_H
#define FLOPPY_H

#include "definitions.h"

/* current write position on the floppy. main() resets it per disk
   and rewinds it to lba 0 when writing the header sector */
extern unsigned short flpCyl;
extern unsigned char flpHead;
extern unsigned char flpSec;
extern unsigned long flpLBA;

/* payload offsets (in sectors, relative to payload start) of sectors
   that could not be written+verified; consumed by buildHeader().
   the pc side uses them to know which parts of the restored image
   are BADFILL rather than real data */
extern unsigned int badFlpOffsets[MAX_BAD_FLP];
extern unsigned int badFlpCount;

/* advances the floppy chs position by one sector, wrapping like
   advanceCHSHdd does on the hdd side */
void advanceCHSFloppy(void);

/* writes one 512 byte sector at the current position with retries,
   then verifies by reading it back and comparing byte for byte.
   returns 0 = written+verified, 1 = gave up: buffer replaced with
   BADFILL and its payload offset logged.
   payloadOffset = this sector's index in the dump stream, needed
   only for logging when things go wrong */
unsigned char writeFloppyAuto(unsigned char* src, unsigned int payloadOffset);

#endif
