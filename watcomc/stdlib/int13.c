#include "int13.h"
#include "print.h"

void resetDiskSystem(uint8_t driveNumber){
  _asm{
    xor ax,ax
    mov dl, driveNumber
    int 0x13
  };
}

uint8_t readFromDrive(uint8_t numSectorsToRead, uint16_t cylinder,
    uint8_t head, uint8_t sector, uint8_t driveNumber, void __far *destination){
  volatile uint8_t status;
  uint16_t myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x2
    mov al, numSectorsToRead
    mov cx, myCx
    mov dh, head
    mov dl, driveNumber
    mov bx, word ptr [destination]
    mov es, word ptr [destination + 2]
    int 0x13
    mov status, ah
  };
  return status;
}

uint8_t writeToDrive(uint8_t numSectorsToWrite, uint16_t cylinder,
    uint8_t head, uint8_t sector, uint8_t driveNumber, void __far *source){
  volatile uint8_t status;
  uint16_t myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x3
    mov al, numSectorsToWrite
    mov cx, myCx
    mov dh, head
    mov dl, driveNumber
    mov bx, word ptr [source]
    mov es, word ptr [source + 2]
    int 0x13
    mov status, ah
  };
  return status;
}

/* format a track: writes sector address marks (id fields) for each
   sector on the track. es:bx points to a buffer of 4-byte entries:
   {cylinder, head, sector_number, size_code}. for 512 byte sectors
   size_code = 2 (2^9 = 512). al = sectors per track. returns bios
   status (0 = ok) */
uint8_t formatTrack(uint8_t numSectors, uint16_t cylinder,
    uint8_t head, uint8_t driveNumber, void __far *sectorIdBuf){
  volatile uint8_t status;
  uint16_t myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0);
  _asm {
    mov ah, 0x05
    mov al, numSectors
    mov cx, myCx
    mov dh, head
    mov dl, driveNumber
    mov bx, word ptr [sectorIdBuf]
    mov es, word ptr [sectorIdBuf + 2]
    int 0x13
    mov status, ah
  };
  return status;
}

void printInt13Status(uint8_t status){
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
