#include "int.h"

void disableInterrupts(void) { __asm { cli } }
void enableInterrupts(void)  { __asm { sti } }
