/* hdd side: runtime geometry, chs stepping, resilient sector reading
   and the bad sector logs */
#ifndef HDD_H
#define HDD_H

#include "definitions.h"

extern unsigned int hddCyls;
extern unsigned char hddHeads;
extern unsigned char hddSpt;
extern unsigned long hddTotalSectors;
extern unsigned char hddRetries;        /* 0 = one attempt, no resets */
extern unsigned char headMask;          /* bit N = dump head N */

extern unsigned short hddCyl;
extern unsigned char hddHead;
extern unsigned char hddSec;
extern unsigned long hddLBA;

extern unsigned long badLbasDisk[MAX_BAD];
extern unsigned int diskBadCount;
extern unsigned long badLbasAll[MAX_BAD_ALL];
extern unsigned int allBadCount;

unsigned char advanceCHSHdd(void);
void seekToLBA(unsigned long target);
void readHddResilient(unsigned char* dest);

#endif
