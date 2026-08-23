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
   plain linear address */
#ifndef INT13_H
#define INT13_H

/* filled in by queryBiosDrive(), read by main() after the call.
   volatile because the bios changes them behind the compiler's back */
extern volatile unsigned char biosCylLo;    /* ch: cylinder low bits */
extern volatile unsigned char biosCylHiSec; /* cl: cyl high bits + spt */
extern volatile unsigned char biosHeadMax;  /* dh: max head index */

/* resets the disk controller. called between retries to clear error
   states. note: on an hdd this makes the drive recalibrate (loud
   seek to cylinder 0 and back), so it is used sparingly there */
void resetDiskSystem(void);

/* asks the bios what it thinks drive 0x80 looks like. results are
   maximum indices except for sectors per track. some ancient bioses
   scribble a parameter table into es:di, hence the dummy buffer */
void queryBiosDrive(void);

/* reads numSectorsToRead consecutive sectors starting at the given
   chs position into destination. returns 0 on success, otherwise the
   bios error code (see printInt13Status) */
unsigned char readFromDrive(unsigned char numSectorsToRead, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* destination);

/* same as readFromDrive but writes */
unsigned char writeToDrive(unsigned char numSectorsToWrite, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* source);

/* human readable text for a bios int 13h status/error code */
void printInt13Status(unsigned char status);

#endif
