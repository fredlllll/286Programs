/* central knob list for hdd saver. every constant the dump behavior
   depends on numerically lives here, so there is one place to tune.

   quick glossary:
   - sector: smallest unit a disk reads/writes, 512 bytes
   - chs: cylinder/head/sector, classic pc addressing
   - lba: logical block addressing, sequential sector numbering */

#ifndef DEFINITIONS_H
#define DEFINITIONS_H
#include "intdef.h"

/* ---- hard disk geometry ---- */
#define HDD_CYLS 820
#define HDD_HEADS 6
#define HDD_SPT 26
#define HDD_TOTAL_SECTORS ((uint32_t)HDD_CYLS*(uint32_t)HDD_HEADS*(uint32_t)HDD_SPT)

/* ---- retry policy ---- */
#define RETRY_HDD 16

#endif
