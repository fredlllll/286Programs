void print_char (unsigned char inChar, unsigned short pageAndColor);
#pragma aux print_char = \
    "mov ah, 0x0e"   \
    "int 0x10"       \
    modify [ah]      \
    parm   [al][bx]
    
void print_char_call(unsigned char inChar, unsigned short pageAndColor){
  print_char(inChar,pageAndColor);
}

void print(const char* text){
  char ch;
  while (ch = *text++){
    print_char(ch, 1);
  }
}

static char* hexAlphabet = "0123456789abcdef";

void printHex(unsigned char value){
  char ch = hexAlphabet[value >> 4];
  print_char(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  print_char(ch, 1);
}
void printHexShort(unsigned short value){
  char ch = hexAlphabet[value >> 12];
  print_char(ch, 1);
  ch = hexAlphabet[(value >> 8) & 0x0F];
  print_char(ch, 1);
  ch = hexAlphabet[(value >> 4) & 0x0F];
  print_char(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  print_char(ch, 1);
}

void printHexLong(unsigned long int value){
  char ch = hexAlphabet[value >> 28];
  print_char(ch, 1);
  ch = hexAlphabet[(value >> 24) & 0x0F];
  print_char(ch, 1);
  ch = hexAlphabet[(value >> 20) & 0x0F];
  print_char(ch, 1);
  ch = hexAlphabet[(value >> 16) & 0x0F];
  print_char(ch, 1);
  ch = hexAlphabet[(value >> 12) & 0x0F];
  print_char(ch, 1);
  ch = hexAlphabet[(value >> 8) & 0x0F];
  print_char(ch, 1);
  ch = hexAlphabet[(value >> 4) & 0x0F];
  print_char(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  print_char(ch, 1);
}

static unsigned char hddHeads = 6;
static unsigned short hddTracks = 820;
static unsigned char hddSectorsPerTrack = 26;
static unsigned long int hddTotalSectors = 6*820*26; // 127920
static unsigned char hddHeadsTimesSectors = 6*26;

static unsigned char floppyHeads = 2;
static unsigned short floppyTracks = 80;
static unsigned char floppySectorsPerTrack = 18;
static unsigned long int floppyTotalSectors = 2*80*18; // 2880
static unsigned char floppyHeadsTimesSectors = 2*18;

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
    print("Enter y for yes or n for no");
  }
}

void resetDiskSystem(){
  _asm{
    xor ax,ax
    int 0x13
  };
}

unsigned char readFromDrive(unsigned char numSectorsToRead, unsigned short cylinder, unsigned char head, unsigned char sector, unsigned char driveNumber, void* destination){
  unsigned short myCx = (cylinder << 8) & ((cylinder>>2)& 0xC0) & sector;
  volatile unsigned char status;
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
      print("Errored 32 times when reading sector ");
      printHexLong(lba);
      print(" with status ");
      printHex(status);
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
  unsigned short myCx = (cylinder << 8) & ((cylinder>>2)& 0xC0) & sector;
  volatile unsigned char status;
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
      print("Errored 32 times when writing sector ");
      printHexLong(lba);
      print(" with status ");
      printHex(status);
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

unsigned long int CHSToLBA(unsigned short cylinder, unsigned char head, unsigned char sector){
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
}

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
      print_char_call(' ',1);
    }
    print_char_call(' ',1);
    print_char_call(' ',1);
    print_char_call('"',1);
    for(j = 0; j < 16; j++){
      int index = i*16+j;
      print_char_call(source[index],1);
    }
    print_char_call('"',1);
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
  unsigned char currentFloppySector = 1;
  unsigned long int currentFloppyLBA = 10;
  
  //TODO: add function to continue of different sectors than beginning
  
  for (currentHddLBA = 0; currentHddLBA < hddTotalSectors; ){
    unsigned short copiedSectors = 0;
    for(currentFloppyLBA = 10; (currentFloppyLBA < floppyTotalSectors) && (currentHddLBA < hddTotalSectors); ++currentHddLBA, ++currentFloppyLBA, ++copiedSectors){
      LBAToCHSHdd(currentHddLBA, &currentHddCylinder, &currentHddHead, &currentHddSector);
      if(!readWithRetry(1, currentHddLBA, currentHddCylinder, currentHddHead, currentHddSector, 0x80, hddSectorMemory)){
        goto leaveLoopAbort;
      }
      LBAToCHSFloppy(currentFloppyLBA, &currentFloppyCylinder, &currentFloppyHead, &currentFloppySector);
      if(!writeWithRetry(1, currentFloppyLBA, currentFloppyCylinder, currentFloppyHead, currentFloppySector, 0, hddSectorMemory)){
        goto leaveLoopAbort;
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
