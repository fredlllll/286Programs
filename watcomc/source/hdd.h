/* hdd side: runtime geometry, position tracking, resilient sector
   reading and the bad sector logs */
#ifndef HDD_H
#define HDD_H

#include "definitions.h"
#include "int13.h"

/* ---- runtime geometry ----
   initialized from definitions.h; the startup prompts in main()
   may override it. see definitions.h for what chs means */
extern struct Geometry hddGeom;

/* read retry policy: attempts per hdd sector (0 = give up after one) */
extern unsigned char hddRetries;

/* head selection bitmask: bit N set -> head N gets dumped. lets you
   retry a single dying head without re-reading the rest */
extern unsigned char headMask;

/* current read position on the hdd. main() saves/restores it around
   each disk so an aborted disk can be retried at the same spot.
   hddLBA is kept alongside because it is what goes into the header */
extern struct Chs hddPos;
extern unsigned long hddLBA;

/* lbas of sectors that could not be read:
   badLbasDisk/diskBadCount      - this floppy disk only
   badLbasAll/allBadCount        - the whole session, for the summary
   both are written by readHddResilient(), consumed by buildHeader() */
extern unsigned long badLbasDisk[MAX_BAD];
extern unsigned int diskBadCount;
extern unsigned long badLbasAll[MAX_BAD_ALL];
extern unsigned int allBadCount;

/* advances the hdd read position by one sector */
void advanceCHSHdd(void);

/* walks the chs position forward to the given linear lba. no fancy
   math needed: stepping through every position visits them in
   exactly lba order, and these drives are small enough that even a
   full-travel seek costs only fractions of a second */
void seekToLBA(unsigned long target);

/* reads one hdd sector into dest with retries; on final failure the
   buffer is filled with BADFILL and the lba lands in the bad logs */
void readHddResilient(unsigned char* dest);

#endif
