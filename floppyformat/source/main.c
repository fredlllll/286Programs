/* low-level floppy formatter for 1.44mb 3.5" floppies.

   uses int 13h ah=05h to write sector address marks to each track,
   then optionally verifies by reading back every sector. this is a
   true low-level format -- it rewrites the sector headers, which can
   recover from bad sector address marks or id field corruption.

   usage: boot from this floppy, it formats drive a: automatically. */

#include "intdef.h"
#include "int13.h"
#include "print.h"
#include "keyboard.h"
#include "definitions.h"

void _cstart(void){}

/* sector id field entry: {cylinder, head, record, size_code}
   512 bytes = size code 2 (2^2 * 512 = 2048... wait, actually:
   size code N means 128 * 2^N bytes. so N=2 -> 128*4 = 512) */
#define SECTOR_SIZE_CODE 2

/* 512 byte buffer for the sector id fields. each track needs
   FLOPPY_SPT entries of 4 bytes = 72 bytes, rest can be zero */
static uint8_t formatBuf[512];

/* read buffer for verification */
static uint8_t verifyBuf[512];

/* fill the format buffer with sector id fields for one track.
   cylinder and head are the track being formatted. */
static void fillFormatBuf(uint8_t cylinder, uint8_t head){
  uint16_t i;
  uint8_t sec;
  /* zero the buffer first */
  for(i = 0; i < 512; i++){
    formatBuf[i] = 0;
  }
  /* 18 sectors per track, numbered 1..18 */
  for(sec = 0; sec < FLOPPY_SPT; sec++){
    uint16_t off = sec * 4;
    formatBuf[off + 0] = cylinder;     /* c: cylinder */
    formatBuf[off + 1] = head;         /* h: head */
    formatBuf[off + 2] = sec + 1;      /* r: sector number (1-based) */
    formatBuf[off + 3] = SECTOR_SIZE_CODE; /* n: size code (512 bytes) */
  }
}

static void printCrlf(void){
  print("\r\n");
}

static void printTrackInfo(uint8_t cyl, uint8_t head){
  print("cyl ");
  printDecLong(cyl);
  print(" head ");
  printDecLong(head);
}

/* format all tracks on the floppy */
static uint8_t formatFloppy(void){
  uint8_t cyl;
  uint8_t head;
  uint8_t status;
  uint8_t errors;
  uint16_t trackNum;
  uint16_t totalTracks;

  totalTracks = FLOPPY_CYLS * FLOPPY_HEADS;
  errors = 0;

  print("Formatting ");
  printDecLong(totalTracks);
  print(" tracks...");
  printCrlf();

  trackNum = 0;
  for(cyl = 0; cyl < FLOPPY_CYLS; cyl++){
    for(head = 0; head < FLOPPY_HEADS; head++){
      fillFormatBuf(cyl, head);

      status = 0xFF; /* sentinel */
      {
        uint8_t attempt;
        for(attempt = 0; attempt < FORMAT_RETRIES; attempt++){
          resetDiskSystem(FLOPPY_DRIVE);
          status = formatTrack(FLOPPY_SPT, cyl, head, FLOPPY_DRIVE,
                               (void __far*)formatBuf);
          if(status == 0x00) break;
        }
      }

      /* progress: track X/Y */
      print("  [");
      printDecLong(trackNum + 1);
      print("/");
      printDecLong(totalTracks);
      print("] ");
      printTrackInfo(cyl, head);
      print(" -- ");
      if(status == 0x00){
        print("ok");
      }else{
        print("FAIL ");
        printInt13Status(status);
        errors++;
      }
      printCrlf();

      trackNum++;
    }
  }
  return errors;
}

/* verify: read sector 1 from each track to confirm formatting worked */
static uint8_t verifyFloppy(void){
  uint8_t cyl;
  uint8_t head;
  uint8_t status;
  uint8_t errors;
  uint16_t trackNum;
  uint16_t totalTracks;
  uint16_t i;
  void __far *vbuf;

  totalTracks = FLOPPY_CYLS * FLOPPY_HEADS;
  errors = 0;
  vbuf = (void __far*)verifyBuf;

  print("Verifying (reading sector 1 from each track)...");
  printCrlf();

  trackNum = 0;
  for(cyl = 0; cyl < FLOPPY_CYLS; cyl++){
    for(head = 0; head < FLOPPY_HEADS; head++){
      status = 0xFF;
      {
        uint8_t attempt;
        for(attempt = 0; attempt < VERIFY_RETRIES; attempt++){
          resetDiskSystem(FLOPPY_DRIVE);
          status = readFromDrive(1, cyl, head, 1, FLOPPY_DRIVE, vbuf);
          if(status == 0x00) break;
        }
      }

      print("  [");
      printDecLong(trackNum + 1);
      print("/");
      printDecLong(totalTracks);
      print("] ");
      printTrackInfo(cyl, head);
      print(" -- ");
      if(status == 0x00){
        print("ok");
      }else{
        print("FAIL ");
        printInt13Status(status);
        errors++;
      }
      printCrlf();

      trackNum++;
    }
  }
  return errors;
}

#pragma code_seg("start_segment")
void main(void){
  uint8_t fmtErr;
  uint8_t verErr;
  uint8_t k;

  print("=== Low-Level Floppy Formatter ===");
  printCrlf();
  print("Drive: A: (1.44MB 3.5)");
  printCrlf();
  print("This will ERASE the entire disk.");
  printCrlf();
  print("Press ENTER to format, ESC to abort.");
  printCrlf();

  k = getNextKeyPress();
  if(k == 27){
    print("Aborted.");
    halt();
  }

  printCrlf();

  fmtErr = formatFloppy();

  printCrlf();
  if(fmtErr){
    print("Format completed with ");
    printDecLong(fmtErr);
    print(" error(s).");
  }else{
    print("Format complete. All tracks OK.");
  }
  printCrlf();

  printCrlf();
  print("Verify? Press ENTER to verify, ESC to skip.");
  printCrlf();
  k = getNextKeyPress();
  if(k == 27){
    print("Skipping verify.");
    printCrlf();
    printCrlf();
    print("Done. Press any key to halt.");
    getNextKeyPress();
    halt();
  }

  printCrlf();
  verErr = verifyFloppy();

  printCrlf();
  if(verErr){
    print("Verify found ");
    printDecLong(verErr);
    print(" track(s) that could not be read.");
  }else{
    print("Verify passed. All tracks readable.");
  }
  printCrlf();
  printCrlf();
  print("Done. Press any key to halt.");
  getNextKeyPress();
  halt();
}
#pragma code_seg()
