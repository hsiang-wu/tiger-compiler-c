#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"

static T_expList Tr_expList_convert(Tr_expList l);
static Tr_exp Tr_StaticLink(Tr_level now, Tr_level def);
struct Tr_access_ {
	//Lab5: your code here
    Tr_level level;
    F_access access;
};

struct Tr_accessList_ {
	Tr_access head;
	Tr_accessList tail;	
};

struct Tr_level_ {
	//Lab5: your code here
    F_frame frame;
    Tr_level parent;
    Temp_label label;
    Tr_accessList formals;
};

struct Cx {patchList trues; patchList falses; T_stm stm;};

struct Tr_exp_ {
	//Lab5: your code here
  enum {Tr_ex, Tr_nx, Tr_cx} kind;
  union {T_exp ex; T_stm nx; struct Cx cx; } u;
};

struct Tr_expList_ {
  Tr_exp head;
  Tr_expList tail;
};

static Tr_exp Tr_Ex(T_exp ex)
{
  Tr_exp e = checked_malloc(sizeof(*e));
  e->kind = Tr_ex;
  e->u.ex = ex;
  return e;
}

static Tr_exp Tr_Nx(T_stm nx)
{
  Tr_exp e = checked_malloc(sizeof(*e));
  e->kind = Tr_nx;
  e->u.nx = nx;
  return e;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm)
{
  Tr_exp e = checked_malloc(sizeof(*e));
  e->kind = Tr_cx;
  e->u.cx.trues = trues;
  e->u.cx.falses = falses;
  e->u.cx.stm = stm;
  return e;
}

struct patchList_ {Temp_label *head; patchList tail;};
static patchList PatchList(Temp_label *head, patchList tail)
{
  patchList pl = checked_malloc(sizeof(*pl));
  pl->head = head;
  pl->tail = tail;
  return pl;
}

Tr_accessList AccTail(Tr_accessList accs) { return accs->tail; }
Tr_access AccHead(Tr_accessList accs) { return accs->head; }

// so the name _outermost is preserved
Tr_level Tr_outermost(void)
{
  static Tr_level outermost = NULL;
  if (outermost == NULL) {
    outermost = checked_malloc(sizeof(*outermost));
    Temp_label label = S_Symbol("_outermost");
    outermost->label = label;
    outermost->frame = F_newFrame(label, NULL);
  }
  return outermost;
}

Tr_level Tr_newLevel(Tr_level parent, Temp_label label,
    U_boolList fmls)
{
  Tr_level newl = checked_malloc(sizeof(*newl));
  printf("call new frame\n");
  newl->frame = F_newFrame(label, U_BoolList(TRUE/* static link */, fmls));
  printf("called\n");
  newl->parent = parent;
  newl->label = label;

  F_accessList fal = F_formals(newl->frame);
  Tr_accessList al = checked_malloc(sizeof(*al));
  Tr_accessList head = al;
  assert(fal);
  for (; fal->tail; fal = fal->tail) {
    al->head = checked_malloc(sizeof(*al->head));;
    al->head->level = newl;
    al->head->access = fal->head;
    al->tail = checked_malloc(sizeof(*al));
    al = al->tail;
  }
  // special for last
  al->head = checked_malloc(sizeof(*al->head));;
  al->head->level = newl;
  al->head->access = fal->head;
  al->tail = NULL;

  newl->formals = head;

  return newl;
}

Tr_accessList Tr_formals(Tr_level level)
{
  return level->formals;
}

Tr_access Tr_allocLocal(Tr_level level, bool escape)
{
  Tr_access ta = checked_malloc(sizeof(*ta));
  ta->level = level;
  ta->access = F_allocLocal(level->frame, escape);
  return ta;
}

void doPatch(patchList tList, Temp_label label) 
{ 
  for (; tList; tList=tList->tail)
    *(tList->head) = label;
}

patchList joinPatch(patchList first,patchList second) 
{ 
  if (!first) return second;
  for (; first->tail; first=first->tail); /* go to end of list */ 
  first->tail = second;
  return first;
}

static T_exp unEx(Tr_exp e)
{ 
  switch (e->kind) {
    case Tr_ex:
      return e->u.ex;
    case Tr_cx: 
      {
      Temp_temp r = Temp_newtemp();
      Temp_label t = Temp_newlabel(), f = Temp_newlabel(); 
      doPatch(e->u.cx.trues, t);
      doPatch(e->u.cx.falses, f);
      return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
               T_Eseq(e->u.cx.stm,
                T_Eseq(T_Label(f),
                  T_Eseq(T_Move(T_Temp(r), T_Const(0)), 
                    T_Eseq(T_Label(t),
                            T_Temp(r))))));
      }
    case Tr_nx:
      return T_Eseq(e->u.nx, T_Const(0)); 
  }
  assert(0); 
}

static T_stm unNx(Tr_exp e)
{
  switch (e->kind) {
    case Tr_ex:
      return T_Exp(e->u.ex);
    case Tr_cx: 
      {
        /* XXX */
        return e->u.cx.stm; 
      }
    case Tr_nx:
      return e->u.nx; 
  }
  assert(0); 
}

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail)
{
  Tr_expList el = checked_malloc(sizeof(*el));
  el->head = head;
  el->tail = tail;
  return el;
}


