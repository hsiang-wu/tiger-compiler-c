#ifndef CODEGEN_H
#define CODEGEN_H

AS_instrList F_codegen(F_frame f, T_stmList stmList);

static Temp_temp munchExp(T_exp e);
static void munchStm(T_stm s);
static Temp_tempList munchArgs(int i, T_expList args);

Temp_tempList L(Temp_temp h, Temp_tempList t);
static void emit(AS_instr inst);

#endif
