#include "keyboard.h"
#include "print.h"

unsigned char getNextKeyPress(void){
  volatile unsigned char result;
  _asm{
    mov ah, 0
    int 0x16
    mov result, al
  };
  return result;
}

static volatile unsigned char keyFlags;

unsigned char escPressed(void){
  /* non blocking: bios AH=01 sets ZF when no key waiting, LAHF grabs it */
  unsigned char k;
  _asm{
    mov ah, 1
    int 0x16
    lahf
    mov keyFlags, ah
  };
  if(keyFlags & 0x40){
    return 0;
  }
  k = getNextKeyPress();
  return (k == 27) ? 1 : 0;
}

void waitForEnter(char* prompt){
  print(prompt);
  while(1){
    if(getNextKeyPress() == 0x0D){
      return;
    }
  }
}

/* decimal input: digits appended with echo, backspace edits,
   enter submits, empty input returns default */
unsigned long decInput(char* prompt, unsigned long defVal){
  char buf[11];
  unsigned char len;
  unsigned char k;
  unsigned char i;
  unsigned long v;
  len = 0;
  print(prompt);
  print(" [");
  printDecLong(defVal);
  print("]: ");
  while(1){
    k = getNextKeyPress();
    if(k == 0x0D){
      if(len == 0){
        print("\r\n");
        return defVal;
      }
      buf[len] = 0;
      v = 0;
      for(i = 0; i < len; i++){
        v = (v << 3) + (v << 1) + (buf[i] - '0');
      }
      print("\r\n");
      return v;
    }
    if(k == 0x08){
      if(len){
        len--;
        printChar(0x08, 1);
        printChar(' ', 1);
        printChar(0x08, 1);
      }
    }
    if(k >= '0' && k <= '9'){
      if(len < 10){
        buf[len++] = k;
        printChar(k, 1);
      }
    }
  }
}