void Tr_elAppend(Tr_exp e, Tr_expList tail)
{
  if (tail == NULL)
    tail = Tr_ExpList(e, NULL);
  else
    tail = tail->tail = Tr_ExpList(e, NULL);
}


static struct Cx unCx(Tr_exp e)
{
  switch (e->kind) {
    case Tr_ex:
      {
        Temp_label t = Temp_newlabel(), f = Temp_newlabel(); 
        return unCx(Tr_Cx(PatchList(&t, NULL), PatchList(&f, NULL), 
                      T_Cjump(T_ne, e->u.ex, T_Const(0), t, f)));
      }
    case Tr_cx: 
      {
        return e->u.cx; 
      }
    case Tr_nx:
      assert(0);
  }
  assert(0); 
}

Tr_exp Tr_simpleVar(Tr_access acc, Tr_level lvl)
{
  //if (access->level == level)
  //  return Tr_Ex(F_Exp(access->access, T_Temp(F_FP())));
  T_exp addr = T_Temp(F_FP()); /*addr is frame point*/
  while (lvl != acc->level) { /* XXX: acc->level or acc->level->parent? */
    F_access link = F_formals(lvl->frame)->head; // first argument
    addr = F_Exp(link, addr);
    lvl = lvl->parent;
  }
  return Tr_Ex(F_Exp(acc->access, addr)); /* return val include frame point & offs */
}

Tr_exp Tr_subscriptVar(Tr_exp var, Tr_exp ind)
{
  return Tr_Ex(T_Mem(T_Binop(T_plus,
                      unEx(var),
                      T_Binop(T_mul, unEx(ind), T_Const(F_wordSize)))));
}

Tr_exp Tr_fieldVar(Tr_exp base, int offs) {
  // return base + offs * WORD-SIZE
  // remember that  record field is reversed.
  return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(base), T_Const(offs * F_wordSize))));
}

Tr_exp Tr_constVar(int i)
{
  return Tr_Ex(T_Const(i));
}

Tr_exp Tr_callExp(Temp_label label, Tr_level fun, Tr_level call, Tr_expList el_reversed) 
{
  T_expList args = NULL;
  Tr_exp sl = Tr_StaticLink(call, fun);
  el_reversed = Tr_ExpList(sl, el_reversed);
  args = Tr_expList_convert(el_reversed); // it's all reversed. so actually (somehow) static link is the last para.
  //return Tr_Ex(T_Eseq(T_Move(T_Temp(F_RV()), T_Call(T_Name(label), args)), 
  //     T_Temp(F_RV())));
  return Tr_Ex(T_Call(T_Name(label), args));;
}

Tr_exp Tr_arithExp(A_oper op, Tr_exp left, Tr_exp right) { /* (+, -, *, /) */
  T_binOp opp;
  switch(op) {
  case A_plusOp  : opp = T_plus; break;
  case A_minusOp : opp = T_minus; break;
  case A_timesOp : opp = T_mul; break;
  case A_divideOp: opp = T_div; break;
  default: assert(0);
  }
  return Tr_Ex(T_Binop(opp, unEx(left), unEx(right)));
}

Tr_exp Tr_compExp(A_oper op, Tr_exp left, Tr_exp right) {
  T_relOp oper;
  switch(op) {
    case A_eqOp: oper = T_eq; break;
    case A_neqOp: oper = T_ne; break;
    case A_ltOp: oper = T_lt; break;
    case A_leOp: oper = T_le; break;
    case A_gtOp: oper = T_gt; break;
    case A_geOp: oper = T_ge; break;
    default: assert(0);
  }
  T_stm cond = T_Cjump(oper, unEx(left), unEx(right), 
      Temp_newlabel(), Temp_newlabel());
  patchList trues = PatchList(&cond->u.CJUMP.true, NULL);
  patchList falses = PatchList(&cond->u.CJUMP.false, NULL);
  return Tr_Cx(trues, falses, cond);
}

// !!! fields are reversedly stored in mem
Tr_exp Tr_recordExp(Tr_expList el_reversed) {
  Temp_temp r = Temp_newtemp();
  int i = 0;
  Tr_expList l;
  T_stm seq;
  T_stm *alloc = &seq; // pre-alloced place for alloc.
  for (l = el_reversed; l; l = l->tail, i++) {
    seq = T_Seq(seq, T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(i * F_wordSize))), 
               unEx(l->head)));
  }
  /*alloc len * WORD-SIZE mem*/
  *alloc = T_Move(T_Temp(r),
                   F_externalCall(String("initRecord"), T_ExpList(T_Const(i * F_wordSize), NULL)));

  return Tr_Ex(T_Eseq(seq, T_Temp(r)));
}

