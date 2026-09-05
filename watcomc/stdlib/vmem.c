// vmem.c
#include "vmem.h"
#include "util.h"
#include "inout.h"

/* standard CGA/EGA/VGA color text mode 3. if this machine has a
   monochrome (MDA/Hercules) card instead, change this to 0xB000. */
#define VMEM_SEG 0xB800
#define VMEM_COLS 80
#define VMEM_ROWS 25
#define VMEM_ATTR 0x07 /* light grey on black */

static uint8_t cursorRow = 0;
static uint8_t cursorCol = 0;

static void updateHwCursor(void)
{
    uint16_t pos = (uint16_t)cursorRow * VMEM_COLS + cursorCol;
    out8(0x3D4, 0x0E);
    out8(0x3D5, (uint8_t)(pos >> 8));
    out8(0x3D4, 0x0F);
    out8(0x3D5, (uint8_t)(pos & 0xFF));
}

static void vmemScroll(void)
{
    uint16_t __far *vmem = (uint16_t __far *)MK_FP(VMEM_SEG, 0);
    uint16_t i;
    for (i = 0; i < (VMEM_ROWS - 1) * VMEM_COLS; i++)
        vmem[i] = vmem[i + VMEM_COLS];
    for (i = 0; i < VMEM_COLS; i++)
        vmem[(VMEM_ROWS - 1) * VMEM_COLS + i] = ((uint16_t)VMEM_ATTR << 8) | ' ';
}

void vmemInit(void)
{
    uint16_t __far *vmem = (uint16_t __far *)MK_FP(VMEM_SEG, 0);
    uint16_t i;
    for (i = 0; i < VMEM_ROWS * VMEM_COLS; i++)
        vmem[i] = ((uint16_t)VMEM_ATTR << 8) | ' ';
    cursorRow = cursorCol = 0;
    updateHwCursor();
}

static void vmemPutChar(char c)
{
    uint16_t __far *vmem = (uint16_t __far *)MK_FP(VMEM_SEG, 0);

    if (c == '\r')
    {
        cursorCol = 0;
    }
    else if (c == '\n')
    {
        cursorCol = 0;
        cursorRow++;
    }
    else
    {
        vmem[cursorRow * VMEM_COLS + cursorCol] = ((uint16_t)VMEM_ATTR << 8) | (uint8_t)c;
        if (++cursorCol >= VMEM_COLS)
        {
            cursorCol = 0;
            cursorRow++;
        }
    }
    if (cursorRow >= VMEM_ROWS)
    {
        vmemScroll();
        cursorRow = VMEM_ROWS - 1;
    }
    updateHwCursor();
}

void vmemPrint(const char *text)
{
    char ch;
    while ((ch = *text++)){
        vmemPutChar(ch);
    }
}

/* same hex/decimal digit logic as print.c, just call vmemPutChar
   instead of printChar — copy printHex/printHexShort/printHexLong/
   printDecLong over verbatim with that one substitution */