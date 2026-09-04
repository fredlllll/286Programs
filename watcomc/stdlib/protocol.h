/* serial protocol for hdd saver: framed packets between 286 and PC.

   packet layout (286 -> PC):
     header (10 bytes): magic(2) + lba(3) + status(1) + dataCRC(2) + headerCRC(2)
     data (512 bytes): only when status = 0x00 or 0x11

   commands (PC -> 286):
     0x01  START       begin/resume sending sectors
     0x02  STOP        pause after current sector
     0x03  SEEK +3     seek to lba (3 bytes LE)
     0x04  PING        286 replies with READY
     0x05  STATUS      286 sends geometry + position + config
     0x10  HEAD_MASK   set head bitmask (1 byte)
     0x11  RETRIES     set retry count (1 byte)
     0x12  BAUD_RATE   set baud divisor (1 byte)

   responses (PC -> 286, after each sector):
     0x06  ACK         sector OK, send next
     0x15  NAK         CRC error, retransmit */
#ifndef PROTOCOL_H
#define PROTOCOL_H
#include "intdef.h"
#include "chs.h"

/* command bytes from PC */
#define CMD_START 0x01
#define CMD_STOP 0x02
#define CMD_SEEK 0x03
#define CMD_HEAD_MASK 0x04
#define CMD_RETRIES 0x05
#define CMD_PING 0x06
#define CMD_PONG 0x07
#define CMD_SEND_STATUS 0x08
#define CMD_STATUS 0x09
#define CMD_SECTOR 0x0a

#define CMD_ACK 0xFE
#define CMD_NACK 0xCC

/* header magic bytes */
#define HEADER_MAGIC0 0xAA
#define HEADER_MAGIC1 0x55

/* sector header as it appears on the wire */
#pragma pack(push, 1)
struct SectorHeader
{
  uint8_t status;   /* INT 13h code */
  uint32_t lba;     /* sector lba */
  uint16_t dataCRC; /* CRC-16 over data (0 if no data) */
};
#pragma pack(pop)

/* status reply packet (sent in response to CMD_STATUS) */
#pragma pack(push, 1)
struct StatusReply
{
  uint16_t totalCyls;
  uint8_t totalHeads;
  uint8_t totalSpt;
  uint32_t totalSectors;
  uint32_t currentLba;
  uint8_t headMask;
  uint8_t retries;
};
#pragma pack(pop)

void Ack(uint32_t packetNum);
void NAck(uint32_t packetNum);

void sendPong(void);

/* send header only (for failed/skipped sectors) */
uint32_t sendSectorHeaderOnly(uint8_t status, uint32_t lba);

/* send a sector header (always) + data (if status is good) */
uint32_t sendSectorPacket(uint8_t status, uint32_t lba, const uint8_t *data);

/* send status reply packet */
void sendStatusReply(const struct Geometry *geom,
                     uint32_t currentLba, uint8_t headMask, uint8_t retries);

#endif
