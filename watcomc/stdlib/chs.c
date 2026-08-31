#include "chs.h"
#include "math.h"

uint32_t ChsToLba(uint16_t c, uint8_t h, uint8_t s, const struct Geometry *geom)
{
    uint32_t cylHead = mul32_16(c, geom->heads) + h;
    return mul32_16(cylHead, geom->spt) + s - 1;
}

struct ChsWithLBA LbaToChsWithLba(uint32_t lba, const struct Geometry *geom)
{
    struct ChsWithLBA chs;
    uint16_t rem_sec;
    uint16_t track = div32_16(lba, geom->spt, &rem_sec);
    chs.sec = (uint8_t)(rem_sec + 1);
    chs.head = (uint8_t)(track % geom->heads);
    chs.cyl = track / geom->heads;
    chs.lba = lba;
    return chs;
}

void stepChs(struct ChsWithLBA* pos, const struct Geometry* geom){
  pos->sec += 1;
  if(pos->sec > geom->spt){
    pos->sec = 1;
    pos->head += 1;
  }
  if(pos->head >= geom->heads){
    pos->head = 0;
    pos->cyl += 1;
  }
  pos->lba++;
}
