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
/* ack handshake robustness. a sector packet is resent up to ACK_RETRIES
   times unless its ack arrives within the per-try timeout. */
#define ACK_RETRIES 4

bool checkCommand(uint32_t timeout);

static void printCmd(uint8_t cmd)
{
  switch (cmd)
  {
  case CMD_START:
    print("\r\n[cmd] START");
    break;
  case CMD_STOP:
    print("\r\n[cmd] STOP");
    break;
  case CMD_SEEK:
    print("\r\n[cmd] SEEK");
    break;
  case CMD_PING:
    print("\r\n[cmd] PING");
    break;
  case CMD_SEND_STATUS:
    print("\r\n[cmd] SEND_STATUS");
    break;
  case CMD_STATUS:
    print("\r\n[cmd] STATUS");
    break;
  case CMD_HEAD_MASK:
    print("\r\n[cmd] HEAD_MASK");
    break;
  case CMD_RETRIES:
    print("\r\n[cmd] RETRIES");
    break;
  case CMD_RESETS:
    print("\r\n[cmd] RESETS");
    break;
  case CMD_PARK:
    print("\r\n[cmd] PARK");
    break;
  case CMD_ACK:
    print("\r\n[cmd] ACK");
    break;
  case CMD_NACK:
    print("\r\n[cmd] NACK");
    break;
  default:
    print("\r\n[cmd] unknown 0x");
    printHex(cmd);
    break;
  }
}

/* wait up to timeoutTicks for the pc's ack of packetNum. unrelated
   commands are still parsed and answered, so ping/status/stop keep
   working while a sector is being chased. returns TRUE when packetNum
   is acked, FALSE on nak or timeout (caller resends the packet) */
static bool waitForAck(uint32_t packetNum, uint32_t timeoutTicks)
{
  uint32_t start = biosTicks();
  while (biosTicks() - start < timeoutTicks)
  {
    if (!checkCommand(1 SECONDS))
    {
      continue;
    }
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
   waits for the ack, resending the same packet (same number) on nak or
   timeout instead of blocking forever. */
static void sendOneSector(void)
{
  uint8_t status;
  uint32_t packetNum;
  uint8_t attempt;

  /* head masked out: skip the sector without touching the drive or the
     wire. an untransmitted sector is indistinguishable from a missing one
     downstream (zero-filled in the assembled image), so a skip descriptor
     would only waste serial bandwidth. blaze across the whole run of
     masked sectors in one pass instead of one lba per loop() round, then
     fall straight into sending the next enabled sector. with a single
     head enabled that used to mean a lengthy poll per lba of the masked
     heads, and a whole extra loop() round for every masked span. */
  while (hddPos.lba < hddGeom.totalSectors && (headMask & (1 << hddPos.head)) == 0)
  {
    advanceHddPosition();
  }
  if(hddPos.lba >= hddGeom.totalSectors){
    return;
  }

  /* read the hdd sector */
  uartSetRts(FALSE); // signal we cant receive during hdd read
  status = readHddResilient(sectorBuf);
  uartSetRts(TRUE);

  /* send header + data (if success) or header only (if failure).
     every try waits for its exact ack; a missing ack or a nak means the
     pc didn't confirm this packet, so send it again (a duplicate row on
     the pc is harmless - assembly prefers data) before giving up. */
  for (attempt = 0; attempt < ACK_RETRIES; attempt++)
  {
    if (isStatusSuccess(status))
    {
      packetNum = sendSectorPacket(status, hddPos.lba, sectorBuf);
    }
    else
    {
      packetNum = sendSectorHeaderOnly(status, hddPos.lba);
    }
    if (waitForAck(packetNum, 2 SECONDS))
    {
      break;
    }
  }

  if (attempt == ACK_RETRIES)
  {
    print("\r\ngive up, no ack for lba ");
    printDecLong(hddPos.lba);
  }

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
    sendStatusReply(&hddGeom, hddPos.lba, headMask, hddRetries, resetsEnabled);
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
  case CMD_RESETS:
  {
    int16_t tmp = uartRxTimeout(1 SECONDS);
    if (tmp >= 0)
    {
      resetsEnabled = (uint8_t)(tmp ? 1 : 0);
      print("\r\nReceived resets ");
      printHex(resetsEnabled);
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
  case CMD_PARK:
  {
    uint8_t status = parkHeads(0x80);
    if (status == 0x00)
    {
      print("\r\nParked (BIOS AH=19h)");
    }
    else
    {
      status = recalibrateDrive(0x80);
      if (status == 0x00)
        print("\r\nParked (recalibrate AH=11h)");
      else
      {
        print("\r\nSeeking to cyl 0");
        seekHdd(0);
      }
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

  print("hdd saver 3.1 - serial mode 9600 8N1");
  print("\r\nwaiting for pc connection...");

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
      /* drain blocked pc commands, but never idle-wait for a fresh one:
         with no sector acks in flight (e.g. while blazing over masked
         heads) a 1-tick checkCommand poll here would stall every
         loop() round ~55-110ms. gating on uartRxReady keeps the drain
         instant when the fifo is empty; buffered commands still resolve
         checkCommand immediately. a stop/ping sent mid-transfer is
         already parsed inside waitForAck, so nothing is dropped. */
      while (uartRxReady() && checkCommand(1))
        ;
      loop();
      break;
    default:
      break;
    }
  }
}
