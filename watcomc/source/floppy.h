/* floppy side: chs stepping, verified sector writing and the on-disk
   format (disk header + per-sector descriptor blocks) */
#ifndef FLOPPY_H
#define FLOPPY_H

#include "definitions.h"
#include "int13.h"

extern const struct Geometry floppyGeometry;
extern struct ChsWithLBA floppyPosition;

void advanceFloppyPosition(void);
uint8_t writeVerified(void *src);
void seekFloppy(unsigned int c, unsigned char h, unsigned char s);
uint8_t writeFloppy(uint16_t c, uint8_t h, uint8_t s, void *src);
unsigned char writeFloppyAuto(void *src);

#define SECTOR_STATUS_OK 0             /* read clean, data present */
#define SECTOR_STATUS_ECC 0x11         /* read after ecc correction, data present */
#define SECTOR_STATUS_HEADSKIPPED 0xfe /* head masked out this pass, never attempted */

/* true when the descriptor's sector carries real data on the floppy */
#define SECTOR_HAS_DATA(s) ((s) == SECTOR_STATUS_OK || (s) == SECTOR_STATUS_ECC)

#endif
