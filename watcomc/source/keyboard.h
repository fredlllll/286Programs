/* bios keyboard input.
   interrupt 16h is the bios keyboard service:
     ah=00 -> wait until a key is pressed, return it
     ah=01 -> check whether a key is waiting without waiting

   keys are reported as two bytes: the ascii code (al) and a hardware
   "scan code" (ah) identifying the physical key. we only care about
   ascii here */
#ifndef KEYBOARD_H
#define KEYBOARD_H
#include "intdef.h"

/* blocks until a key is pressed, returns its ascii code.
   used for "press enter" prompts and the decimal input editor */
uint8_t getNextKeyPress(void);

/* non blocking esc check for use inside the copy loop: returns 1 if
   escape was pressed, 0 otherwise (or if some other key is waiting) */
uint8_t escPressed(void);

/* prints prompt, then waits for enter. empty string allowed */
void waitForEnter(char* prompt);

/* prints "prompt [default]: " and lets the user type a decimal
   number. digits are echoed, backspace edits, enter submits, an
   empty input returns defVal unchanged */
uint32_t decInput(char* prompt, uint32_t defVal);

#endif
