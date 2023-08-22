//#define DEBUG

void printChar (unsigned char inChar, unsigned short pageAndColor);
#pragma aux printChar = \
    "mov ah, 0x0e"   \
    "int 0x10"       \
    modify [ah]      \
    parm   [al][bx]
    
void printChar_call(unsigned char inChar, unsigned short pageAndColor){
  printChar(inChar,pageAndColor);
}

void print(const char* text){
  char ch;
  while (ch = *text++){
    printChar(ch, 1);
  }
}

static char* hexAlphabet = "0123456789abcdef";

void printHex(unsigned char value){
  char ch = hexAlphabet[value >> 4];
  printChar(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  printChar(ch, 1);
}
void printHexShort(unsigned short value){
  char ch = hexAlphabet[value >> 12];
  printChar(ch, 1);
  ch = hexAlphabet[(value >> 8) & 0x0F];
  printChar(ch, 1);
  ch = hexAlphabet[(value >> 4) & 0x0F];
  printChar(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  printChar(ch, 1);
}

void printHexLong(unsigned long int value){
  char ch = hexAlphabet[value >> 28];
  printChar(ch, 1);
  ch = hexAlphabet[(value >> 24) & 0x0F];
  printChar(ch, 1);
  ch = hexAlphabet[(value >> 20) & 0x0F];
  printChar(ch, 1);
  ch = hexAlphabet[(value >> 16) & 0x0F];
  printChar(ch, 1);
  ch = hexAlphabet[(value >> 12) & 0x0F];
  printChar(ch, 1);
  ch = hexAlphabet[(value >> 8) & 0x0F];
  printChar(ch, 1);
  ch = hexAlphabet[(value >> 4) & 0x0F];
  printChar(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  printChar(ch, 1);
}

#define HDD_HEADS 6
#define HDD_TRACKS 820
#define HDD_SECTORS_PER_TRACK 26
//#define HDD_TOTAL_SECTORS (HDD_HEADS*HDD_TRACKS*HDD_SECTORS_PER_TRACK)
#define HDD_TOTAL_SECTORS (6*820*26)

#define FLOPPY_HEADS 2
#define FLOPPY_TRACKS 80
#define FLOPPY_SECTORS_PER_TRACK 18
//#define FLOPPY_TOTAL_SECTORS (FLOPPY_HEADS*FLOPPY_TRACKS*FLOPPY_SECTORS_PER_TRACK)
#define FLOPPY_TOTAL_SECTORS (2*80*18)

#ifdef DEBUG
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
      print("fixed disk controlled data address mark detected");
      break;
    case 0x0f:
      print("fixed disk DMA arbitration level out of range");
      break;
    case 0x10:
      print("ECC/CRC error on disk read");
      break;
    case 0x11:
      print("recoverable fixed disk data error, data fixed by ECC");
      break;
    case 0x20:
      print("controller error (NEC for floppies)");
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
      print("fixed disk status error/Error reg = 0");
      break;
    case 0xFF:
      print("sense operation failed");
      break;
    default:
      print("unknown error ");
      printHex(status);
  }
  //print("\r\n");
}
#endif

unsigned char getNextKeyPress(){
  volatile unsigned char result;
  _asm{
    mov ah, 0
    int 0x16
    mov result, al
  };
  return result;
}

unsigned short waitForYesNo(){
  while(1){
    unsigned char key = getNextKeyPress();
    if(key == 'y'){
      return 1;
    }else if(key == 'n'){
      return 0;
    }
    print("\r\nPress y for yes or n for no");
  }
}

void resetDiskSystem(){
  _asm{
    xor ax,ax
    int 0x13
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

unsigned char readWithRetry(unsigned char numSectorsToRead, unsigned long int lba, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* destination){
  int i;
  unsigned char status;
  while(1){
    status = readFromDrive(numSectorsToRead, cylinder, head, sector, driveNumber, destination);
    if(status == 0){
      return 1; //successful read
    }
    if(i >= 31){
      #ifdef DEBUG
      print("Errored 32 times when reading CHS ");
      printHexShort(cylinder);
      printChar_call(' ',1);
      printHex(head);
      printChar_call(' ',1);
      printHex(sector);
      print(" (LBA ");
      printHexLong(lba);
      print(") on drive ");
      printHex(driveNumber);
      print(" with error: ");
      printInt13Status(status);
      #else
      printHex(status);
      #endif
      print("\r\nRetry?(y/n)");
      if(waitForYesNo()){
        i = -1;
      }else{
        print("\r\nAborting...\r\n");
        return 0;
      }
    }
    resetDiskSystem();
    ++i;
  }
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

unsigned char writeWithRetry(unsigned char numSectorsToWrite, unsigned long int lba, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* source){
  int i;
  unsigned char status;
  while(1){
    status = writeToDrive(numSectorsToWrite, cylinder, head, sector, driveNumber, source);
    if(status == 0){
      return 1; //successful write
    }
    if(i >= 31){
      #ifdef DEBUG
      print("Errored 32 times when writing CHS ");
      printHexShort(cylinder);
      printChar_call(' ',1);
      printHex(head);
      printChar_call(' ',1);
      printHex(sector);
      print(" (LBA ");
      printHexLong(lba);
      print(") on drive ");
      printHex(driveNumber);
      print(" with error ");
      printInt13Status(status);
      #else
      printHex(status);
      #endif
      print("\r\nRetry?(y/n)");
      if(waitForYesNo()){
        i = -1;
      }else{
        print("\r\nAborting...\r\n");
        return 0;
      }
    }
    resetDiskSystem();
    ++i;
  }
}

unsigned char advanceCHSHdd(unsigned short *cylinder, unsigned char *head, unsigned char *sector){
  *sector += 1;
  if(*sector > HDD_SECTORS_PER_TRACK){
    *sector = 1;
    *head += 1;
  }
  if(*head >= HDD_HEADS){
    *head = 0;
    *cylinder += 1;
  }
  return *cylinder < HDD_TRACKS; //return false if we have reached the end
}

unsigned char advanceCHSFloppy(unsigned short *cylinder, unsigned char *head, unsigned char *sector){
  *sector += 1;
  if(*sector > FLOPPY_SECTORS_PER_TRACK){
    *sector = 1;
    *head += 1;
  }
  if(*head >= FLOPPY_HEADS){
    *head = 0;
    *cylinder += 1;
  }
  return *cylinder < FLOPPY_TRACKS; //return false if we have reached the end
}

/*unsigned long int CHSToLBA(unsigned short cylinder, unsigned char head, unsigned char sector){
  return ((cylinder * hddHeads + head) * hddSectorsPerTrack) + sector - 1;
}

void LBAToCHSHdd(unsigned long int lba, unsigned short *cylinder, unsigned char *head, unsigned char *sector){
  unsigned int temp;
  *cylinder = lba / hddHeadsTimesSectors;
  temp = lba % hddHeadsTimesSectors;
  *head = temp / hddSectorsPerTrack;
  *sector = temp % hddSectorsPerTrack +1;
}

void LBAToCHSFloppy(unsigned long int lba, unsigned short *cylinder, unsigned char *head, unsigned char *sector){
  unsigned int temp;
  *cylinder = lba / floppyHeadsTimesSectors;
  temp = lba % floppyHeadsTimesSectors;
  *head = temp / floppySectorsPerTrack;
  *sector = temp % floppySectorsPerTrack + 1;
}*/

void waitForEnter(){
  while(1){
    if(getNextKeyPress() == 0x0D){
      return;
    }
  }
}

void halt(){
  while(1){
    _asm{
      hlt
    };
  }
}

void prettyPrint(unsigned char* source){
  //prints 256 bytes pretty
  int i;
  int j;
  for(i = 0; i < 256/16; i++){
    for(j = 0; j< 16; j++){
      int index = i*16+j;
      printHex(source[index]);
      printChar_call(' ',1);
    }
    printChar_call(' ',1);
    printChar_call(' ',1);
    printChar_call('"',1);
    for(j = 0; j < 16; j++){
      int index = i*16+j;
      printChar_call(source[index],1);
    }
    printChar_call('"',1);
    print("\r\n");
  }
}

void _cstart(void){
  //shut up linker who cant find _cstart_ that it doesnt need
}

#pragma code_seg ( "start_segment" )
void main(void){
  unsigned char hddSectorMemory[512]; //put on stack so we dont run into trouble with our program growing into fixed memory
  
  unsigned short currentHddCylinder = 0;
  unsigned char currentHddHead = 0;
  unsigned char currentHddSector = 1;
  unsigned long int currentHddLBA = 0;
  
  unsigned short currentFloppyCylinder = 0;
  unsigned char currentFloppyHead = 0;
  unsigned char currentFloppySector = 11;
  unsigned long int currentFloppyLBA = 10;
  
  unsigned char hddAdvanced;
  unsigned char floppyAdvanced;
  
  //TODO: add function to continue of different CHS than beginning
  
  for (currentHddLBA = 0; currentHddLBA < HDD_TOTAL_SECTORS; ){
    unsigned short copiedSectors = 0;
    for(currentFloppyLBA = 10; (currentFloppyLBA < FLOPPY_TOTAL_SECTORS) && (currentHddLBA < HDD_TOTAL_SECTORS); ++currentHddLBA, ++currentFloppyLBA, ++copiedSectors){
      if(!readWithRetry(1, currentHddLBA, currentHddCylinder, currentHddHead, currentHddSector, 0x80, hddSectorMemory)){
        goto leaveLoopAbort;
      }
      if(!writeWithRetry(1, currentFloppyLBA, currentFloppyCylinder, currentFloppyHead, currentFloppySector, 0, hddSectorMemory)){
        goto leaveLoopAbort;
      }
      
      hddAdvanced = advanceCHSHdd(&currentHddCylinder, &currentHddHead, &currentHddSector);
      floppyAdvanced = advanceCHSFloppy(&currentFloppyCylinder, &currentFloppyHead, &currentFloppySector);
      if(hddAdvanced == 0){
        //end of hdd, exit should be handled by the for loop?
        print("End of hdd");
      }
      if(floppyAdvanced == 0){
        //end of floppy, exit should also be handled by for?
        currentFloppyCylinder = 0;
        currentFloppyHead = 0;
        currentFloppySector = 1;
        print("End of floppy");
      }
    }
    print("Copied Sectors: ");
    printHexShort(copiedSectors);
    print("\r\nWaiting For Enter to begin next transfer");
    waitForEnter();
  }
  print("Successfully transfered entire hdd");
  halt();
  leaveLoopAbort:
  print("Aborted transfer at sector ");
  printHexLong(currentHddLBA);
  halt();
}
#pragma code_seg ()
