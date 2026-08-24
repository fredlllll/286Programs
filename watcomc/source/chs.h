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

uint32_t ChsToLba(uint16_t c, uint8_t h, uint8_t s);

struct ChsWithLBA LbaToChsWithLba(uint32_t lba, const struct Geometry *geom);

#endif
