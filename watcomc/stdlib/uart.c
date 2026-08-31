#include "uart.h"

#define UART_RBR(base)  (base + 0)  /* receive buffer (read)  */
#define UART_THR(base)  (base + 0)  /* transmit hold  (write) */
#define UART_IER(base)  (base + 1)  /* interrupt enable       */
#define UART_FCR(base)  (base + 2)  /* fifo control (16550)   */
#define UART_LCR(base)  (base + 3)  /* line control           */
#define UART_MCR(base)  (base + 4)  /* modem control          */
#define UART_LSR(base)  (base + 5)  /* line status            */
#define UART_SCR(base)  (base + 7)  /* scratch register       */

static uint8_t inb(uint16_t port){
  volatile uint8_t v;
  _asm{
    mov dx, port
    in  al, dx
    mov v, al
  };
  return v;
}

static void outb(uint16_t port, uint8_t val){
  _asm{
    mov dx, port
    mov al, val
    out dx, al
  };
}

void uartInit(uint16_t base, uint16_t divisor){
  /* disable interrupts */
  outb(UART_IER(base), 0x00);
  /* set baud rate */
  uartSetBaud(base, divisor);
  /* 8N1: 8 data bits, no parity, 1 stop bit = 0x03 */
  outb(UART_LCR(base), 0x03);
  /* enable DTR + RTS */
  outb(UART_MCR(base), 0x03);
}

void uartSetBaud(uint16_t base, uint16_t divisor){
  uint8_t lcr;
  lcr = inb(UART_LCR(base));
  outb(UART_LCR(base), lcr | 0x80);    /* set DLAB bit -> access divisor */
  outb(UART_THR(base), divisor & 0xFF);
  outb(UART_IER(base), (divisor >> 8) & 0xFF);
  outb(UART_LCR(base), lcr);            /* clear DLAB */
}

void uartTx(uint16_t base, uint8_t ch){
  while(!uartTxReady(base));
  outb(UART_THR(base), ch);
}

uint8_t uartRx(uint16_t base){
  while(!uartRxReady(base));
  return inb(UART_RBR(base));
}

uint8_t uartRxReady(uint16_t base){
  return inb(UART_LSR(base)) & 0x01;    /* bit 0 = data ready */
}

uint8_t uartTxReady(uint16_t base){
  return inb(UART_LSR(base)) & 0x20;    /* bit 5 = THR empty */
}

void uartSendBlock(uint16_t base, const uint8_t *data, uint16_t len){
  uint16_t i;
  for(i = 0; i < len; i++){
    uartTx(base, data[i]);
  }
}

void uartRecvBlock(uint16_t base, uint8_t *data, uint16_t len){
  uint16_t i;
  for(i = 0; i < len; i++){
    data[i] = uartRx(base);
  }
}

uint8_t uartDetect(uint16_t base){
  uint8_t v;
  outb(UART_SCR(base), 0xAA);
  v = inb(UART_SCR(base));
  if(v == 0xAA) return 2;    /* 16550 or better */
  return 1;                   /* 8250 or 16450 */
}
