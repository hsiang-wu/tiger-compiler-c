#include "absyn.h"
#include "frame.h"
#include "symbol.h"
#include "table.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>

/*this model must before semant model*/
static void transExp(S_table env, int depth, A_exp e);
static void transDec(S_table env, int depth, A_dec d);
static void transVar(S_table env, int depth, A_var v);

typedef struct escentry_* escentry;

struct escentry_ {
  bool* escape;
  int depth;
};

escentry
EscEntry(bool* esc, int d)
{
  escentry eet = checked_malloc(sizeof(*eet));
  eet->escape = esc;
  eet->depth = d;
  return eet;
}

void
Esc_findEscape(A_exp exp)
{
  transExp(S_empty(), 0, exp);
}

static void
transExp(S_table env, int depth, A_exp e)
{
  switch (e->kind) {
    case A_varExp: {
      transVar(env, depth, e->u.var);
      return;
    }
    case A_callExp: {
      A_expList el = e->u.call.args;
      for (; el; el = el->tail) {
        transExp(env, depth, el->head);
      }
      return;
    }
    case A_opExp: {
      transExp(env, depth, e->u.op.left);
      transExp(env, depth, e->u.op.right);
      return;
    }
    case A_seqExp: {
      A_expList el = e->u.seq;
      for (; el; el = el->tail) {
        transExp(env, depth, el->head);
      }
      return;
      // return et;
    }
    case A_assignExp: {
      transVar(env, depth, e->u.assign.var);
      transExp(env, depth, e->u.assign.exp);
      return;
    }
    case A_ifExp: {
      transExp(env, depth, e->u.iff.test);
      transExp(env, depth, e->u.iff.then);
      if (e->u.iff.elsee != NULL) { // if-then-else
        transExp(env, depth, e->u.iff.elsee);
      }
      return;
    }
    case A_whileExp: {
      // not in test?
      // test
      transExp(env, depth, e->u.whilee.test);
      // body
      transExp(env, depth, e->u.whilee.body);
      return;
    }
    case A_forExp: {
      bool* useless = checked_malloc(sizeof(*useless));
      S_enter(env, e->u.forr.var, EscEntry(useless, depth));
      transExp(env, depth, e->u.forr.body);
    }
    case A_letExp: {
      A_decList d;
      S_beginScope(env);

      for (d = e->u.let.decs; d; d = d->tail) {
        // append in a reversed order. but don't worry. check return.
        transDec(env, depth, d->head);
      }
      transExp(env, depth, e->u.let.body);
      S_endScope(env);
      return;
    }
    case A_arrayExp: {
      transExp(env, depth, e->u.array.init);
      transExp(env, depth, e->u.array.size); // size
      return;
    }
    case A_recordExp: {
    }
    case A_nilExp: {
    }
    case A_intExp: {
    }
    case A_stringExp: {
    }
    case A_breakExp: {
      return;
    }
    default:;
  }
  assert(0);
}

static void
transDec(S_table env, int depth, A_dec d)
{
  switch (d->kind) {
    case A_varDec: {
      printf("dec:vardec:%p %p\n", d->u.var.var, d->u.var.typ);
      transExp(env, depth, d->u.var.init);
      // at first, all variable doesn't escape.
      // change it to true to test the compatability with old code.
      d->u.var.escape = FALSE; // should be false
      S_enter(env, d->u.var.var, EscEntry(&(d->u.var.escape), depth));

      return;
    }
    case A_typeDec: {
      return;
    }
    case A_functionDec: {
      printf("dec:funcdec:%p\n", d->u.type);
      A_fundec f;
      A_fundecList fl;
      // pre scan the headers
      for (fl = d->u.function; fl; fl = fl->tail) {
      }
      for (fl = d->u.function; fl; fl = fl->tail) {
        f = fl->head;
        S_beginScope(env);
        {
          A_fieldList l;
          // XXX: reconsider the reverse/sequence of parameters.
          for (l = f->params; l; l = l->tail) {
            l->head->escape =
              F_initParamEsc(); // All parameters escape, by IA32's convention.
            S_enter(env, l->head->name, EscEntry(&(l->head->escape), depth));
          }
        }
        printf("evaluate body exp!\n");
        transExp(env, depth + 1, f->body); // depth+1 for nested.
        S_endScope(env);
      }
      printf("\tfuncdec:3 finish%p\n", fl);
      return;
    }
  }
  //  assert(0);
}

static void
transVar(S_table env, int depth, A_var v)
{
  switch (v->kind) {
    case A_simpleVar: {
      escentry eet = S_look(env, v->u.simple);
      if (depth > eet->depth) *(eet->escape) = TRUE;
      return;
    }
    case A_fieldVar: {
      printf("fieldvar!\n");
      transVar(env, depth, v->u.field.var);
      return;
    }
    case A_subscriptVar: {
      printf("subscript!\n");
      transVar(env, depth, v->u.subscript.var);
      transExp(env, depth, v->u.subscript.exp);
      return;
    }
    defalut:;
  }
  assert(0);
}
