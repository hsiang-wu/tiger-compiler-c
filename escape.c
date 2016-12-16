#include "util.h"
#include "symbol.h" 
#include "absyn.h"  
#include <stdlib.h>
#include <stdio.h>
#include "table.h"

/*this model must before semant model*/
static void transExp(S_table env, int depth, A_exp e);
static void transDec(S_table env, int depth, A_dec d);
static void transVar(S_table env, int depth, A_var v);

void Esc_findEscape(A_exp exp) {
  //EscapeEntry(d, &(x->escape));
}
