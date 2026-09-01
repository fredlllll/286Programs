#include "uart.h"

// Circular Buffer
volatile uint8_t ringBuf[RING_BUF_SIZE];
volatile uint16_t head = 0;
volatile uint16_t tail = 0;

// Far pointer directly to Vector 0x0C in the IVT (0x0000:0x0030)
uint32_t old_com1_isr;

// Interrupt Service Routine
void __interrupt __far com1Isr(void)
{
  // Read byte to clear the UART interrupt flag
  uint8_t b = in8(RX_DATA);

  // Push into ring buffer
  uint16_t next_head = (head + 1) % RING_BUF_SIZE;
  if (next_head != tail)
  {
    ringBuf[head] = b;
    head = next_head;
  }
  else
  {
    // buffer full, throwing away input
  }

  // Acknowledge interrupt to the 8259 Master PIC (EOI)
  out8(PIC1_CMD, 0x20);
}

void initUartAndIrq(uint16_t divisor)
{
  uint8_t pic_mask;
  disableInterrupts();

  // 1. Hook IVT Vector 0x0C directly
  old_com1_isr = readFar(0x0000, 0x0030);

  // 2. Set new vector to our ISR
  writeFar(0x0000, 0x0030, &com1Isr); // TODO: does this actually give us a 32 bit address?

  // 2. Set Baud Rate (Enable DLAB = 0x80)
  out8(LCR, 0x80);
  out8(COM1_BASE + 0, (uint8_t)(divisor & 0xFF));        // Divisor Low Byte
  out8(COM1_BASE + 1, (uint8_t)((divisor >> 8) & 0xFF)); // Divisor High Byte

  // 3. Configure Frame: 8 Data Bits, 1 Stop Bit, No Parity (DLAB = 0)
  out8(LCR, 0x03);

  // 4. Enable Receiver Data Available Interrupt in UART
  out8(IER, 0x01);

  // 5. Assert OUT2 + RTS + DTR in Modem Control Register
  // Note: OUT2 (bit 3) is MANDATORY on PC architecture to pass IRQ to PIC
  out8(MCR, 0x0B);

  // 6. Flush leftover data in Rx buffer
  (void)in8(RX_DATA);

  // 7. Unmask IRQ 4 on 8259 PIC
  pic_mask = in8(PIC1_DATA);
  out8(PIC1_DATA, pic_mask & ~IRQ4_MASK);

  enableInterrupts();
}

void cleanupUartAndIrq(void)
{
  uint8_t pic_mask;
  disableInterrupts();

  // Mask IRQ 4 on PIC
  pic_mask = in8(PIC1_DATA);
  out8(PIC1_DATA, pic_mask | IRQ4_MASK);

  // Disable UART interrupts & OUT2
  out8(IER, 0x00);
  out8(MCR, 0x00);

  // Restore original IVT entry
  writeFar(0x0000, 0x0030, old_com1_isr);

  enableInterrupts();
}

int16_t uartRx(void)
{
  uint8_t b;
  if (!uartRxReady())
  {
    return -1;
  }
  b = ringBuf[tail];
  tail = (tail + 1) % RING_BUF_SIZE;
  return (int16_t)b;
}

uint8_t uartRxBlocking()
{
  uint8_t b;
  while (!uartRxReady())
    ;
  b = ringBuf[tail];
  tail = (tail + 1) % RING_BUF_SIZE;
  return b;
}

bool uartRxReady()
{
  return head != tail;
}

bool uartTxReady()
{
  return (in8(LSR) & 0x20);
}

void uartTxBlocking(uint8_t c)
{
  // Polled Transmit: Wait until LSR Bit 5 (THRE) is set
  while (!uartTxReady())
    ;
  out8(COM1_BASE, c);
}

void uartSendBlock(const uint8_t *data, uint16_t len)
{
  uint16_t i;
  for (i = 0; i < len; i++)
  {
    uartTxBlocking(data[i]);
  }
}

void uartRecvBlock(uint8_t *data, uint16_t len)
{
  uint16_t i;
  for (i = 0; i < len; i++)
  {
    data[i] = uartRxBlocking();
  }
}

uint8_t uartRecvBlockTimeout(uint8_t *buf, uint16_t len, uint32_t timeoutTicks)
{
  uint16_t i = 0;
  uint32_t start = biosTicks();
  while (i < len)
  {
    if (uartRxReady())
    {
      buf[i] = uartRx();
      i++;
    }
    else
    {
      if (biosTicks() - start > timeoutTicks)
      {
        return 0; /* timeout */
      }
    }
  }
  return 1; /* success */
}

void uartFlushRx()
{
  head = tail;
}
