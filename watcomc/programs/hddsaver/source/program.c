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
static uint8_t sectorBuf[DATA_SIZE];
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
  case CMD_STATUS:
    print("[cmd] STATUS\r\n");
    break;
  case CMD_HEAD_MASK:
    print("[cmd] HEAD_MASK\r\n");
    break;
  case CMD_RETRIES:
    print("[cmd] RETRIES\r\n");
    break;
  case CMD_BAUD_RATE:
    print("[cmd] BAUD_RATE\r\n");
    break;
  case RESP_ACK:
    print("[resp] ACK\r\n");
    break;
  case RESP_NACK:
    print("[resp] NACK\r\n");
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

/* ---- main loop ----
   wait for pc to send start. then stream sectors until:
   - all sectors sent (drive fully dumped)
   - pc sends stop
   - user presses esc
   after stop, loop back to idle so pc can resume later */


void checkCommand(uint32_t timeout)
{
  uint8_t tmp;
  uint8_t buf[3];
  int16_t cmd;
  cmd = waitForCommand(timeout);
  if (cmd < 0)
  {
    return; // no command within timeout
  }
  printCmd(cmd);
  switch (cmd)
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
    Ack();
    break;
  case CMD_STATUS:
    sendStatusReply(&hddGeom, hddPos.lba, headMask, hddRetries);
    break;
  case CMD_HEAD_MASK:
    if (uartRecvBlockTimeout(&tmp, 1, 18))
    {
      headMask = tmp;
    }
    break;
  case CMD_RETRIES:
    if (uartRecvBlockTimeout(&tmp, 1, 18))
    {
      hddRetries = tmp;
    }
    break;
  case CMD_SEEK:
    if (uartRecvBlockTimeout(buf, 3, 1 SECONDS))
    {
      uint32_t lba = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16);
      seekHdd(lba);
    }
    break;
  }
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
      checkCommand(1 SECONDS);
      break;
    case STATE_RUN:
      checkCommand(0);
      loop();
      break;
    default:
      break;
    }
  }
}
