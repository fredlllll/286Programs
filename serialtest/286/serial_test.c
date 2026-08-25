/* serial ping-pong test: waits for "ping" on COM1, replies "pong".
   self-contained, runs barebones from floppy - no dos, no runtime. */

#include "intdef.h"

/* bios teletype: ah=0eh, int 10h prints one char at cursor */
#pragma aux printChar = \
    "mov ah, 0x0e"   \
    "int 0x10"       \
    modify [ah]      \
    parm   [al][bx]

void printChar(uint8_t ch, uint16_t pageAndColor);

static void print(const char* s){
  while(*s){
    printChar(*s, 1);
    s++;
  }
}

/* com1 = 0x3f8, bios int 14h */
#define COM1 0

static void serialInit(void){
  _asm{
    mov ah, 0x00        ; initialize serial port
    mov al, 0xE3        ; 9600 baud, no parity, 1 stop, 8 data
    mov dx, COM1
    int 0x14
  };
}

static uint8_t serialTx(uint8_t ch){
  volatile uint8_t status;
  _asm{
    mov ah, 0x01        ; send character
    mov al, ch
    mov dx, COM1
    int 0x14
    mov status, ah      ; ah bit 7 = timeout
  };
  return status;
}

static uint8_t serialRx(uint8_t* ch){
  volatile uint8_t status;
  volatile uint8_t c;
  _asm{
    mov ah, 0x02        ; receive character
    mov dx, COM1
    int 0x14
    mov status, ah
    mov c, al
  };
  *ch = c;
  return status;
}

void _cstart(void){
}

void main(void){
  uint8_t ch;
  uint8_t state;
  uint8_t i;
  static char ping[] = "ping";
  static char pong[] = "pong";

  serialInit();
  print("serial test - waiting for ping...\r\n");

  state = 0;
  while(1){
    if((serialRx(&ch) & 0x80) == 0){    /* character received */
      if(ch == ping[state]){
        state++;
        if(state == 4){
          for(i = 0; i < 4; i++){
            serialTx(pong[i]);
          }
          print("ping -> pong\r\n");
          state = 0;
        }
      } else {
        state = 0;
        if(ch == ping[0]){
          state = 1;
        }
      }
    }
  }
}
