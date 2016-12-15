#ifndef TRANSLATE_H
#define TRANSLATE_H

#include "absyn.h"
#include "frame.h"
#include "tree.h"

/* Lab5: your code below */

typedef struct Tr_exp_* Tr_exp;

typedef struct Tr_expList_* Tr_expList;

typedef struct Tr_access_* Tr_access;

typedef struct Tr_accessList_* Tr_accessList;

typedef struct Tr_level_* Tr_level;

typedef struct patchList_* patchList;

Tr_level Tr_outermost(void);
Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);
Tr_accessList Tr_formals(Tr_level level);
Tr_access Tr_allocLocal(Tr_level level, bool escape);

Tr_accessList Tr_allocPara(Tr_level, bool escape, Tr_accessList tail);
Tr_expList Tr_ExpList(Tr_exp, Tr_expList);

// append element e to the tail
// modify t to point to the new tail.
// tail can be NULL.
void Tr_elAppend(Tr_exp e, Tr_expList t);

// for encapsulation. provide this for semant.c so it
// can iterate through accessList.
Tr_accessList AccTail(Tr_accessList accs);
Tr_access AccHead(Tr_accessList accs);

// IR

Tr_exp Tr_simpleVar(Tr_access access, Tr_level level);
Tr_exp Tr_fieldVar(Tr_exp exp, int off /* const ? */);
Tr_exp Tr_subscriptVar(Tr_exp var, Tr_exp sub);

Tr_exp Tr_constVar(int i);
Tr_exp Tr_stringVar(string i);

Tr_exp Tr_arithExp(A_oper op, Tr_exp e1, Tr_exp e2);
// exp should not be string.
Tr_exp Tr_compExp(A_oper, Tr_exp, Tr_exp);

Tr_exp Tr_recordExp(Tr_expList);

Tr_exp Tr_stringExp(string);

Tr_exp Tr_intExp(int);

Tr_exp Tr_callExp(Temp_label label, Tr_level, Tr_level, Tr_expList);

Tr_exp Tr_nilExp();

Tr_exp Tr_arrayExp(Tr_exp, Tr_exp);

Tr_exp Tr_seqExp(Tr_expList);

Tr_exp Tr_doneExp();

Tr_exp Tr_whileExp(Tr_exp, Tr_exp, Tr_exp);

Tr_exp Tr_forExp(Tr_exp, Tr_exp, Tr_exp, Tr_exp);

Tr_exp Tr_assignExp(Tr_exp, Tr_exp);

Tr_exp Tr_breakExp();

Tr_exp Tr_compString(A_oper, Tr_exp, Tr_exp);

Tr_exp Tr_ifExp(Tr_exp, Tr_exp, Tr_exp);

void Tr_procEntryExit(Tr_level, Tr_exp);

F_fragList Tr_getFragList();

#endif
