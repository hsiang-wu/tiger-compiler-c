#ifndef SEMANT_H
#define SEMANT_H
#include <stdio.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "env.h"
/* new in lab5 */
#include "temp.h"
#include "tree.h"
#include "frame.h"

/* struct expty; */

struct expty expTy(Tr_exp exp, Ty_ty ty);

F_fragList SEM_transProg(A_exp exp);
struct expty transVar(S_table venv, S_table tenv, A_var v, Tr_level level, Tr_exp breakk);
struct expty transExp(S_table venv, S_table tenv, A_exp a, Tr_level level, Tr_exp brreakk /* for break in while and for */);
Tr_exp       transDec(S_table venv, S_table tenv, A_dec d, Tr_level level, Tr_exp breakk);
Ty_ty transTy(               S_table tenv, A_ty a);

Ty_ty actual_ty(Ty_ty ty);
Ty_tyList makeFormalTyList(S_table tenv, A_fieldList);
Ty_fieldList makeTyFieldList(S_table tenv, A_fieldList afl);
Tr_expList checkRecordFieldList(S_table tenv, S_table venv, A_efieldList efl, Ty_fieldList, Tr_level level, Tr_exp brk);
Ty_tyList nametyList(S_table tenv, A_nametyList nl);
void checkTypeDec(S_table tenv, Ty_tyList nl);
void checkRecord(S_table tenv, Ty_fieldList fl);

#endif
