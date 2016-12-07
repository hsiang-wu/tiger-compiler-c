#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"

/*Lab5: Your implementation here.*/
const int F_wordSize = 4;

struct F_frame_ {
  Temp_label name;
  F_accessList accessList;
  int off;
};

struct F_access_ {
    enum {inFrame, inReg} kind;
    union {
      int offset; /* InFrame */
      Temp_temp reg; /* InReg */
    } u;
};

static F_access InFrame(int offset)
{
  F_access access = checked_malloc(sizeof(*access));
  access->kind = inFrame;
  access->u.offset = offset;
  return access;
}

static F_access InReg(Temp_temp reg)
{
  F_access access = checked_malloc(sizeof(*access));
  access->kind = inReg;
  access->u.reg = reg;
  return access;
}

Temp_label F_name(F_frame f)
{
  return f->name;
}

F_accessList F_formals(F_frame f)
{
  return f->accessList;
}

F_access F_allocLocal(F_frame f, bool escape)
{
    if (escape) { /* must be place on stack */
      f->off -= F_wordSize;
      printf("frame offset:%d\n", f->off);
      return InFrame(f->off);
    } { /* LOL, also put on stack. But CAN be put in register */
      f->off -= F_wordSize;
      return InFrame(f->off);
    }
}

static F_access F_allocParam(F_frame f, bool escape)
{
    if (escape) { /* must be place on stack */
      f->off += F_wordSize;
      printf("frame offset:%d\n", f->off);
      return InFrame(f->off);
    } { /* LOL, also put on stack. But CAN be put in register */
      f->off += F_wordSize;
      return InFrame(f->off);
    }
}

F_frame F_newFrame(Temp_label name, U_boolList formals)
{
  F_frame frame = checked_malloc(sizeof(*frame));
  frame->name = name;
  frame->off = 4;

  if (!formals) return frame; // only happens in outermost

  F_accessList al = checked_malloc(sizeof(*al));
  F_accessList head = al;
  for (; formals->tail; formals = formals->tail) {
    printf("  frame:loop body\n");
    bool escape = formals->head;
    al->head = F_allocParam(frame, escape);
    al->tail = checked_malloc(sizeof(*al));
    al = al->tail;
  }
    printf("  frame:loop tail\n");
  al->head = F_allocParam(frame, formals->head);
  al->tail = NULL;
  frame->accessList = head;
  frame->off = 0; // set to -4, cuz it's > ebp but local vars lives < ebp. and -4(%ebp) is %esp.
  return frame;
}

F_frag F_StringFrag(Temp_label label, string str) {
    F_frag frg = checked_malloc(sizeof(*frg));
    frg->kind = F_stringFrag;
    frg->u.stringg.label = label;
    frg->u.stringg.str = str;
	return frg;
}

F_frag F_ProcFrag(T_stm body, F_frame frame) {
    F_frag frg = checked_malloc(sizeof(*frg));
    frg->kind = F_procFrag;
    frg->u.proc.body = body;
    frg->u.proc.frame = frame;
	return frg;
}

F_fragList F_FragList(F_frag head, F_fragList tail) {
    F_fragList fl = checked_malloc(sizeof(*fl));
    fl->head = head;
    fl->tail = tail;
	return fl;
}

T_stm F_procEntryExit1(F_frame frame, T_stm stm)
{
  assert(frame->name);
  return T_Seq(T_Label(frame->name), stm);
}

Temp_temp F_FP(void)
{
  static Temp_temp fp = NULL;
  if (fp == NULL)
    fp = Temp_newtemp();
  return fp;
}

Temp_temp F_RV(void)
{
  static Temp_temp fp = NULL;
  if (fp == NULL)
    fp = Temp_newtemp();
  return fp;
}

Temp_temp F_SP(void)
{
  static Temp_temp sp = NULL;
  if (sp == NULL)
    sp = Temp_newtemp();
  return sp;
}

T_exp F_Exp(F_access acc, T_exp framePtr)
{
  if (acc->kind == inFrame) {
    return T_Mem(T_Binop(T_plus,
                        framePtr, T_Const(acc->u.offset)));
  } else { // in register
    return framePtr;
  }
}

T_exp F_externalCall(string str, T_expList args)
{

  //T_stm passparam = NULL;
  //for (; args; args=args->tail) { // reverse the reverse. a stack shuold be arg3..arg2..arg1..eip..ebp
  //  if (!passparam) {
  //    passparam = T_Push(args->head);
  //  } else {
  //    passparam = T_Seq(passparam, T_Push(args->head));
  //  }
  //}

  return T_Call(T_Name(Temp_namedlabel(str)), args);
}

Temp_tempList F_registers()
{

  return NULL;
}

F_frag F_string(Temp_label lab, string lit)
{
    F_frag frg = checked_malloc(sizeof(*frg));
    frg->kind = F_stringFrag;
    frg->u.stringg.label = lab;

    string s = checked_malloc(strlen(lit) + 4);
    *(int *) s = (int) strlen(lit);
    //printf("len:%d\n", *(int*)s);
    //*(int *) (s+4) = (int) strlen(lit);
    strcpy(s+4, lit);

    //fwrite(s, 1, strlen(lit)+4, stdout);
    frg->u.stringg.str = s;
	return frg;
}

AS_proc F_procEntryExit3(F_frame frame, AS_instrList body)
{
  AS_proc proc = checked_malloc(sizeof(*proc));
  assert(body->head->kind == I_LABEL);
  string fname = body->head->u.LABEL.assem;
  char *r = checked_malloc(64);
  sprintf(r, "%s\tpushl\t%%ebp\n\tmovl\t%%esp, %%ebp\n", fname);
  proc->prolog = r;
  body = body->tail;
  proc->body = body;
  proc->epilog = "\tleave\n\tret\n";
  return proc;
}
