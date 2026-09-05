/* hdd saver 3.1 - dumps the whole 60MB MFM/RLL drive to a modern PC
   via serial connection at 9600 baud.

   protocol:
   - pc sends commands (start, stop, seek, ping, status, config)
   - 286 sends sector headers (10 bytes) + data (512 bytes if read ok)
   - pc responds with ack (ok) or nak (crc error, retransmit)

   the entire dump is controlled from the pc side via serial commands.
   no keyboard interaction needed after startup. */

#include "definitions.h"
#include "math.h"
#include "print.h"
#include "keyboard.h"
#include "int13.h"
#include "util.h"
#include "hdd.h"
#include "protocol.h"
#include "uart.h"

/* one 512 byte sector buffer. lives in dgroup like every other global */
static uint8_t sectorBuf[512];
static bool stopRequested;
static uint8_t state;
static uint32_t lastAck;
static uint32_t lastNack;

#define STATE_PAUSE 0
#define STATE_RUN 1

bool checkCommand(uint32_t timeout);

static void printCmd(uint8_t cmd)
{
  switch (cmd)
  {
  case CMD_START:
    print("[cmd] START\r\n");
    break;
  case CMD_STOP:
    print("[cmd] STOP\r\n");
    break;
  case CMD_SEEK:
    print("[cmd] SEEK\r\n");
    break;
  case CMD_PING:
    print("[cmd] PING\r\n");
    break;
  case CMD_SEND_STATUS:
    print("[cmd] SEND_STATUS\r\n");
    break;
  case CMD_STATUS:
    print("[cmd] STATUS\r\n");
    break;
  case CMD_HEAD_MASK:
    print("[cmd] HEAD_MASK\r\n");
    break;
  case CMD_RETRIES:
    print("[cmd] RETRIES\r\n");
    break;
  case CMD_ACK:
    print("[cmd] ACK\r\n");
    break;
  case CMD_NACK:
    print("[cmd] NACK\r\n");
    break;
  default:
    print("[cmd] unknown 0x");
    printHex(cmd);
    print("\r\n");
    break;
  }
}

static bool ExpectAck(uint32_t packetNum)
{
  while (checkCommand(1 SECONDS))
  {
    if (lastAck == packetNum)
    {
      return TRUE;
    }
    if (lastNack == packetNum)
    {
      return FALSE;
    }
  }
  return FALSE;
}

/* send one sector: header always, data only if read was successful.
   waits for ack/nak. returns 1 if ok to continue, 0 if stopped */
static void sendOneSector(void)
{
  uint8_t status;
  uint32_t packetNum;

  /* read the hdd sector */
  status = readHddResilient(sectorBuf);

  /* send header + data (if success) or header only (if failure) */
  // dont do retransmission here, serial faults extremely unlikely, just blocks the program and is hard to implement properly
  // do
  {
    if (isStatusSuccess(status))
    {
      packetNum = sendSectorPacket(status, hddPos.lba, sectorBuf);
    }
    else
    {
      packetNum = sendSectorHeaderOnly(status, hddPos.lba);
    }
  }
  while (!ExpectAck(packetNum))
    ;

  advanceHddPosition();
}

static bool verifyMagic(uint32_t timeout)
{
  /* resync scanner: slide over the incoming stream looking for the
     0xAA 0x55 magic pair instead of requiring the caller to retry
     from a fixed position. a single stray or lost byte just gets
     consumed and we lock onto the next real magic, so the parser can
     no longer be shifted by a one-byte glitch permanently. */
  int16_t b;
  bool gotMagic0 = FALSE;
  uint32_t start = biosTicks();

  while (1)
  {
    if (uartRxReady())
    {
      b = uartRx();
      if (!gotMagic0)
      {
        if (b == (int16_t)HEADER_MAGIC0)
        {
          gotMagic0 = TRUE;
        }
        continue;
      }
      if (b == (int16_t)HEADER_MAGIC1)
      {
        return TRUE;
      }
      if (b == (int16_t)HEADER_MAGIC0)
      {
        continue; /* back-to-back magics: this one may start the real pair */
      }
      gotMagic0 = FALSE;
      continue;
    }
    if (biosTicks() - start > timeout)
    {
      return FALSE;
    }
  }
}

