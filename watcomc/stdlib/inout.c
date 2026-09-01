#include "inout.h"

uint8_t in8(uint16_t port){
  volatile uint8_t v;
  _asm{
    mov dx, port
    in  al, dx
    mov v, al
  };
  return v;
}

uint16_t in16(uint16_t port){
  volatile uint16_t v;
  _asm{
    mov dx, port
    in  ax, dx
    mov v, ax
  };
  return v;
}

void out8(uint16_t port, uint8_t val){
  _asm{
    mov dx, port
    mov al, val
    out dx, al
  };
}

void out16(uint16_t port, uint16_t val){
  _asm{
    mov dx, port
    mov ax, val
    out dx, ax
  };
}
