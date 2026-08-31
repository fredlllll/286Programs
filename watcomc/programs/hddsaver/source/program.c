#include "hdd.h"
#include "floppy.h"
#include "eta.h"
#include "programState.h"
#include "util.h"
#include "keyboard.h"
#include "print.h"
#include <i86.h>

struct ProgramState prgState;

void ensureStartLbaLimit(uint32_t startLba)
{
    if (startLba >= hddGeom.totalSectors)
    {
        print("Start LBA beyond end of drive.\r\nProgram halted.\r\n");
        halt();
    }
}

void printProgramStart(void)
{
    print("HDD saver 3.0\r\n");
    print("Dumps ");
    printDecLong(hddGeom.totalSectors);
    print(" hdd sectors (");
    printDecLong(hddGeom.totalSectors >> 11); /* >>11 = /2048 sectors = MB */
    print(" MB) to floppies");
    print("Every sector is stored with its lba+status, so unreadable\r\n");
    print("sectors are just logged (with the drive's error code) and\r\n");
    print("cost no floppy space. Disk order does not matter.\r\n");
    print("Floppy writes are verified and retried automatically.\r\n");
    print("ESC stops cleanly after the current transfer.\r\n");
}

void collectProgramInput(void)
{
    /* every prompt shows the compiled-in default in brackets; typing
     nothing (just enter) accepts it */
    uint32_t v;
    uint32_t startLba;
    v = decInput("Hdd read retries", hddRetries);
    if (v <= 255)
    {
        hddRetries = (uint8_t)v;
    }
    print("Head select: decimal bitmask, bit N = head N.\r\n");
    v = decInput("Head bitmask", 0xFF);
    if (v)
    {
        headMask = (uint8_t)v;
    }
    print("Using: ");
    printDecLong(hddGeom.cyls);
    print("/");
    printDecLong(hddGeom.heads);
    print("/");
    printDecLong(hddGeom.spt);
    print(", retries ");
    printDecLong(hddRetries);
    print(", head mask ");
    printDecLong(headMask);
    print(", total ");
    printDecLong(hddGeom.totalSectors);
    print(" sectors (");
    printDecLong(hddGeom.totalSectors >> 11);
    print(" MB).\r\n");

    /* resuming: the lba where the previous run stopped becomes the
     starting point; its disk number is derived from how many full
     disks worth of sectors fit before it */
    startLba = decInput("Resume at hdd LBA", 0);
    ensureStartLbaLimit(startLba);
    seekHdd(startLba);
    // TODO: startslba is used to calculate ETA
}

/* one status line per flushed group: disk label, absolute hdd
   position, descriptor/data counts, floppy failures and a rolling
   time estimate */
static void progressLine(void)
{
    print("\rLBA:");
    printDecLong(hddPos.lba);
    print("/");
    printDecLong(hddGeom.totalSectors);
}

/* one 512 byte sector of raw data */
struct Sector
{
    uint8_t data[512];
};

/* sector currently being filled by the hdd read path. small enough
   to live in dgroup like every other global */
struct Sector hddReadBuffer;

/* ---- far data arena ----

   the dump buffers a whole descriptor block worth of data sectors
   before writing them out (DESC_PER_BLOCK * 512 bytes). that does
   not fit below the 64k line together with code, stack and all
   other data - and in this flat setup (all segment registers 0,
   raw binary image) the linker cannot even express objects in a
   second segment: wlink rejects far relocations with "invalid
   relocation for flat memory model".

   so the arena is not a c object at all. it is a fixed region of
   free conventional memory high above the image, addressed through
   explicitly constructed far pointers. physical layout:

     0x00000..0x003FF  interrupt vector table (untouched)
     0x07C00           bootloader + stack growing down from 0x7C00
     0x07E00..         this program: code + dgroup, must stay < 64k
      0x10000..0x1C9FF  arena: DESC_PER_BLOCK sector slots
                        (segment 0x1000, offsets 0..0xC9FF)

   nothing else occupies that range at boot; keep ARENA_SEG in sync
   with build.py, which asserts dgroup really ends below it */
#define ARENA_SEG 0x1000

static struct Sector __far *dataSlot(uint8_t idx)
{
    return (struct Sector __far *)MK_FP(ARENA_SEG, (unsigned int)idx * sizeof(struct Sector));
}

