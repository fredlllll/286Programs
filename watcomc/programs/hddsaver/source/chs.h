#ifndef CHS_H
#define CHS_H
#include "intdef.h"
#include "math.h"
#include "int13.h"

/* ---- chs addressing types ---- */

/* one position on a chs-addressed drive. sectors are numbered from
   1, cylinders and heads from 0 - an ancient bios convention that
   still bites today */
struct Chs
{
    uint16_t cyl; /* 0..1023 fit in the int13 register bits */
    uint8_t head;
    uint8_t sec; /* starts at 1! */
};

struct ChsWithLBA
{
    uint16_t cyl; /* 0..1023 fit in the int13 register bits */
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

#endif
