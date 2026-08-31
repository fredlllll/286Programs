/* estimated time remaining for the current floppy */
#ifndef ETA_H
#define ETA_H
#include "intdef.h"

/* prints " eta M:SS  " based on elapsed bios ticks and how full the
   floppy is (floppyLba = sectors written so far). shows nothing
   when data is insufficient or under one second remains. wait ticks
   (time spent paused for floppy swaps) are subtracted so the eta
   only reflects actual writing time */
void printEta(uint32_t elapsedTicks, uint16_t floppyLba);

/* record ticks spent waiting for the user to swap a floppy. these
   are subtracted from the elapsed time used for eta calculations */
void addWaitTicks(uint32_t ticks);

/* reset accumulated wait ticks at the start of each floppy */
void resetWaitTicks(void);

#endif
