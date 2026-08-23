/* floppy side: chs stepping, verified sector writing and the on-disk
   header format */
#ifndef FLOPPY_H
#define FLOPPY_H

#include "definitions.h"
#include "int13.h"

/* ---- the per disk header sector (lba 0) ----
   this struct describes EXACTLY what lives in the first 512 byte
   sector of every floppy: identity, resume info, checksums and the
   two bad lists. the pc side that re-reads the disks expects this
   layout, so any change here must happen there too.

   how it stays exact:
   - #pragma pack(1) removes all padding bytes the compiler would
     otherwise insert between members to align them - without it,
     "unsigned long after two chars" would silently shift by 2 bytes
     and corrupt the format
   - members must stay in this order; each one's offset is fixed by
     the ones before it
   - multi byte members are stored little endian (least significant
     byte first). this happens implicitly because we write the struct
     directly and x86 is little endian - convenient, but it means the
     reader side has to expect little endian too

   the size check below the struct breaks the build if the layout
   ever drifts from the documented 502 bytes */

#pragma pack( push, 1 )
struct FloppyHeader {
  char magic[4];                          /*   0 : 'HDSV'          */
  unsigned char version;                  /*   4 : currently 2     */
  unsigned char flags;                    /*   5 : bit0 = last disk */
  unsigned long diskIndex;                /*   6                   */
  unsigned long startLBA;                 /*  10 : first hdd lba    */
  unsigned short sectorCount;             /*  14 : payload sectors  */
  unsigned short payloadCrc;              /*  16 : crc over payload */
  unsigned short diskBadCount;            /*  18                    */
  unsigned long hddTotalSectors;          /*  20                    */
  unsigned long badLbas[MAX_BAD];         /*  24..215               */
  char toolId[38];                        /* 216..253               */
  unsigned short badFlpCount;             /* 254                    */
  unsigned short badFlpOffsets[MAX_BAD_FLP]; /* 256..315            */
  unsigned char reserved[500-316];        /* 316..499 zeros         */
  unsigned short crc;                     /* 500 : crc over 0..499  */
};
#pragma pack( pop )

/* compile-time guard: negative array size = build error if the
   packed struct is not exactly as documented */
typedef char floppysizecheck[(sizeof(struct FloppyHeader) == 502) ? 1 : -1];

/* current write position on the floppy. main() resets it per disk
   and rewinds it to lba 0 when writing the header sector.
   flpLBA counts payload sectors written, used for progress only */
extern struct Chs flpPos;
extern unsigned long flpLBA;

/* payload offsets (in sectors, relative to payload start) of sectors
   that could not be written+verified; consumed by buildHeader().
   the pc side uses them to know which parts of the restored image
   are BADFILL rather than real data */
extern unsigned int badFlpOffsets[MAX_BAD_FLP];
extern unsigned int badFlpCount;

/* advances the floppy write position by one sector */
void advanceCHSFloppy(void);

/* writes one 512 byte sector at the current position with retries,
   then verifies by reading it back and comparing byte for byte.
   returns 0 = written+verified, 1 = gave up: buffer replaced with
   BADFILL and its payload offset logged.
   payloadOffset = this sector's index in the dump stream, needed
   only for logging when things go wrong */
unsigned char writeFloppyAuto(unsigned char* src, unsigned int payloadOffset);

#endif
