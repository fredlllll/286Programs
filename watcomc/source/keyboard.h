/* bios keyboard input */
#ifndef KEYBOARD_H
#define KEYBOARD_H

unsigned char getNextKeyPress(void);
unsigned char escPressed(void);
void waitForEnter(char* prompt);
unsigned long decInput(char* prompt, unsigned long defVal);

#endif
