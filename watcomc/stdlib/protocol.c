#include "protocol.h"
#include "uart.h"
#include "util.h"
#include "math.h"

#define UART_BASE UART_COM1

void sendSectorHeaderOnly(uint32_t lba, uint8_t status){
  struct SectorHeader hdr;
  hdr.magic0 = HEADER_MAGIC0;
  hdr.magic1 = HEADER_MAGIC1;
  poke24(hdr.lba, lba);
  hdr.status = status;
  hdr.dataCRC[0] = 0;
  hdr.dataCRC[1] = 0;
  /* CRC over bytes 0..7 (magic + lba + status + dataCRC) */
  {
    uint16_t hcrc = crc16((const uint8_t*)&hdr, 8);
    hdr.headerCRC[0] = (uint8_t)(hcrc & 0xFF);
    hdr.headerCRC[1] = (uint8_t)((hcrc >> 8) & 0xFF);
  }
  uartSendBlock(UART_BASE, (uint8_t*)&hdr, HEADER_SIZE);
}

void sendSectorPacket(uint32_t lba, uint8_t status, const uint8_t *data){
  struct SectorHeader hdr;
  uint16_t dcrc;

  hdr.magic0 = HEADER_MAGIC0;
  hdr.magic1 = HEADER_MAGIC1;
  poke24(hdr.lba, lba);
  hdr.status = status;

  /* calculate data CRC */
  dcrc = crc16(data, DATA_SIZE);
  hdr.dataCRC[0] = (uint8_t)(dcrc & 0xFF);
  hdr.dataCRC[1] = (uint8_t)((dcrc >> 8) & 0xFF);

  /* CRC over bytes 0..7 (magic + lba + status + dataCRC) */
  {
    uint16_t hcrc = crc16((const uint8_t*)&hdr, 8);
    hdr.headerCRC[0] = (uint8_t)(hcrc & 0xFF);
    hdr.headerCRC[1] = (uint8_t)((hcrc >> 8) & 0xFF);
  }

  /* send header then data */
  uartSendBlock(UART_BASE, (uint8_t*)&hdr, HEADER_SIZE);
  uartSendBlock(UART_BASE, data, DATA_SIZE);
}

void sendResponse(uint16_t base, uint8_t resp){
  uartTx(base, resp);
}

uint8_t peekByte(uint16_t base){
  if(uartRxReady(base)){
    return uartRx(base);
  }
  return 0xFF;
}

uint8_t recvCommand(uint16_t base){
  if(uartRxReady(base)){
    return uartRx(base);
  }
  return 0;
}

uint8_t recvBlockTimeout(uint16_t base, uint8_t *buf, uint16_t len, uint32_t timeoutTicks){
  uint16_t i = 0;
  uint32_t start = biosTicks();
  while(i < len){
    if(uartRxReady(base)){
      buf[i] = uartRx(base);
      i++;
    } else {
      if(biosTicks() - start > timeoutTicks){
        return 0; /* timeout */
      }
    }
  }
  return 1; /* success */
}

void sendStatusReply(uint16_t base, const struct Geometry *geom,
                     uint32_t currentLba, uint8_t headMask, uint8_t retries){
  struct StatusReply rep;
  uint16_t rcrc;

  rep.magic0 = HEADER_MAGIC0;
  rep.magic1 = HEADER_MAGIC1;
  rep.totalCyls = geom->cyls;
  rep.totalHeads = geom->heads;
  rep.totalSpt = geom->spt;
  rep.totalSectors = geom->totalSectors;
  rep.currentLba = currentLba;
  rep.headMask = headMask;
  rep.retries = retries;
  rep.reserved[0] = 0;
  rep.reserved[1] = 0;
  rep.reserved[2] = 0;
  rep.reserved[3] = 0;

  rcrc = crc16((const uint8_t*)&rep, sizeof(rep) - 2);
  rep.replyCRC[0] = (uint8_t)(rcrc & 0xFF);
  rep.replyCRC[1] = (uint8_t)((rcrc >> 8) & 0xFF);

  uartSendBlock(base, (uint8_t*)&rep, sizeof(rep));
}
