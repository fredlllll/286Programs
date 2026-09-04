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

#define STATE_PAUSE 0
#define STATE_RUN 1

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

/* send one sector: header always, data only if read was successful.
   waits for ack/nak. returns 1 if ok to continue, 0 if stopped */
static void sendOneSector(void)
{
  uint8_t status;
  uint8_t resp;

  /* read the hdd sector */
  status = readHddResilient(sectorBuf);

  /* send header + data (if success) or header only (if failure) */
  do
  {
    if (isStatusSuccess(status))
    {
      sendSectorPacket(hddPos.lba, status, sectorBuf);
    }
    else
    {
      sendSectorHeaderOnly(hddPos.lba, status);
    }
  } while (!ExpectAck());

  advanceHddPosition();
}

static bool verifyMagic(uint32_t timeout)
{
  uint8_t b;
  b = uartRxTimeout(timeout);
  if (b != HEADER_MAGIC0)
  {
    return FALSE;
  }
  b = uartRxTimeout(timeout);
  if (b != HEADER_MAGIC1)
  {
    return FALSE;
  }
  return TRUE;
}

bool checkCommand(uint32_t timeout)
{
  uint8_t tmp;
  uint8_t buf[3];
  int16_t opcode;
  uint32_t packetNumber;

  if (!verifyMagic(timeout))
  {
    return FALSE; // magic verify failed, redo the thing till we sync
  }
  opcode = uartRxTimeout(1 SECONDS);
  if (opcode < 0)
  {
    return FALSE; // no command within timeout
  }
  printCmd(opcode);
  uartRecvBlockTimeout(&packetNumber, 4, 1 SECONDS);
  if (opcode == CMD_ACK)
  {
    return TRUE; // ignore
  }
  if (opcode == CMD_NACK)
  {
    print("\r\nNack for packet ");
    printDecLong(packetNumber);
    return TRUE; // ignore, retransmissions are too hard
  }
  Ack(packetNumber); // we just ack all messages (not ack and nack though)

  switch (opcode)
  {
  case CMD_START:
    state = STATE_RUN;
    break;
  case CMD_STOP:
    state = STATE_PAUSE;
    print("\r\ndump paused at lba ");
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
    }
  }
  break;
  case CMD_RETRIES:
  {
    int16_t tmp = uartRxTimeout(1 SECONDS);
    if (tmp >= 0)
    {
      hddRetries = (uint8_t)tmp;
    }
  }
  break;
  case CMD_SEEK:
  {
    uint32_t lba;
    if (uartRecvBlockTimeout(&lba, sizeof(lba), 1 SECONDS))
    {
      seekHdd(lba);
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
  int16_t cmd;

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
