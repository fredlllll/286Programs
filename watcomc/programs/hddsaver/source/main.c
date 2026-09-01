// hdd saver 3.1 - dumps the whole 60MB MFM/RLL drive to a modern PC
// via serial connection at 9600 baud.
//
// this file is the entry point: initializes uart and crc, then hands
// off to program.c for the main loop.

#include "definitions.h"
#include "math.h"
#include "print.h"
#include "keyboard.h"
#include "int13.h"
#include "util.h"
#include "hdd.h"
#include "protocol.h"
#include "uart.h"
#include "program.h"

void _cstart(void){
  /*shut up linker who cant find _cstart_ that it doesnt need*/
}

#pragma code_seg ( "start_segment" )
void main(void){
  /* initialize crc table for protocol */
  crcInit();

  /* initialize uart: 9600 baud, 8N1 */
  initUartAndIrq(12);
  uartFlushRx();

  /* run the main program */
  program();
  halt();
}
#pragma code_seg ()
