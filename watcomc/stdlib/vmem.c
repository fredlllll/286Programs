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

/* lookup table for hex nibbles: index 0..15 -> character '0'..'F'. */
static char* hexAlphabet = "0123456789ABCDEF";

void vmemPrintHex(uint8_t value)
{
    vmemPutChar(hexAlphabet[value >> 4]);
    vmemPutChar(hexAlphabet[value & 0x0F]);
}

void vmemPrintHexShort(uint16_t value)
{
    vmemPrintHex((uint8_t)(value >> 8));   /* high byte first */
    vmemPrintHex((uint8_t)(value & 0xFF)); /* then low byte   */
}

void vmemPrintHexLong(uint32_t value)
{
    vmemPrintHexShort((uint16_t)(value >> 16));
    vmemPrintHexShort((uint16_t)(value & 0xFFFF));
}

/* powers of ten up to 10^9; the largest unsigned long is about
   4.29 * 10^9, so ten entries are enough */
static uint32_t pow10[10] = {1ul,10ul,100ul,1000ul,10000ul,100000ul,
                             1000000ul,10000000ul,100000000ul,1000000000ul};

/* prints v in decimal without any division: for each power of ten,
   count how often it fits by subtracting over and over. the "started"
   flag suppresses leading zeros but makes sure at least one digit
   (the last) always comes out */
void vmemPrintDecLong(uint32_t v)
{
    char c;
    uint8_t i;
    uint8_t started;
    started = 0;
    for (i = 9;; i--){
        c = '0';
        while (v >= pow10[i]){
            v -= pow10[i];
            c++;                  /* count subtractions = next digit */
        }
        if (c > '0' || started || i == 0){
            vmemPutChar(c);
            started = 1;
        }
        if (i == 0){
            break;
        }
    }
}