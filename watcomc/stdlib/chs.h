#ifndef CHS_H
#define CHS_H
#include "intdef.h"

/* drive geometry: how many cylinders/heads/sectors a drive has, and
   the resulting total capacity */
struct Geometry {
  uint16_t cyls;
  uint8_t heads;
  uint8_t spt;
  uint32_t totalSectors;
};

struct Chs
{
    uint16_t cyl;
    uint8_t head;
    uint8_t sec; /* starts at 1! */
};

struct ChsWithLBA
{
    uint16_t cyl;
    uint8_t head;
    uint8_t sec; /* starts at 1! */
    uint32_t lba;
};

/* standard chs -> lba conversion:
   lba = (cylinder * heads + head) * sectors_per_track + (sector - 1).
   the multiplies go through mul32_16 so intermediate values cannot
   truncate at 16 bits */
uint32_t ChsToLba(uint16_t c, uint8_t h, uint8_t s, const struct Geometry *geom);

struct ChsWithLBA LbaToChsWithLba(uint32_t lba, const struct Geometry *geom);

/* advances a chs position by one sector, wrapping like the bios
   expects: sector overflows into head, head into cylinder */
void stepChs(struct ChsWithLBA* pos, const struct Geometry* geom);

#endif
