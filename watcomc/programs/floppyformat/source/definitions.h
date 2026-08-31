/* floppy geometry constants only -- no hdd stuff */

#ifndef DEFINITIONS_H
#define DEFINITIONS_H
#include "intdef.h"

#define FLOPPY_CYLS 80
#define FLOPPY_HEADS 2
#define FLOPPY_SPT 18
#define FLOPPY_TOTAL_SECTORS (FLOPPY_CYLS*FLOPPY_HEADS*FLOPPY_SPT)

#define FLOPPY_DRIVE 0    /* 0x00 = first floppy, 0x01 = second */

#define FORMAT_RETRIES 3  /* attempts per track before giving up */
#define VERIFY_RETRIES 3  /* read attempts per sector during verify */

#endif