#define DATAIDXNOTWRITTEN 255
/* ---- one sector descriptor: the ID card of a single hdd sector ----

   6 bytes each. lba is 24 bits (8 gb ceiling at 512 b/sector, far
   past anything an mfm/rll controller can address). status is the
   int13 code from the read attempt (or HEADSKIPPED). dataIdx says
   which slot of the following data list holds this sector's 512
   bytes; only meaningful when SECTOR_HAS_DATA(status). crc8 covers
   the first 5 bytes (lba + status + dataIdx) */

#define DESC_MAGIC0 'H'
#define DESC_MAGIC1 'D'
#define DESC_MAGIC2 'S'
#define DESC_MAGIC3 'A'
#define DESC_MAGIC4 'V'

#pragma pack(push, 1)
struct SectorDesc
{
    uint8_t lba[3];  /* hdd lba, little endian                  */
    uint8_t status;  /* int13 code, see SECTOR_STATUS_*         */
    uint8_t dataIdx; /* index into the group's data list        */
    uint8_t crc8;    /* crc8 over lba+status+dataIdx            */
};
#pragma pack(pop)

/* ---- one descriptor block: describes one whole track batch ----

   physically written AFTER the data it talks about, so the crc can
   cover the data sectors. count tells how many of the DESC_PER_BLOCK
   slots are real; unused tail slots are zeroed at flush time so every
   block emits deterministic bytes. each descriptor carries its own crc8
   so a partially written entry is caught. the header contains a
   five-byte magic ("HDSAV") so the python reader can positively
   identify descriptor blocks. the block crc (bytes 510..511) covers
   the data sectors that follow the descriptor block, not the
   descriptor block itself */

#define DESC_PER_BLOCK ((uint8_t)(509 / sizeof(struct SectorDesc)))
/* 509 / 6 = 84 descriptors. 84*6 = 504, +1 count, +5 magic, +2 crc
   = 512 exactly */

uint16_t currentDescriptorHeaderFloppyLba = 0;

#pragma pack(push, 1)
struct DescBlock
{
    uint8_t count;                                                         /* entries used           */
    struct SectorDesc desc[DESC_PER_BLOCK];                                /* the descriptors        */
    uint8_t magic[5];                                                      /* "HDSAV"                */
    uint16_t crc;                                                          /* crc over data sectors  */
} currentDescriptorHeader;
#pragma pack(pop)

uint8_t currentDataIdx = 0;

static uint8_t stopRequested = 0;

void writeOutBufferedData(void)
{
    uint8_t i;
    struct SectorDesc zeroed = {0};
    uint8_t neededSectors = 1;
    for (i = 0; i < currentDescriptorHeader.count; i++)
    {
        if (currentDescriptorHeader.desc[i].dataIdx != DATAIDXNOTWRITTEN)
        {
            neededSectors++;
        }
    }

    /* stamp header: magic, then zero unused descriptor slots */
    currentDescriptorHeader.magic[0] = DESC_MAGIC0;
    currentDescriptorHeader.magic[1] = DESC_MAGIC1;
    currentDescriptorHeader.magic[2] = DESC_MAGIC2;
    currentDescriptorHeader.magic[3] = DESC_MAGIC3;
    currentDescriptorHeader.magic[4] = DESC_MAGIC4;
    for (i = currentDescriptorHeader.count; i < DESC_PER_BLOCK; i++)
    {
        currentDescriptorHeader.desc[i] = zeroed;
    }

    /* per-descriptor crc8: covers lba(3) + status + dataIdx = 5 bytes */
    for (i = 0; i < currentDescriptorHeader.count; i++)
    {
        currentDescriptorHeader.desc[i].crc8 =
            crc8(currentDescriptorHeader.desc[i].lba, 5);
    }

    if (FLOPPY_TOTAL_SECTORS - floppyPosition.lba < neededSectors)
    {
        uint32_t wstart = biosTicks();
        if (waitForContinue("Floppy is full, put a new one in and press enter\r\n"))
        {
            stopRequested = 1;
            return;
        }
        addWaitTicks(biosTicks() - wstart);
        seekFloppy(0, 0, 1);
    }

    while (1)
    {
        uint8_t ok = 1;
        uint16_t dataCrc = 0xFFFF;
        uint8_t dataSectors = 0;

        /* write data sectors first */
        for (i = 0; i < currentDescriptorHeader.count; ++i)
        {
            uint8_t dataIdx = currentDescriptorHeader.desc[i].dataIdx;
            if (dataIdx != DATAIDXNOTWRITTEN)
            {
                ok &= writeVerified(dataSlot(dataIdx));
                dataCrc = crcBuf(dataCrc, (uint8_t __far *)dataSlot(dataIdx), 512);
                dataSectors++;
            }
        }

        /* crc covers the data sectors that follow the descriptor block */
        currentDescriptorHeader.crc = dataCrc;

        /* write descriptor block last, now with correct crc */
        ok &= writeVerified(&currentDescriptorHeader);
        if (ok)
        {
            break;
        }
        {
            uint32_t wstart = biosTicks();
            if (waitForContinue("Floppy write impossible, give new one and press enter\r\n"))
            {
                addWaitTicks(biosTicks() - wstart);
                if (!writeVerified(&currentDescriptorHeader))
                {
                    for (i = 0; i < currentDescriptorHeader.count; ++i)
                    {
                        uint8_t dataIdx = currentDescriptorHeader.desc[i].dataIdx;
                        if (dataIdx != DATAIDXNOTWRITTEN)
                        {
                            writeVerified(dataSlot(dataIdx));
                        }
                    }
                }
                stopRequested = 1;
                return;
            }
            addWaitTicks(biosTicks() - wstart);
        }
        seekFloppy(0, 0, 1);
    }
    currentDescriptorHeader.count = 0;
    currentDataIdx = 0;
}