bool checkCommand(uint32_t timeout)
{
  int16_t opcode;
  uint32_t packetNumber;

  if (!verifyMagic(timeout))
  {
    return FALSE; // magic verify failed, redo the thing till we sync
  }
  opcode = uartRxTimeout(1 SECONDS);
  if (opcode < 0)
  {
    print("\r\nTimeout after receiving magic (no opcode)");
    return FALSE; // no command within timeout
  }
  printCmd(opcode);
  if (!uartRecvBlockTimeout(&packetNumber, sizeof(packetNumber), 1 SECONDS))
  {
    print("\r\nTimeout after receiving opcode (no packet number)");
    return FALSE; // timed out when reading packet number
  }
  if (opcode == CMD_ACK)
  {
    lastAck = packetNumber;
    return TRUE;
  }
  if (opcode == CMD_NACK)
  {
    lastNack = packetNumber;
    print("\r\nNack for packet ");
    printDecLong(packetNumber);
    return TRUE;
  }
  Ack(packetNumber); // we just ack all messages (except ack and nack we receive)

  switch (opcode)
  {
  case CMD_START:
    state = STATE_RUN;
    print("\r\nStarted at lba ");
    printDecLong(hddPos.lba);
    break;
  case CMD_STOP:
    state = STATE_PAUSE;
    print("\r\nPaused at lba ");
    printDecLong(hddPos.lba);
    break;
  case CMD_PING:
    sendPong();
    break;
  case CMD_SEND_STATUS:
    sendStatusReply(&hddGeom, hddPos.lba, headMask, hddRetries);
    break;
  case CMD_HEAD_MASK:
  {
    int16_t tmp = uartRxTimeout(1 SECONDS);
    if (tmp >= 0)
    {
      headMask = (uint8_t)tmp;
      print("\r\nReceived headmask ");
      printHex(headMask);
    }
  }
  break;
  case CMD_RETRIES:
  {
    int16_t tmp = uartRxTimeout(1 SECONDS);
    if (tmp >= 0)
    {
      hddRetries = (uint8_t)tmp;
      print("\r\nReceived retries ");
      printHex(hddRetries);
    }
  }
  break;
  case CMD_SEEK:
  {
    uint32_t lba;
    if (uartRecvBlockTimeout(&lba, sizeof(lba), 1 SECONDS))
    {
      seekHdd(lba);
      print("\r\nReceived seek to ");
      printDecLong(lba);
    }
  }
  break;
  }
  return TRUE;
}

void loop(void)
{
  /* streaming loop */

  if (hddPos.lba < hddGeom.totalSectors && !stopRequested)
  {
    sendOneSector();
  }

  if (hddPos.lba >= hddGeom.totalSectors)
  {
    print("\r\ndump complete!\r\n");
    state = STATE_PAUSE;
  }
}

void program(void)
{
  state = STATE_PAUSE;
  stopRequested = FALSE;

  print("hdd saver 3.1 - serial mode 9600 8N1\r\n");
  print("waiting for pc connection...\r\n");

  /* outer loop: idle → stream → stop → idle */
  while (1)
  {
    if (escPressed())
    {
      // TODO: check if we can do this with an interrupt instead so we can just poll stopRequested everywhere in code
      stopRequested = TRUE;
      break;
    }
    switch (state)
    {
    case STATE_PAUSE:
      while (checkCommand(1 SECONDS))
        ;
      break;
    case STATE_RUN:
      while (checkCommand(1))
        ;
      loop();
      break;
    default:
      break;
    }
  }
}
