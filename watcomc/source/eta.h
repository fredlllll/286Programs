/* estimated time remaining for the current floppy */
#ifndef ETA_H
#define ETA_H
#include "intdef.h"

/* prints " eta M:SS" based on elapsed bios ticks and how full the
   floppy is (floppyLba = sectors written so far). shows nothing
   when data is insufficient or under one second remains */
void printEta(uint32_t elapsedTicks, uint16_t floppyLba);

#endif