void addDescriptor(uint32_t lba, uint8_t status)
{
    struct SectorDesc newDesc;
    newDesc.status = status;
    poke24(newDesc.lba, lba);
    if (isStatusSuccess(status))
    {
        // write descriptor and data
        newDesc.dataIdx = currentDataIdx++;
        *dataSlot(newDesc.dataIdx) = hddReadBuffer;
    }
    else
    {
        newDesc.dataIdx = DATAIDXNOTWRITTEN;
    }
    currentDescriptorHeader.desc[currentDescriptorHeader.count] = newDesc;
    currentDescriptorHeader.count++;

    if (currentDescriptorHeader.count >= DESC_PER_BLOCK)
    {
        writeOutBufferedData();
    }
}

void processFloppy(void)
{
    uint32_t diskStartTicks = biosTicks();
    uint8_t status;
    currentDescriptorHeaderFloppyLba = 0;
    resetWaitTicks();

    while (hddPos.lba < hddGeom.totalSectors)
    {
        if (stopRequested || escPressed())
        {
            stopRequested = 1;
            break;
        }
        if (headMask & (1 << hddPos.head))
        {
            /* read straight into the next FREE data slot: failed sectors
               do not consume one, so good data stays packed and the
               descriptor indices come out in encounter order */
            status = readHddResilient(hddReadBuffer.data);
        }
        else
        {
            /* head deselected via bitmask: record it as such, dump
               nothing. a later pass with that head enabled can fill the
               hole - the restorer simply leaves this lba uncovered */
            status = SECTOR_STATUS_HEADSKIPPED;
        }
        addDescriptor(hddPos.lba, status);
        advanceHddPosition();
        progressLine();
        printEta(biosTicks() - diskStartTicks, floppyPosition.lba);
    }
    if (currentDescriptorHeader.count > 0)
    {
        writeOutBufferedData();
    }
    {
        uint16_t rem;
        uint16_t m = div32_16(biosTicks() - diskStartTicks, 1092, &rem);
        uint16_t s = div32_16(rem, 18, 0);
        print("\rDisk done in ");
        printDecLong(m);
        print("m ");
        printDecLong(s);
        print("s\r\n");
    }
}

void checkProgramEnd(void)
{
    if (hddPos.lba >= hddGeom.totalSectors)
    {
        /* every sector of the drive has been through the pipeline */
        print("\r=== DUMP COMPLETE ===\r\n");
        print("Safe to power off.\r\n");
        halt();
    }
}

void program(void)
{
    printProgramStart();
    collectProgramInput();
    print("Starting. Everything else runs by itself.\r\n");

    while (1)
    {
        processFloppy();
        if (stopRequested)
        {
            print("\r=== STOPPED BY ESC ===\r\nResume next run at LBA ");
            printDecLong(hddPos.lba);
            print(".\r\nSafe to power off.\r\n");
            halt();
        }
        checkProgramEnd();
    }
}
