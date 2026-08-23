/* raw bios int 13h disk access, shared by hdd and floppy paths */
#ifndef INT13_H
#define INT13_H

extern volatile unsigned char biosCylLo;
extern volatile unsigned char biosCylHiSec;
extern volatile unsigned char biosHeadMax;

void resetDiskSystem(void);
void queryBiosDrive(void);
unsigned char readFromDrive(unsigned char numSectorsToRead, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* destination);
unsigned char writeToDrive(unsigned char numSectorsToWrite, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* source);
void printInt13Status(unsigned char status);

#endif
