/* floppy side: chs stepping, verified sector writing and the on-disk
   format (disk header + per-sector descriptor blocks) */
#ifndef FLOPPY_H
#define FLOPPY_H

#include "definitions.h"
#include "int13.h"

/* ---- the v3 floppy format ----

   a disk no longer pretends the hdd image is cut into fixed-size,
   strictly ordered slices. instead every dumped sector travels with
   its own little piece of ID, so:

     - disks can be restored in any order (diskIndex is just a label)
     - unreadable hdd sectors cost one 5-byte descriptor instead of
       512 bytes of filler, so they waste (almost) nothing
     - the restorer learns the exact bios error for every missing
       sector, not just "something is missing here"

   physical layout of a disk:

     lba 0        : disk header (struct FloppyHeader below)
     lba 1..9     : reserved
     lba 10..     : repeating groups, one group per hdd track batch:
                      [descriptor block: 1 sector, struct DescBlock]
                      [the data sectors referenced by its entries]

   within a group only sectors whose status says "data follows" get a
   data slot; their position in that list is stored explicitly in the
   descriptor (dataIdx), because skipped sectors punch holes into the
   sequence. the restorer scatters each data sector to file offset
   lba * 512 and leaves everything no descriptor covers alone */

/* ---- status codes stored in a descriptor ----

   the raw int 13h bios status is stored verbatim, so the restorer
   sees exactly what the drive reported (0x02 = address mark not
   found, 0x04 = sector not found, 0x10 = ecc error, ...). two codes
   mean "good data follows", everything else means "no data". 0xfe is
   our own synthetic code for heads deselected via the head bitmask -
   deliberately chosen outside the range of real bios errors */

#define SECTOR_STATUS_OK          0     /* read clean, data present    */
#define SECTOR_STATUS_ECC         0x11  /* read after ecc correction,
                                           data present                */
#define SECTOR_STATUS_HEADSKIPPED 0xfe  /* head masked out this pass,
                                           never attempted             */

/* true when the descriptor's sector carries real data on the floppy */
#define SECTOR_HAS_DATA(s) ((s) == SECTOR_STATUS_OK || (s) == SECTOR_STATUS_ECC)

/* ---- the per disk header sector (lba 0), version 3 ----

   slimmed down hard compared to v2: everything the descriptors now
   record per sector (which lbas, which errors) is gone from here.
   what remains identifies the disk and sums up what it holds.

   layout discipline (same as before):
   - #pragma pack(1) removes padding bytes the compiler would insert
   - members must stay in this order; offsets are cumulative
   - multi byte members are little endian, implicit via x86
   - size guard typedefs below break the build if anything drifts */

#pragma pack( push, 1 )
struct FloppyHeader {
  char magic[4];               /*   0 : 'HDSV'                            */
  unsigned char version;       /*   4 : 3                                 */
  unsigned char flags;         /*   5 : bit0 = last disk of the dump      */
  unsigned long diskIndex;     /*   6 : label only, restore order is free */
  unsigned long hddTotalSectors;/*  10 : size of the source drive          */
  unsigned short descCount;    /*   14 : descriptors on this disk         */
  unsigned short dataCount;    /*   16 : data sectors on this disk        */
  unsigned short flpFailCount; /*   18 : surrendered floppy writes (goal 0)*/
  char toolId[36];             /*   20..55                                */
  unsigned char reserved[510-56]; /* 56..509 zeros                        */
  unsigned short crc;          /*  510 : crc16-ccitt over bytes 0..509    */
};
#pragma pack( pop )


void seekFloppy(unsigned int c, unsigned char h, unsigned char s);

/* current write position on the floppy. main() resets it per disk
   and rewinds it to lba 0 when writing the header sector.
   flpLBA counts floppy payload sectors consumed, used for capacity
   tracking and progress display */
extern struct ChsWithLBA floppyPosition;

/* floppy write surrenders on the current disk (retries exhausted).
   expected to stay zero; counted so the disk header can prove it */
extern unsigned int flpFailCount;

/* advances the floppy write position by one sector */
void advanceFloppyPosition(void);

/* writes one 512 byte sector at the current position with retries,
   then verifies by reading it back and comparing byte for byte.
   returns 0 = written+verified, nonzero = gave up. used for data
   sectors, descriptor blocks and the disk header alike */
unsigned char writeFloppyAuto(unsigned char* src);

/* fills a descriptor block for a batch of n consecutive hdd sectors
   starting at firstLba; statuses[i] is the read result of sector i.
   good sectors get packed data indices in encounter order - matching
   how main.c packs their buffers - bad/skipped ones get none. zeroes
   the unused tail and seals the block with its crc */
void fillDescBlock(struct DescBlock* blk, unsigned long firstLba,
                   const unsigned char* statuses, unsigned char n);

#endif
