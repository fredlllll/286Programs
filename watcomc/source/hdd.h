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

/* lbas of sectors that could not be read, whole session, for the
   end-of-run summary printed by main(). the durable per-sector record
   (lba + error code) lives in the descriptor blocks on the floppies */
extern unsigned long badLbasAll[MAX_BAD_ALL];
extern unsigned int allBadCount;

/* advances the hdd read position by one sector */
void advanceCHSHdd(void);

/* walks the chs position forward to the given linear lba. no fancy
   math needed: stepping through every position visits them in
   exactly lba order, and these drives are small enough that even a
   full-travel seek costs only fractions of a second */
void seekToLBA(unsigned long target);

/* reads one hdd sector into dest with retries. returns the bios
   status: 0 = clean read, 0x11 = read but ecc-corrected (both are
   good data), anything else = unreadable, dest content undefined
   and NOT to be dumped; caller records the code in the sector
   descriptor instead */
unsigned char readHddResilient(unsigned char* dest);

#endif
