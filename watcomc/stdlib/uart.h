/* uart driver for 8250/16450/16550 serial ports.
   polling mode, no interrupts. COM1 = 0x3F8, COM2 = 0x2F8 */
#ifndef UART_H
#define UART_H
#include "intdef.h"

#define UART_COM1 0x3F8
#define UART_COM2 0x2F8

/* initialize uart: set baud rate divisor, 8N1, disable interrupts,
   enable DTR/RTS. divisor = 115200 / baud_rate (e.g. 12 = 9600) */
void uartInit(uint16_t base, uint16_t divisor);

/* set baud rate via divisor latch. call after uartInit to change speed */
void uartSetBaud(uint16_t base, uint16_t divisor);

/* blocking transmit: waits for THR empty, then sends byte */
void uartTx(uint16_t base, uint8_t ch);

/* blocking receive: waits for data ready, then returns byte */
uint8_t uartRx(uint16_t base);

/* poll: returns 1 if a byte is available in the receive buffer */
uint8_t uartRxReady(uint16_t base);

/* poll: returns 1 if THR is empty (can transmit) */
uint8_t uartTxReady(uint16_t base);

/* send a block of bytes */
void uartSendBlock(uint16_t base, const uint8_t *data, uint16_t len);

/* receive a block of bytes */
void uartRecvBlock(uint16_t base, uint8_t *data, uint16_t len);

/* detect uart type: 0=unknown, 1=8250/16450, 2=16550+ */
uint8_t uartDetect(uint16_t base);

#endif
