#include "symbol.h"

#include "util.h"

#include "types.h"
#include "env.h"

#include <stdio.h>

/*Lab4: Your implementation of lab4*/

E_enventry
E_VarEntry(Tr_access access, Ty_ty ty)
{
  E_enventry e = checked_malloc(sizeof(*e));
  e->kind = E_varEntry;
  e->u.var.ty = ty;
  e->u.var.access = access;
  return e;
}

E_enventry
E_FunEntry(Tr_level level, Temp_label label, Ty_tyList formals, Ty_ty result)
{
  E_enventry e = checked_malloc(sizeof(*e));
  e->kind = E_funEntry;
  e->u.fun.level = level;
  e->u.fun.label = label;
  e->u.fun.formals = formals;
  e->u.fun.result = result;
  return e;
}

S_table E_base_tenv(void) /* contains predefined functions */
{
  S_table tenv = S_empty();
  S_enter(tenv, S_Symbol("int"), Ty_Int());
  S_enter(tenv, S_Symbol("string"), Ty_String());
  return tenv;
}

#define ORD "ord"
#define GETCHAR "getchar"
#define CHR "chr"
#define PRINT "print"
#define PRINTI "printi"
#define END ""

char* externalCallNames[] = { ORD, GETCHAR, CHR, PRINT, PRINTI, END };

S_table
E_base_venv(void)
{
  S_table venv = S_empty();
  S_enter(venv, S_Symbol(ORD),
          E_FunEntry(Tr_outermost(), Temp_namedlabel(ORD),
                     Ty_TyList(Ty_String(), NULL), Ty_Int()));
  S_enter(
    venv, S_Symbol(GETCHAR),
    E_FunEntry(Tr_outermost(), Temp_namedlabel(GETCHAR), NULL, Ty_String()));
  S_enter(venv, S_Symbol(CHR),
          E_FunEntry(Tr_outermost(), Temp_namedlabel(CHR),
                     Ty_TyList(Ty_Int(), NULL), Ty_String()));
  S_enter(venv, S_Symbol(PRINT),
          E_FunEntry(Tr_outermost(), Temp_namedlabel(PRINT),
                     Ty_TyList(Ty_String(), NULL), Ty_Void()));
  S_enter(venv, S_Symbol(PRINTI),
          E_FunEntry(Tr_outermost(), Temp_namedlabel(PRINTI),
                     Ty_TyList(Ty_Int(), NULL), Ty_Void()));

  return venv;
}
