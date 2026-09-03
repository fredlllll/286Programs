/* uart driver for 8250/16450/16550 serial ports.
   polling mode, no interrupts. COM1 = 0x3F8, COM2 = 0x2F8 */
#ifndef UART_H
#define UART_H
#include "intdef.h"
#include "inout.h"
#include "int.h"
#include "util.h"

#define COM1_BASE   0x3F8
#define RX_DATA     (COM1_BASE + 0)
#define IER         (COM1_BASE + 1)
#define LCR         (COM1_BASE + 3)
#define MCR         (COM1_BASE + 4)
#define LSR         (COM1_BASE + 5)

#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define IRQ4_MASK   0x10  // Bit 4

#define RING_BUF_SIZE 1024

/* initialize uart: set baud rate divisor, 8N1, disable interrupts,
   enable DTR/RTS. divisor = 115200 / baud_rate (e.g. 12 = 9600) */
void initUartAndIrq(uint16_t divisor);

void cleanupUartAndIrq(void);

/* blocking transmit: waits for THR empty, then sends byte */
void uartTxBlocking(uint8_t c);

/* nonblocking receive: returns -1 if no data present */
int16_t uartRx(void);
/* blocking receive with timeout, returns -1 if timed out*/
int16_t uartRxTimeout(uint32_t timeoutTicks);
/* blocking receive: waits till data present and returns byte */
uint8_t uartRxBlocking();

/* poll: returns 1 if a byte is available in the receive buffer */
bool uartRxReady();

/* poll: returns 1 if THR is empty (can transmit) */
bool uartTxReady();

/* send a block of bytes */
void uartSendBlock(const void *data, uint16_t len);

/* receive a block of bytes */
void uartRecvBlock(void *data, uint16_t len);

bool uartRecvBlockTimeout(void *buf, uint16_t len, uint32_t timeoutTicks);

/* flush receive buffer: discard all pending bytes */
void uartFlushRx();

#endif
