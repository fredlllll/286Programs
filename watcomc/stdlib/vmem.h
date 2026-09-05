// vmem.h
#ifndef VMEM_H
#define VMEM_H
#include "intdef.h"

void vmemInit(void);
void vmemPrint(const char *text);
void vmemPrintHex(uint8_t value);
void vmemPrintHexShort(uint16_t value);
void vmemPrintHexLong(uint32_t value);
void vmemPrintDecLong(uint32_t value);

#endif
