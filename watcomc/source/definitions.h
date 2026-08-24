/* central knob list for the whole tool. every constant the dump
   behavior depends on numerically lives here, so there is one place
   to tune things.

   quick glossary for the terms used below:
   - sector: the smallest unit a disk can read or write, 512 bytes
     on both drives handled here
   - chs: cylinder/head/sector, the classic pc way of addressing a
     disk position (think vinyl record: groove = cylinder, side =
     head, position on the ring = sector)
   - lba: logical block addressing, an alternative scheme that just
     numbers all sectors 0, 1, 2, ... from the start of the disk */

#ifndef DEFINITIONS_H
#define DEFINITIONS_H
#include "intdef.h"

/* ---- hard disk geometry ----
   factory geometry of the target mfm/rll drive. reads only line up
   with data written earlier when the exact same geometry is used,
   so these values double as the documentation of the expected drive */
#define HDD_CYLS 820
#define HDD_HEADS 6
#define HDD_SPT 26              /* sectors per track */
#define HDD_TOTAL_SECTORS ((uint32_t)HDD_CYLS*(uint32_t)HDD_HEADS*(uint32_t)HDD_SPT)

/* ---- 1.44mb floppy geometry ----
   fixed for a standard 3.5" hd floppy: 80 cylinders x 2 heads x 18
   sectors of 512 bytes = 2880 sectors total */
#define FLOPPY_CYLS 80
#define FLOPPY_HEADS 2
#define FLOPPY_SPT 18
#define FLOPPY_TOTAL_SECTORS (FLOPPY_CYLS*FLOPPY_HEADS*FLOPPY_SPT)

/* ---- retry and error handling policy ---- */

#define RETRY_HDD 16            /* read attempts per hdd sector */
#define RETRY_FLOPPY 4          /* write attempts per round */
#define REWRITE_ROUNDS 5        /* floppy: up to RETRY_FLOPPY*REWRITE_ROUNDS attempts per sector */

#define APPROX_DISK_CAPACITY \
  ((uint32_t)(FLOPPY_TOTAL_SECTORS-28))

#endif