Tr_exp Tr_arrayExp(Tr_exp init, Tr_exp size)
{
  return Tr_Ex(F_externalCall(String("initArray"), 
              T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}

Tr_exp Tr_assignExp(Tr_exp lval, Tr_exp exp) 
{ return Tr_Nx(T_Move(unEx(lval), unEx(exp))); }

Tr_exp Tr_breakExp(Tr_exp b) 
{ 
  // jumps to done
  return Tr_Nx(T_Jump(T_Name(unEx(b)->u.NAME), 
        Temp_LabelList(unEx(b)->u.NAME, NULL)));
}

// XXX
static Tr_exp Tr_StaticLink(Tr_level now, Tr_level def) {
  /* get call-function's static-link */
  T_exp addr = T_Temp(F_FP());/* frame-point */
  while(now && (now != def)) { /* until find the level which def the function */
    F_access sl = F_formals(now->frame)->head;
    addr = F_Exp(sl, addr);
    now = now->parent;
  }
  return Tr_Ex(addr);
}

static T_expList Tr_expList_convert(Tr_expList l) {
  /*trans Tr_expList to T_expList*/
  T_expList head = NULL, t = NULL;
  for (; l; l = l->tail) {
    T_exp tmp = unEx(l->head);
    if (head != NULL) {
      t->tail =  T_ExpList(tmp, NULL);
      t = t->tail;
    } else {
      head = T_ExpList(tmp, NULL);
      t = head;
    }   
  }
  return head;
}


// Reverse the l.
Tr_exp Tr_seqExp(Tr_expList l) {
  T_exp resl = unEx(l->head); /* resl cant be NULL */
  for (l = l->tail; l; l = l->tail) {
    resl = T_Eseq(T_Exp(unEx(l->head)), resl);
  }
  return Tr_Ex(resl);
}


Tr_exp Tr_nilExp() {
  static Temp_temp nilTemp = NULL;
  if (!nilTemp) {
    nilTemp = Temp_newtemp(); // so we can compare nil
    /* XXX */
    T_stm alloc = T_Move(T_Temp(nilTemp),
                     F_externalCall(String("initRecord"), T_ExpList(T_Const(0), NULL)));
    return Tr_Ex(T_Eseq(alloc, T_Temp(nilTemp)));
  } else {
    return Tr_Ex(T_Temp(nilTemp));
  }
}

Tr_exp Tr_whileExp(Tr_exp test, Tr_exp body, Tr_exp done) 
{
  Temp_label testl = Temp_newlabel(), bodyl = Temp_newlabel();
  return Tr_Ex(T_Eseq(T_Jump(T_Name(testl), Temp_LabelList(testl, NULL)), 
                T_Eseq(T_Label(bodyl),
                 T_Eseq(unNx(body),
                      T_Eseq(T_Label(testl),
                             T_Eseq(T_Cjump(T_eq, unEx(test), T_Const(0), unEx(done)->u.NAME, bodyl),
                                T_Eseq(T_Label(unEx(done)->u.NAME), T_Const(0))))))));
}

Tr_exp Tr_doneExp() 
{ 
  return Tr_Ex(T_Name(Temp_newlabel())); 
} 

Tr_exp Tr_forExp(Tr_exp lo, Tr_exp hi, Tr_exp body, Tr_exp done)
{
  // TODO: fill this ! 
  // it's not used since we translate for to while in semant.c
  return NULL;
}

Tr_exp Tr_ifExp(Tr_exp cond, Tr_exp then, Tr_exp elsee)
{
  unCx(cond);

  Temp_temp r = Temp_newtemp();
  Temp_label t = Temp_newlabel();
  Temp_label f = Temp_newlabel();
  struct Cx condcx = unCx(cond);
  doPatch(condcx.trues, t);
  doPatch(condcx.falses, f);
  return Tr_Ex(T_Eseq(T_Move(T_Temp(r),unEx(then)),
          T_Eseq(condcx.stm,
            T_Eseq(T_Label(f), 
              T_Eseq(T_Move(T_Temp(r), unEx(elsee)),
                  T_Eseq(T_Label(t), T_Temp(r)))))));
}

static F_fragList fragList = NULL;
static F_fragList stringFragList = NULL;

static int debug_proc_count = 0;
static int debug_str_count = 0;
void Tr_procEntryExit(Tr_level level, Tr_exp body) {
  //printf("p %d\n", ++debug_proc_count);
  // move the value of body to RV(return value register)
  T_stm procstm = F_procEntryExit1(level->frame, T_Move(T_Temp(F_RV()), unEx(body)));
  fragList = F_FragList(F_ProcFrag(procstm, level->frame), fragList);
}

Tr_exp Tr_stringVar(string lit)
{
  //printf("s %d : %s\n", ++debug_str_count, lit);
  Temp_label lab = Temp_newlabel();
  F_frag fragment = F_StringFrag(lab, lit);
  stringFragList = F_FragList(fragment, stringFragList);
  return Tr_Ex(T_Name(lab));
}

F_fragList Tr_getFragList()
{
  F_fragList cur = stringFragList;
  if (!cur) return fragList;
  for (; cur->tail; cur = cur->tail) { /* to end */; }

  cur->tail = fragList;
  return stringFragList;
}
