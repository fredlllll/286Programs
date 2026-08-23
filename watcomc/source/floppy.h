/* floppy side: chs stepping and best effort verified sector writing */
#ifndef FLOPPY_H
#define FLOPPY_H

#include "definitions.h"

extern unsigned short flpCyl;
extern unsigned char flpHead;
extern unsigned char flpSec;
extern unsigned long flpLBA;

extern unsigned int badFlpOffsets[MAX_BAD_FLP];
extern unsigned int badFlpCount;

void advanceCHSFloppy(void);
/* returns 0 = written+verified, 1 = gave up (sector replaced with
   BADFILL and its payload offset logged) */
unsigned char writeFloppyAuto(unsigned char* src, unsigned int payloadOffset);

#endif
