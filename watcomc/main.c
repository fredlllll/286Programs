void print_char (unsigned char inChar, unsigned short pageAndColor);
#pragma aux print_char = \
    "mov ah, 0x0e"   \
    "int 0x10"       \
    modify [ah]      \
    parm   [al][bx]

void print(const char* text){
  char ch;
  while (ch = *text++){
    print_char(ch, 1);
  }
}

static char* hexAlphabet = "0123456789abcdef";

void printHex(unsigned char value){
  char ch = hexAlphabet[value >>4];
  print_char(ch, 1);
  ch = hexAlphabet[value & 0x0F];
  print_char(ch, 1);
}

static unsigned char hddHeads = 6;
static unsigned short hddTracks = 820;
static unsigned char hddSectorsPerTrack = 26;
static unsigned long int hddTotalSectors = 6*820*26;

unsigned char loadFromHdd(const void* destination){
  _asm {
    mov ah, 0x2 
    mov al, 0x1 
    mov ch, 0x0 
    mov cl, 0x1 
    mov dh, 0x0 
    mov dl, 0x80
    mov bx, destination
    int 0x13
  };
}

void _cstart(void){
  //shut up linker who cant find _cstart_ that it doesnt need
}
#pragma code_seg ( "start_segment" )
void main(void){
  unsigned char * hddSectorMemory = (unsigned char*) 0xFE00;
  int i;
  unsigned int result = loadFromHdd(hddSectorMemory);
  print("Read result: ");
  printHex(result >> 8);
  printHex(result & 0xF);
  print("\r\n");
  
  for(i = 0; i < 512; i++){
    printHex(hddSectorMemory[i]);
    print_char(' ',1);
  }

  print("\r\nthis is a test");
  while (1){
    __asm { hlt };
  }
}
#pragma code_seg ()
