#ifndef INOUT_H
#define INOUT_H
#include "intdef.h"
uint8_t in8(uint16_t port);
uint16_t in16(uint16_t port);
void out8(uint16_t port, uint8_t val);
void out16(uint16_t port, uint16_t val);
#endif
