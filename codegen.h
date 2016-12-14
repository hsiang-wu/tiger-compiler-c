#ifndef CODEGEN_H
#define CODEGEN_H
#include "assem.h"
#include "frame.h"
#include "temp.h"

AS_instrList F_codegen(F_frame f, T_stmList stmList);

Temp_tempList L(Temp_temp h, Temp_tempList t);

#endif
