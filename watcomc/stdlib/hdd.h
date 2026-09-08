/* hdd side: runtime geometry, position tracking, resilient sector
   reading and the bad sector logs.

   the default geometry and retry policy live here because they are
   drive properties, not an application concern. any program that wants
   different values may overwrite the variables after including this
   header; hddGeom is initialized from these defines at build time. */
#ifndef HDD_H
#define HDD_H

#include "intdef.h"
#include "int13.h"

/* ---- default hard disk geometry ----
   chs = cylinder/head/sector, classic pc addressing. lba = logical
   block addressing, sequential sector numbering */
#define HDD_CYLS 820
#define HDD_HEADS 6
#define HDD_SPT 26
#define HDD_TOTAL_SECTORS ((uint32_t)HDD_CYLS*(uint32_t)HDD_HEADS*(uint32_t)HDD_SPT)

/* ---- bios read status codes ----
   returned by readHddResilient (see isStatusSuccess below). 0 = clean
   read, 0x11 = read but ecc-corrected, both carry good data */
#define ST_OK 0x00
#define ST_ECC 0x11

/* ---- runtime geometry ----
   the startup code may override the values before first use */
extern const struct Geometry hddGeom;
/* current read position on the hdd. main() saves/restores it around
   each disk so an aborted disk can be retried at the same spot.
   hddLBA is kept alongside because it is what goes into the header */
extern struct ChsWithLBA hddPos;

/* read retry policy: attempts per hdd sector (0 = give up after one) */
extern uint8_t hddRetries;
#define RETRY_HDD 16

/* controller resets while retrying: set from the pc. resets are loud
   (seek to cylinder 0 and back), so a worn drive can be read with them
   permanently off */
extern uint8_t resetsEnabled;
#define RESETS_HDD 0

/* head selection bitmask: bit N set -> head N gets dumped. lets you
   retry a single dying head without re-reading the rest */
extern uint8_t headMask;

/* advances the hdd read position by one sector */
void advanceHddPosition(void);

/* walks the chs position forward to the given linear lba. no fancy
   math needed: stepping through every position visits them in
   exactly lba order, and these drives are small enough that even a
   full-travel seek costs only fractions of a second */
void seekHdd(uint32_t lba);

/* fast-forwards hddPos over any sectors whose head is masked. masked
   sectors are neither touched nor transmitted, so they must not be
   "started at". returns how many sectors were skipped, so callers can
   tell the operator if a seek landed inside a masked head and the dump
   will really begin a few sectors later instead of at the target */
uint32_t skipMaskedHeads(void);

uint8_t isStatusSuccess(uint8_t status);

/* reads one hdd sector into dest with retries. returns the bios
   status: ST_OK = clean read, ST_ECC = read but ecc-corrected (both
   are good data), anything else = unreadable, dest content undefined
   and NOT to be dumped; caller records the code in the sector
   descriptor instead. dest is a far pointer (see int13.h); the near
   hddReadBuffer converts automatically */
uint8_t readHddResilient(void __far *dest);

#endif
