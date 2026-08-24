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

/* ---- hard disk geometry ----
   factory geometry of the target mfm/rll drive. the startup prompts
   allow overriding these because the bios may report something else
   (drive translation, dead cmos battery), and recovery only lines up
   with data written earlier when the exact same geometry is used */
#define HDD_CYLS 820
#define HDD_HEADS 6
#define HDD_SPT 26              /* sectors per track */
#define HDD_TOTAL_SECTORS ((unsigned long)HDD_CYLS*HDD_HEADS*HDD_SPT)

/* ---- 1.44mb floppy geometry ----
   fixed for a standard 3.5" hd floppy: 80 cylinders x 2 heads x 18
   sectors of 512 bytes = 2880 sectors total */
#define FLPD_CYLS 80
#define FLPD_HEADS 2
#define FLPD_SPT 18
#define FLPD_TOTAL_SECTORS (FLPD_CYLS*FLPD_HEADS*FLPD_SPT)

/* each floppy starts with 10 reserved sectors (the header sector
   plus padding), the remaining capacity carries groups of descriptor
   blocks + their data sectors */
#define HEADER_LBAS 10

/* how many hdd sectors one floppy can roughly hold in the v3 format:
   payload slots shrunk by the descriptor-block overhead (one block
   sector per BATCH_SECTORS data sectors). this is only good enough
   for the disk-number suggestion at startup and the time estimate;
   the real capacity depends on how many sectors get skipped, because
   unreadable sectors cost a descriptor (5 bytes) but no data slot */
#define APPROX_DISK_CAPACITY \
  ((unsigned long)(FLPD_TOTAL_SECTORS-HEADER_LBAS)*BATCH_SECTORS/(BATCH_SECTORS+1))

/* ---- retry and error handling policy ---- */
#define BATCH_SECTORS 26        /* one full hdd track per ram buffer; also
                                   the number of descriptors per descriptor
                                   block, see DESC_PER_BLOCK */
#define DESC_PER_BLOCK BATCH_SECTORS  /* descriptors carried by one descriptor
                                         block sector = one batch/track */
#define RETRY_HDD 16            /* read attempts per hdd sector */
#define RETRY_FLOPPY 4          /* write attempts per round */
#define REWRITE_ROUNDS 5        /* floppy: up to RETRY_FLOPPY*REWRITE_ROUNDS attempts per sector */
#define MAX_BAD_ALL 64          /* session-wide bad lba log for the summary */

#endif
