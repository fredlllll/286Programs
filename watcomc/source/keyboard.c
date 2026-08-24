#include "keyboard.h"
#include "print.h"

uint8_t getNextKeyPress(void){
  volatile uint8_t result;
  _asm{
    mov ah, 0          ; bios "read key, wait if needed"
    int 0x16
    mov result, al     ; al = ascii code of the key
  };
  return result;
}

static volatile uint8_t keyFlags;

/* how the non blocking check works: int 16h ah=01 reports "key
   waiting?" in the zero flag (zf=1 -> none). lahf copies the cpu
   flags into ah, so bit 6 of keyFlags is zf. if a key IS waiting it
   is pulled with the blocking call and compared against esc's
   ascii code (27). any other queued key is swallowed */
uint8_t escPressed(void){
  uint8_t k;
  _asm{
    mov ah, 1          ; bios "key available?"
    int 0x16
    lahf               ; load flags into ah, bit 6 = zero flag
    mov keyFlags, ah
  };
  if(keyFlags & 0x40){ /* 0x40 = bit 6 = zf set = buffer empty */
    return 0;
  }
  k = getNextKeyPress();
  return (k == 27) ? 1 : 0;
}

void waitForEnter(char* prompt){
  print(prompt);
  while(1){
    if(getNextKeyPress() == 0x0D){   /* 0x0d = carriage return */
      return;
    }
  }
}

/* decimal input editor.
   the typed digits collect in buf; on enter they are folded into a
   number with v = v*10 + digit. the expression (v << 3) + (v << 1)
   is v*8 + v*2 = v*10, done with shifts because it is cheaper on an
   80286 and avoids the watcom multiply helper */
uint32_t decInput(char* prompt, uint32_t defVal){
  char buf[11];        /* max 10 digits + terminator */
  uint8_t len;
  uint8_t k;
  uint8_t i;
  uint32_t v;
  len = 0;
  print(prompt);
  print(" [");
  printDecLong(defVal);
  print("]: ");
  while(1){
    k = getNextKeyPress();
    if(k == 0x0D){                 /* enter = submit */
      if(len == 0){
        print("\r\n");
        return defVal;             /* empty input keeps default */
      }
      buf[len] = 0;                /* terminate, unused but tidy */
      v = 0;
      for(i = 0; i < len; i++){
        v = (v << 3) + (v << 1) + (buf[i] - '0');
      }
      print("\r\n");
      return v;
    }
    if(k == 0x08){                 /* backspace = erase one digit */
      if(len){
        len--;
        printChar(0x08, 1);        /* move cursor left ... */
        printChar(' ', 1);         /* ... wipe the character ... */
        printChar(0x08, 1);        /* ... and move back again */
      }
    }
    if(k >= '0' && k <= '9'){      /* accept plain digits only */
      if(len < 10){
        buf[len++] = k;
        printChar(k, 1);           /* echo */
      }
    }
  }
}
