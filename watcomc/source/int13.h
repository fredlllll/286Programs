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
#include "intdef.h"


/* drive geometry: how many cylinders/heads/sectors a drive has, and
   the resulting total capacity. used both for the runtime-tunable
   hdd description and the fixed floppy layout */
struct Geometry {
  uint16_t cyls;
  uint8_t heads;
  uint8_t spt;
  uint32_t totalSectors;
};

/* advances a chs position by one sector, wrapping like the bios
   expects: sector overflows into head, head into cylinder. works
   for any drive, which is why both disk modules share it */
void stepChs(struct ChsWithLBA* pos, const struct Geometry* geom);


/* resets the disk controller. called between retries to clear error
   states. note: on an hdd this makes the drive recalibrate (loud
   seek to cylinder 0 and back), so it is used sparingly there.
   takes the drive number explicitly - relying on dl still holding
   it from some earlier bios call would depend on register survival
   across compiler-generated code */
void resetDiskSystem(uint8_t driveNumber);

/* reads numSectorsToRead consecutive sectors starting at the given
   chs position into destination. returns 0 on success, otherwise the
   bios error code (see printInt13Status). destination is a far
   pointer because transfers must be doable to/from the high arena
   above the 64k line too (see program.c); ordinary near objects are
   promoted automatically since ds = 0 */
uint8_t readFromDrive(uint8_t numSectorsToRead, uint16_t cylinder, uint8_t head, uint8_t sector, uint8_t driveNumber, void __far *destination);

/* same as readFromDrive but writes */
uint8_t writeToDrive(uint8_t numSectorsToWrite, uint16_t cylinder, uint8_t head, uint8_t sector, uint8_t driveNumber, void __far *source);

/* human readable text for a bios int 13h status/error code */
void printInt13Status(uint8_t status);

#endif
