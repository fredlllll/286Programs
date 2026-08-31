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
#define CMD_START       0x01
#define CMD_STOP        0x02
#define CMD_SEEK        0x03
#define CMD_PING        0x04
#define CMD_STATUS      0x05
#define CMD_HEAD_MASK   0x10
#define CMD_RETRIES     0x11
#define CMD_BAUD_RATE   0x12

/* response bytes from 286 to PC */
#define RESP_READY      0x06
#define RESP_ACK        0x06
#define RESP_NAK        0x15

/* header magic bytes */
#define HEADER_MAGIC0   0xAA
#define HEADER_MAGIC1   0x55
#define HEADER_SIZE     10
#define DATA_SIZE       512
#define PACKET_SIZE     (HEADER_SIZE + DATA_SIZE)  /* max packet */

/* sector header as it appears on the wire */
#pragma pack(push, 1)
struct SectorHeader {
  uint8_t magic0;     /* 0xAA */
  uint8_t magic1;     /* 0x55 */
  uint8_t lba[3];     /* little-endian, 24-bit */
  uint8_t status;     /* INT 13h code */
  uint8_t dataCRC[2]; /* CRC-16 over data (0 if no data) */
  uint8_t headerCRC[2]; /* CRC-16 over bytes 0..7 */
};
#pragma pack(pop)

/* status reply packet (sent in response to CMD_STATUS) */
#pragma pack(push, 1)
struct StatusReply {
  uint8_t magic0;     /* 0xAA */
  uint8_t magic1;     /* 0x55 */
  uint16_t totalCyls;
  uint8_t totalHeads;
  uint8_t totalSpt;
  uint32_t totalSectors;
  uint32_t currentLba;
  uint8_t headMask;
  uint8_t retries;
  uint8_t reserved[4]; /* future use */
  uint8_t replyCRC[2];
};
#pragma pack(pop)

/* send a sector header (always) + data (if status is good) */
void sendSectorPacket(uint32_t lba, uint8_t status, const uint8_t *data);

/* send header only (for failed/skipped sectors) */
void sendSectorHeaderOnly(uint32_t lba, uint8_t status);

/* send a single byte response (READY, ACK, NAK) */
void sendResponse(uint16_t base, uint8_t resp);

/* receive a command byte. returns 0 if no command available,
   otherwise the command byte (CMD_*) */
uint8_t recvCommand(uint16_t base);

/* receive a block of bytes with timeout. returns 1 on success, 0 on timeout */
uint8_t recvBlockTimeout(uint16_t base, uint8_t *buf, uint16_t len, uint32_t timeoutTicks);

/* send status reply packet */
void sendStatusReply(uint16_t base, const struct Geometry *geom,
                     uint32_t currentLba, uint8_t headMask, uint8_t retries);

/* peek at next byte without consuming. returns 0xFF if nothing available */
uint8_t peekByte(uint16_t base);

#endif
