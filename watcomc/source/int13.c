#include "int13.h"
#include "print.h"

void resetDiskSystem(void){
  _asm{
    xor ax,ax
    int 0x13
  };
}

/* INT13 AH=08: what the bios itself thinks the drive looks like.
   ES:DI must be writable, some xt-class bioses copy a parameter table
   there. results are max indices except sectors-per-track */
static unsigned char prmTable[16];
volatile unsigned char biosCylLo;
volatile unsigned char biosCylHiSec;
volatile unsigned char biosHeadMax;

void queryBiosDrive(void){
  unsigned short diOff = (unsigned short)(unsigned int)prmTable;
  _asm{
    mov ah, 0x08
    mov dl, 0x80
    mov di, diOff
    push ds
    pop es
    int 0x13
    mov biosCylLo, ch
    mov biosCylHiSec, cl
    mov biosHeadMax, dh
  };
}

unsigned char readFromDrive(unsigned char numSectorsToRead, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* destination){
  volatile unsigned char status;
  unsigned short myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x2
    mov al, numSectorsToRead
    mov cx, myCx
    mov dh, head
    mov dl, driveNumber
    mov bx, destination
    int 0x13
    mov status, ah
  };
  return status;
}

unsigned char writeToDrive(unsigned char numSectorsToWrite, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* source){
  volatile unsigned char status;
  unsigned short myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x3
    mov al, numSectorsToWrite
    mov cx, myCx
    mov dh, head
    mov dl, driveNumber
    mov bx, source
    int 0x13
    mov status, ah
  };
  return status;
}

void printInt13Status(unsigned char status){
  switch(status){
    case 0x00:
      print("no error");
      break;
    case 0x01:
      print("bad command passed to driver");
      break;
    case 0x02:
      print("address mark not found or bad sector");
      break;
    case 0x03:
      print("diskette write protect error");
      break;
    case 0x04:
      print("sector not found");
      break;
    case 0x05:
      print("fixed disk reset failed");
      break;
    case 0x06:
      print("diskette changed or removed");
      break;
    case 0x07:
      print("bad fixed disk parameter table");
      break;
    case 0x08:
      print("DMA overrun");
      break;
    case 0x09:
      print("DMA access across 64k boundary");
      break;
    case 0x0a:
      print("bad fixed disk sector flag");
      break;
    case 0x0b:
      print("bad fixed disk cylinder");
      break;
    case 0x0c:
      print("unsupported track/invalid media");
      break;
    case 0x0d:
      print("invalid number of sectors on fixed disk format");
      break;
    case 0x0e:
      print("fixed disk controller data address mark detected");
      break;
    case 0x0f:
      print("fixed disk DMA arbitration level out of range");
      break;
    case 0x10:
      print("ECC error on disk read, data bad");
      break;
    case 0x11:
      print("data recovered by ECC");
      break;
    case 0x20:
      print("controller error");
      break;
    case 0x40:
      print("seek failure");
      break;
    case 0x80:
      print("time out, drive not ready");
      break;
    case 0xAA:
      print("fixed disk drive not ready");
      break;
    case 0xBB:
      print("fixed disk undefined error");
      break;
    case 0xCC:
      print("fixed disk write fault on selected drive");
      break;
    case 0xE0:
      print("fixed disk status error");
      break;
    case 0xFF:
      print("sense operation failed");
      break;
    default:
      print("unknown error ");
      printHex(status);
  }
}
