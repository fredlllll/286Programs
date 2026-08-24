/* raw bios int 13h disk access, shared by hdd and floppy paths.

   int 13h is the bios disk service. a call selects one function via
   the ah register:
     ah=08  query drive geometry
     ah=02  read sectors
     ah=03  write sectors
     ah=00  reset the disk controller (used to recover from errors)

   positions are given in chs form, packed into registers like this:
     ch = cylinder low 8 bits
     cl = bit 6..7: cylinder bits 8..9   (yes, really split!)
          bit 0..5: sector number (1 based!)
     dh = head
     dl = drive number: 0x80 = first hard disk, 0 = first floppy
   data is transferred through es:bx, a "segment:offset" pointer pair.
   we run with all segment registers = 0, so es:bx is effectively a
   plain linear address

   this header also owns the chs addressing types shared by the hdd
   and floppy modules, since chs is exactly how int 13h sees disks */
#ifndef INT13_H
#define INT13_H
#include "chs.h"


/* drive geometry: how many cylinders/heads/sectors a drive has, and
   the resulting total capacity. used both for the runtime-tunable
   hdd description and the fixed floppy layout */
struct Geometry {
  unsigned int cyls;
  unsigned char heads;
  unsigned char spt;
  unsigned long totalSectors;
};

/* advances a chs position by one sector, wrapping like the bios
   expects: sector overflows into head, head into cylinder. works
   for any drive, which is why both disk modules share it */
void stepChs(struct ChsWithLBA* pos, const struct Geometry* geom);


/* resets the disk controller. called between retries to clear error
   states. note: on an hdd this makes the drive recalibrate (loud
   seek to cylinder 0 and back), so it is used sparingly there */
void resetDiskSystem(void);

/* reads numSectorsToRead consecutive sectors starting at the given
   chs position into destination. returns 0 on success, otherwise the
   bios error code (see printInt13Status) */
unsigned char readFromDrive(unsigned char numSectorsToRead, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* destination);

/* same as readFromDrive but writes */
unsigned char writeToDrive(unsigned char numSectorsToWrite, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* source);

/* human readable text for a bios int 13h status/error code */
void printInt13Status(unsigned char status);

#endif
