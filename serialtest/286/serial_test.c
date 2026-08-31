/* serial ping-pong test: waits for "ping" on COM1, replies "pong".
   self-contained, runs barebones from floppy - no dos, no runtime.
   uses direct uart port I/O for diagnostics, bios int 14h for i/o. */

#include "intdef.h"

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

static char* hexAlphabet = "0123456789ABCDEF";

static void printHex(uint8_t v){
  printChar(hexAlphabet[v >> 4], 1);
  printChar(hexAlphabet[v & 0x0F], 1);
}

static void printHexShort(uint16_t v){
  printHex(v >> 8);
  printHex(v & 0xFF);
}

/* direct uart register access. COM1 base = 0x3F8.
   bios int 14h works on most machines but gives poor diagnostics.
   direct port I/O lets us see exactly what the hardware is doing. */
#define UART_BASE 0x3F8
#define UART_RBR  (UART_BASE + 0)  /* receive buffer (read)  */
#define UART_THR  (UART_BASE + 0)  /* transmit hold  (write) */
#define UART_IER  (UART_BASE + 1)  /* interrupt enable       */
#define UART_FCR  (UART_BASE + 2)  /* fifo control (16550)   */
#define UART_LCR  (UART_BASE + 3)  /* line control           */
#define UART_MCR  (UART_BASE + 4)  /* modem control          */
#define UART_LSR  (UART_BASE + 5)  /* line status            */
#define UART_MSR  (UART_BASE + 6)  /* modem status           */
#define UART_SCR  (UART_BASE + 7)  /* scratch register       */

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

/* set baud rate via divisor latch. 9600 baud = divisor 12. */
static void uartSetBaud(uint16_t divisor){
  uint8_t lcr;
  lcr = inb(UART_LCR);
  outb(UART_LCR, lcr | 0x80);    /* set DLAB bit -> access divisor */
  outb(UART_BASE, divisor & 0xFF);       /* divisor low  */
  outb(UART_BASE + 1, (divisor >> 8) & 0xFF);  /* divisor high */
  outb(UART_LCR, lcr);            /* clear DLAB */
}

/* set line format: data bits, parity, stop bits.
   8N1 = 8 data bits, no parity, 1 stop bit = 0x03 */
static void uartSetLine(uint8_t lcr_val){
  outb(UART_LCR, lcr_val);
}

/* detect uart type by reading scratch register.
   8250:  scratch reg reads 0x00 (or random).
   16450: same as 8250.
   16550: scratch reg reads whatever was written (0xAA test).
   returns 0=unknown, 1=8250/16450, 2=16550+ */
static uint8_t uartDetect(void){
  uint8_t v;
  outb(UART_SCR, 0xAA);
  v = inb(UART_SCR);
  if(v == 0xAA) return 2;    /* 16550 or better */
  return 1;                   /* 8250 or 16450 */
}

/* read line status register.
   bit 0: data ready (byte available in RBR)
   bit 1: overrun error
   bit 2: parity error
   bit 3: framing error
   bit 4: break detect
   bit 5: THR empty (can transmit)
   bit 6: THR empty + line idle
   bit 7: error in FIFO (16550) or always 0 (8250) */
static uint8_t uartLsr(void){
  return inb(UART_LSR);
}

/* read modem status register.
   bit 0: delta CTS
   bit 1: delta DSR
   bit 2: trailing edge RI
   bit 3: delta DCD
   bit 4: CTS (clear to send)
   bit 5: DSR (data set ready)
   bit 6: RI  (ring indicator)
   bit 7: DCD (data carrier detect) */
static uint8_t uartMsr(void){
  return inb(UART_MSR);
}

/* write modem control register.
   bit 0: DTR
   bit 1: RTS
   bit 2: OUT1 (not typically used)
   bit 3: OUT2 (enables int controller on some boards)
   bit 4: loopback test mode */
static void uartSetMcr(uint8_t val){
  outb(UART_MCR, val);
}

static uint8_t uartTxReady(void){
  return uartLsr() & 0x20;    /* bit 5 = THR empty */
}

static uint8_t uartRxReady(void){
  return uartLsr() & 0x01;    /* bit 0 = data ready */
}

static void uartTx(uint8_t ch){
  while(!uartTxReady());
  outb(UART_THR, ch);
}

static uint8_t uartRx(void){
  while(!uartRxReady());
  return inb(UART_RBR);
}

/* loopback test: set MCR bits 4-5, send a byte, check if it comes back.
   this tests the uart chip itself without needing a cable. */
static uint8_t loopbackTest(void){
  uint8_t orig_mcr, test_val, received;
  uint16_t timeout;

  orig_mcr = inb(UART_MCR);
  uartSetMcr(orig_mcr | 0x10);   /* enable loopback (bit 4) */

  /* flush any leftover data */
  while(uartRxReady()) inb(UART_RBR);

  test_val = 0xA5;
  uartTx(test_val);

  /* wait for echo with timeout */
  timeout = 0;
  while(!uartRxReady()){
    timeout++;
    if(timeout > 10000) break;
  }

  uartSetMcr(orig_mcr);      /* restore original MCR */

  if(timeout > 10000) return 0;   /* timeout = no echo */
  received = inb(UART_RBR);
  return (received == test_val);   /* 1 = loopback OK */
}

void _cstart(void){
}

#pragma code_seg ( "start_segment" )
void main(void){
  uint8_t ch;
  uint8_t state;
  uint8_t i;
  uint8_t uart_type;
  uint8_t msr;
  static char ping[] = "ping";
  static char pong[] = "pong";

  /* --- phase 1: uart hardware detection --- */
  print("serial test\r\n");
  print("----------\r\n");

  uart_type = uartDetect();
  if(uart_type == 2){
    print("uart: 16550 or better\r\n");
  } else {
    print("uart: 8250/16450 (no fifo)\r\n");
  }

  /* --- phase 2: configure uart --- */
  uartSetBaud(12);            /* 115200 / 9600 = 12 */
  uartSetLine(0x03);          /* 8N1 */

  /* disable interrupts - we poll */
  outb(UART_IER, 0x00);

  /* enable DTR + RTS */
  uartSetMcr(0x03);

  print("baud: 9600  line: 8N1\r\n");

  /* --- phase 3: loopback test (no cable needed) --- */
  print("loopback test... ");
  if(loopbackTest()){
    print("OK - uart works\r\n");
  } else {
    print("FAIL - uart not responding\r\n");
    print("(cable not needed for this test)\r\n");
  }

  /* --- phase 4: modem status --- */
  print("modem status: ");
  msr = uartMsr();
  print("CTS=");
  printChar((msr & 0x10) ? '1' : '0', 1);
  print(" DSR=");
  printChar((msr & 0x20) ? '1' : '0', 1);
  print(" DCD=");
  printChar((msr & 0x80) ? '1' : '0', 1);
  print(" RI=");
  printChar((msr & 0x40) ? '1' : '0', 1);
  print("\r\n");

  if(!(msr & 0x20)){
    print("WARNING: DSR=0 - other end not ready?\r\n");
    print("(check null modem wiring)\r\n");
  }

  /* --- phase 5: ping-pong loop --- */
  print("\r\nwaiting for ping...\r\n");

  state = 0;
  while(1){
    if(uartRxReady()){
      ch = uartRx();
      printChar(ch, 1);       /* echo received char */
      if(ch == ping[state]){
        state++;
        if(state == 4){
          print("\r\n");
          for(i = 0; i < 4; i++){
            uartTx(pong[i]);
          }
          print("sent pong!\r\n");
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
#pragma code_seg ()
