// HDD saver 3.0 - dumps the whole 60MB MFM/RLL drive to 1.44MB floppies
//
// this file is the application logic: session state, batching, the
// per-disk flow and the startup prompts. everything below it is
// plumbing that lives in its own module:
//
//   int13.c    raw bios disk calls (read/write/reset/query),
//              chs addressing types + stepping helper
//   hdd.c      hdd geometry, position tracking, resilient reads
//   floppy.c   position tracking, verified writes, format structs,
//              descriptor block builder
//   print.c    console output
//   keyboard.c key input
//   math.c     32 bit mul/div without runtime helpers
//   util.c     crc, byte poking, fill patterns, tick counter
//   definitions.h all tunable constants
//
// floppy layout (per disk), version 3:
//   LBA 0      : header sector, see struct FloppyHeader in floppy.h
//   LBA 1..9   : reserved (zeros)
//   LBA 10..2879: repeating groups, one group per hdd track batch:
//                  [descriptor block: 1 sector, struct DescBlock]
//                  [the data sectors its entries refer to]
//
// every dumped hdd sector carries its own identification in the
// descriptor: lba (3 bytes), the int13 error/status code of the read
// attempt (1 byte) and the index of its 512-byte data slot within the
// group (1 byte). statuses 0/0x11 mean "good data follows"; anything
// else means the sector could not be read (or the head was masked
// out) and NO data was written for it - it costs 5 bytes instead of
// 512. consequences:
//   - disks are self-describing, the restorer scatters by lba, so
//     disk order no longer matters and gaps between lbas are legal
//   - diskIndex survives only as a human-friendly label
//   - the restorer sees the exact bios error for every hole
//
// fully unattended operation:
// - unreadable hdd sectors: retried RETRY_HDD times, then logged as
//   a descriptor carrying the error code; copying continues
// - floppy writes: verified by reading back. a surrendered write is
//   treated as fatal for the current disk: it gets redone on fresh
//   media (see TODO in floppy.c)
//
// note: no 32 bit division/multiplication anywhere, watcom would emit
// calls to runtime helpers (__U4D etc.) which we dont link against.
// 32 bit add/sub/cmp/constant-shift is fine, it compiles inline.
// see math.c for the shift-add helpers that stand in for them

#include "definitions.h"
#include "math.h"
#include "print.h"
#include "keyboard.h"
#include "int13.h"
#include "util.h"
#include "hdd.h"
#include "floppy.h"
#include "program.h"


/* ------------------------- entry */

void _cstart(void){
  /*shut up linker who cant find _cstart_ that it doesnt need*/
}


/* main() runs the interactive part once (bios query, geometry
   prompts), then loops over floppies forever until the drive is
   fully dumped or the user presses esc. it never returns; finished
   states halt the cpu */
#pragma code_seg ( "start_segment" )
void main(void){

  crcInit();

  program();
}
#pragma code_seg ()
