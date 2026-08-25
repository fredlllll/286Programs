#include "int13.h"
#include "print.h"

void resetDiskSystem(uint8_t driveNumber){
  _asm{
    xor ax,ax
    mov dl, driveNumber
    int 0x13
  };
}

uint8_t readFromDrive(uint8_t numSectors, uint16_t cylinder,
    uint8_t head, uint8_t sector, uint8_t driveNumber, void __far *dest){
  volatile uint8_t status;
  uint16_t myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x2
    mov al, numSectors
    mov cx, myCx
    mov dh, head
    mov dl, driveNumber
    mov bx, word ptr [dest]
    mov es, word ptr [dest + 2]
    int 0x13
    mov status, ah
  };
  return status;
}

uint8_t writeToDrive(uint8_t numSectors, uint16_t cylinder,
    uint8_t head, uint8_t sector, uint8_t driveNumber, void __far *src){
  volatile uint8_t status;
  uint16_t myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x3
    mov al, numSectors
    mov cx, myCx
    mov dh, head
    mov dl, driveNumber
    mov bx, word ptr [src]
    mov es, word ptr [src + 2]
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
    mov ah, 0x05       ; "format track"
    mov al, numSectors
    mov cx, myCx       ; cylinder in ch:cl bits 6-7
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
      print("ok");
      break;
    case 0x01:
      print("bad command");
      break;
    case 0x02:
      print("addr mark not found");
      break;
    case 0x03:
      print("write protect");
      break;
    case 0x04:
      print("sector not found");
      break;
    case 0x08:
      print("DMA overrun");
      break;
    case 0x09:
      print("DMA boundary");
      break;
    case 0x0C:
      print("unsupported track");
      break;
    case 0x10:
      print("ECC error");
      break;
    case 0x11:
      print("ECC recovered");
      break;
    case 0x20:
      print("controller error");
      break;
    case 0x40:
      print("seek failure");
      break;
    case 0x80:
      print("timeout");
      break;
    case 0xAA:
      print("drive not ready");
      break;
    default:
      print("error 0x");
      printHex(status);
  }
}
