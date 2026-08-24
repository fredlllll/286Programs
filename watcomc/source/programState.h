#ifndef PROGRAMSTATE_H
#define PROGRAMSTATE_H

#include "int13.h"
#include "definitions.h"

struct ProgramState
{
    uint32_t startLba;
};

/* defined once in program.c - a definition must never live in a
   header, every additional includer would get its own copy */
extern struct ProgramState prgState;


#endif
