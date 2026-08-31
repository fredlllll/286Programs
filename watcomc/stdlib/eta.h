/* estimated time remaining for the current task */
#ifndef ETA_H
#define ETA_H
#include "intdef.h"

/* prints " eta M:SS  " based on elapsed bios ticks and progress.
   elapsedTicks = ticks since start, current = sectors completed,
   total = total sectors to do. shows nothing when data is
   insufficient or under one second remains. wait ticks (time spent
   paused) are subtracted so the eta only reflects active time */
void printEta(uint32_t elapsedTicks, uint16_t current, uint16_t total);

/* record ticks spent waiting (subtracted from elapsed time) */
void addWaitTicks(uint32_t ticks);

/* reset accumulated wait ticks at the start of each unit of work */
void resetWaitTicks(void);

#endif
