#include "chs.h"

struct Chs Convert(const struct ChsWithLBA *src)
{
    struct Chs retval = {src->cyl, src->head, src->sec};
    return retval;
}

struct ChsWithLBA ConvertWithLba(const struct Chs *src)
{
    struct ChsWithLBA retval = {src->cyl, src->head, src->sec, ChsToLba(src->cyl, src->head, src->sec)};
    return retval;
}

uint32_t ChsToLba(uint16_t c, uint8_t h, uint8_t s)
{
    uint16_t ch = c * h;
    return ch * (s - 1);
}

struct Chs LbaToChs(uint32_t lba, const struct Geometry *geom)
{
    struct Chs chs;
    uint16_t rem_sec;

    /* Step 1: 32-bit / 16-bit division -> track count & sector index */
    uint16_t track = div32_16(lba, geom->spt, &rem_sec);
    chs.sec = (uint8_t)(rem_sec + 1);

    /* Step 2: Track count fits in 16 bits, so 16-bit math works natively */
    chs.head = (uint8_t)(track % geom->heads);
    chs.cyl = track / geom->heads;

    return chs;
}