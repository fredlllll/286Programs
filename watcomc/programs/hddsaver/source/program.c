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

static void printCmd(uint8_t cmd){
  switch(cmd){
    case CMD_START:     print("[cmd] START\r\n"); break;
    case CMD_STOP:      print("[cmd] STOP\r\n"); break;
    case CMD_SEEK:      print("[cmd] SEEK\r\n"); break;
    case CMD_PING:      print("[cmd] PING\r\n"); break;
    case CMD_STATUS:    print("[cmd] STATUS\r\n"); break;
    case CMD_HEAD_MASK: print("[cmd] HEAD_MASK\r\n"); break;
    case CMD_RETRIES:   print("[cmd] RETRIES\r\n"); break;
    case CMD_BAUD_RATE: print("[cmd] BAUD_RATE\r\n"); break;
    case RESP_ACK:      print("[resp] ACK\r\n"); break;
    case RESP_NAK:      print("[resp] NAK\r\n"); break;
    default:
      print("[cmd] unknown 0x");
      printHex(cmd);
      print("\r\n"); break;
  }
}

/* one 512 byte sector buffer. lives in dgroup like every other global */
static uint8_t sectorBuf[DATA_SIZE];

/* current state */
static uint8_t paused = 1;

/* wait for a command from the pc. returns the command byte, or 0 if
   nothing received within timeout */
static uint8_t waitForCommand(uint32_t timeoutTicks){
  uint32_t start;
  uint8_t cmd;

  start = biosTicks();
  while(1){
    cmd = recvCommand(UART_COM1);
    if(cmd) return cmd;
    if(biosTicks() - start > timeoutTicks) return 0;
  }
}

/* send one sector: header always, data only if read was successful.
   waits for ack/nak. returns 1 if ok to continue, 0 if stopped */
static uint8_t sendOneSector(void){
  uint8_t status;
  uint8_t resp;

  /* read the hdd sector */
  status = readHddResilient(sectorBuf);

  /* send header + data (if success) or header only (if failure) */
  if(isStatusSuccess(status)){
    sendSectorPacket(hddPos.lba, status, sectorBuf);
  } else {
    sendSectorHeaderOnly(hddPos.lba, status);
  }

  /* wait for ack or nak from pc */
  while(1){
    resp = waitForCommand(18 * 60); /* 60 second timeout */
    printCmd(resp);
    if(resp == RESP_ACK){
      advanceHddPosition();
      return 1; /* continue */
    }
    if(resp == RESP_NAK){
      /* retransmit same sector */
      if(isStatusSuccess(status)){
        sendSectorPacket(hddPos.lba, status, sectorBuf);
      } else {
        sendSectorHeaderOnly(hddPos.lba, status);
      }
      continue; /* wait for ack again */
    }
    if(resp == CMD_STOP){
      paused = 1;
      return 0; /* stop */
    }
    if(resp == CMD_SEEK){
      uint8_t buf[3];
      if(recvBlockTimeout(UART_COM1, buf, 3, 18)){
        uint32_t lba = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16);
        seekHdd(lba);
        sendResponse(UART_COM1, RESP_ACK);
      }
      return 0; /* stop sending, let pc re-issue start */
    }
    if(resp == CMD_HEAD_MASK){
      uint8_t tmp;
      if(recvBlockTimeout(UART_COM1, &tmp, 1, 18)){
        headMask = tmp;
        sendResponse(UART_COM1, RESP_ACK);
      }
      /* continue sending */
    }
    if(resp == CMD_RETRIES){
      uint8_t tmp;
      if(recvBlockTimeout(UART_COM1, &tmp, 1, 18)){
        hddRetries = tmp;
        sendResponse(UART_COM1, RESP_ACK);
      }
      /* continue sending */
    }
  }
}

/* ---- main loop ----
   wait for pc to send start. then stream sectors until:
   - all sectors sent (drive fully dumped)
   - pc sends stop
   - user presses esc */
static uint8_t stopRequested;

void program(void){
  uint8_t cmd;

  print("hdd saver 3.1 - serial mode\r\n");
  print("waiting for pc connection...\r\n");

  /* main loop: idle until pc sends start */
  while(1){
    cmd = waitForCommand(0); /* wait forever */
    printCmd(cmd);
    if(cmd == CMD_START){
      print("dump started\r\n");
      break;
    }
    if(cmd == CMD_PING){
      sendResponse(UART_COM1, RESP_READY);
    }
    if(cmd == CMD_STATUS){
      sendStatusReply(UART_COM1, &hddGeom, hddPos.lba, headMask, hddRetries);
    }
    if(cmd == CMD_HEAD_MASK){
      uint8_t tmp;
      if(recvBlockTimeout(UART_COM1, &tmp, 1, 18)){
        headMask = tmp;
        sendResponse(UART_COM1, RESP_ACK);
      }
    }
    if(cmd == CMD_RETRIES){
      uint8_t tmp;
      if(recvBlockTimeout(UART_COM1, &tmp, 1, 18)){
        hddRetries = tmp;
        sendResponse(UART_COM1, RESP_ACK);
      }
    }
  }

  /* streaming loop */
  stopRequested = 0;
  while(hddPos.lba < hddGeom.totalSectors && !stopRequested){
    if(escPressed()){
      stopRequested = 1;
      break;
    }
    if(!sendOneSector()){
      break;
    }
    /* progress to log */
    print(".");
  }

  if(hddPos.lba >= hddGeom.totalSectors){
    print("\r\ndump complete!\r\n");
  } else {
    print("\r\ndump paused at lba ");
    printDecLong(hddPos.lba);
    print("\r\n");
  }
}
