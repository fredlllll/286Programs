/* bios int 13h disk access -- floppy only.

   int 13h is the bios disk service. a call selects one function via
   the ah register:
     ah=00  reset the disk controller
     ah=02  read sectors
     ah=03  write sectors
     ah=05  format a track (writes sector address fields)

   positions are given in chs form, packed into registers like this:
     ch = cylinder low 8 bits
     cl = bit 6..7: cylinder bits 8..9
          bit 0..5: sector number (1 based!)
     dh = head
     dl = drive number: 0x00 = first floppy
   data is transferred through es:bx */
#ifndef INT13_H
#define INT13_H
#include "intdef.h"

void resetDiskSystem(uint8_t driveNumber);

uint8_t readFromDrive(uint8_t numSectors, uint16_t cylinder,
    uint8_t head, uint8_t sector, uint8_t driveNumber, void __far *dest);

uint8_t writeToDrive(uint8_t numSectors, uint16_t cylinder,
    uint8_t head, uint8_t sector, uint8_t driveNumber, void __far *src);

uint8_t formatTrack(uint8_t numSectors, uint16_t cylinder,
    uint8_t head, uint8_t driveNumber, void __far *sectorIdBuf);

void printInt13Status(uint8_t status);

#endif
