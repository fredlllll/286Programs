/* estimated time remaining for the current floppy */
#ifndef ETA_H
#define ETA_H
#include "intdef.h"

/* prints " eta M:SS" based on elapsed bios ticks and sectors
   attempted. shows nothing when data is insufficient or the
   estimate is behind the current time */
void printEta(uint32_t elapsedTicks, uint16_t done);

#endif
