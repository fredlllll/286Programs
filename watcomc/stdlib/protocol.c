#include "protocol.h"
#include "uart.h"
#include "util.h"
#include "math.h"

static uint32_t currentPackageNum = 1;

static uint32_t getNextMessageNumber(void)
{
  return currentPackageNum++;
}

static void sendMagic()
{
  uartTxBlocking(HEADER_MAGIC0);
  uartTxBlocking(HEADER_MAGIC1);
}

static void sendMessage(uint8_t opcode, uint32_t packetNumber)
{
  sendMagic();
  uartTxBlocking(opcode);
  uartSendBlock((uint8_t *)&packetNumber, sizeof(packetNumber));
}

static void sendMessage(uint8_t opcode, uint32_t packetNumber, void *body, uint16_t bodyLen)
{
  sendMagic();
  uartTxBlocking(opcode);
  uartSendBlock((uint8_t *)&packetNumber, sizeof(packetNumber));
  uartSendBlock(body, bodyLen);
}

void Ack(uint32_t packetNum)
{
  sendMessage(CMD_ACK, packetNum);
}

void NAck(uint32_t packetNum)
{
  sendMessage(CMD_NACK, packetNum);
}

void sendPong(void)
{
  sendMessage(CMD_PONG, getNextMessageNumber());
}

void sendSectorHeaderOnly(uint32_t lba, uint8_t status)
{
  struct SectorHeader hdr;
  hdr.status = status;
  hdr.lba = lba;
  hdr.dataCRC = 0;
  sendMessage(CMD_SECTOR, getNextMessageNumber(), &hdr, sizeof(hdr));
}

void sendSectorPacket(uint32_t lba, uint8_t status, const uint8_t *data)
{
  struct SectorHeader hdr;
  hdr.status = status;
  hdr.lba = lba;
  hdr.dataCRC = crc16(data, 512);
  sendMessage(CMD_SECTOR, getNextMessageNumber(), &hdr, sizeof(hdr));
  uartSendBlock(data, 512);
}

void sendStatusReply(const struct Geometry *geom, uint32_t currentLba, uint8_t headMask, uint8_t retries)
{
  struct StatusReply rep;
  rep.totalCyls = geom->cyls;
  rep.totalHeads = geom->heads;
  rep.totalSpt = geom->spt;
  rep.totalSectors = geom->totalSectors;
  rep.currentLba = currentLba;
  rep.headMask = headMask;
  rep.retries = retries;
  sendMessage(CMD_STATUS, getNextMessageNumber(), &rep, sizeof(rep));
}
