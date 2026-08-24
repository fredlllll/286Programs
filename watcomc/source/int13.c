#include "int13.h"
#include "print.h"

/* chs counting works exactly like a car odometer: sector 1..spt,
   then head rolls over, then cylinder. since lba numbers sectors in
   precisely that order, stepping also equals seeking */
void stepChs(struct ChsWithLBA* pos, const struct Geometry* geom){
  pos->sec += 1;
  if(pos->sec > geom->spt){       /* past last sector -> next head */
    pos->sec = 1;                 /* sectors are numbered from 1! */
    pos->head += 1;
  }
  if(pos->head >= geom->heads){   /* past last head -> next cylinder */
    pos->head = 0;
    pos->cyl += 1;
  }
  pos->lba++;
}

void resetDiskSystem(void){
  _asm{
    xor ax,ax          ; ah = 0 = "reset disk system", dl already
    int 0x13           ; holds the last used drive number
  };
}

/* builds the chs register pair for a position:
     ch gets the low 8 bits of the cylinder   -> cylinder << 8
     cl bits 6..7 get cylinder bits 8..9:
         cylinder >> 2 slides bits 8..9 down to positions 6..7,
         & 0xC0 keeps exactly those two
     cl bits 0..5 get the sector number
   or-ing the three pieces together yields the value for cx */
unsigned char readFromDrive(unsigned char numSectorsToRead, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* destination){
  volatile unsigned char status;
  unsigned short myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x2        ; "read sectors"
    mov al, numSectorsToRead
    mov cx, myCx       ; cylinder + sector, see comment above
    mov dh, head
    mov dl, driveNumber
    mov bx, destination
    int 0x13
    mov status, ah     ; on return ah holds 0 or an error code
  };
  return status;
}

unsigned char writeToDrive(unsigned char numSectorsToWrite, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* source){
  volatile unsigned char status;
  unsigned short myCx = (cylinder << 8) | ((cylinder>>2)& 0xC0) | sector;
  _asm {
    mov ah, 0x3        ; same as read but "write sectors"
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

/* translates a bios error code into text. codes are documented in
   the ibm pc technical reference / rbil; the important ones here:
     0x00 ok, 0x11 recovered by ecc (data fine), everything else bad */
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
