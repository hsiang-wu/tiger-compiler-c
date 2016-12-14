#ifndef REALLOC_H
#define REALLOC_H
#include "assem.h"
#include "frame.h"
/* function prototype from regalloc.c */
struct RA_result {
    Temp_map coloring;
    AS_instrList il;
};
struct RA_result RA_regAlloc(F_frame f, AS_instrList il);
#endif
