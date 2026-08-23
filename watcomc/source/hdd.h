/* hdd side: runtime geometry, chs stepping, resilient sector reading
   and the bad sector logs */
#ifndef HDD_H
#define HDD_H

#include "definitions.h"

/* ---- runtime geometry ----
   initialized from definitions.h; the startup prompts in main()
   may override them. see definitions.h for what chs means.
   hddRetries = read attempts per sector (0 = give up after one).
   headMask   = bit N set -> head N gets dumped; lets you retry a
                single dying head without re-reading the rest */
extern unsigned int hddCyls;
extern unsigned char hddHeads;
extern unsigned char hddSpt;
extern unsigned long hddTotalSectors;
extern unsigned char hddRetries;
extern unsigned char headMask;

/* current read position on the hdd. main() saves/restores it around
   each disk so an aborted disk can be retried at the same spot */
extern unsigned short hddCyl;
extern unsigned char hddHead;
extern unsigned char hddSec;
extern unsigned long hddLBA;

/* lbas of sectors that could not be read:
   badLbasDisk/diskBadCount      - this floppy disk only
   badLbasAll/allBadCount        - the whole session, for the summary
   both are written by readHddResilient(), consumed by buildHeader() */
extern unsigned long badLbasDisk[MAX_BAD];
extern unsigned int diskBadCount;
extern unsigned long badLbasAll[MAX_BAD_ALL];
extern unsigned int allBadCount;

/* advances the chs position by one sector, wrapping like the bios
   expects: sector overflows into head, head into cylinder.
   returns 1 while cylinders remain (hddCyl < hddCyls) */
unsigned char advanceCHSHdd(void);

/* walks the chs position forward to the given linear lba. no fancy
   math needed: stepping through every position visits them in
   exactly lba order, and these drives are small enough that even a
   full-travel seek costs only fractions of a second */
void seekToLBA(unsigned long target);

/* reads one hdd sector into dest with retries; on final failure the
   buffer is filled with BADFILL and the lba lands in the bad logs */
void readHddResilient(unsigned char* dest);

#endif
