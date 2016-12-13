#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "errormsg.h"
#include "tree.h"
#include "assem.h"
#include "frame.h"
#include "codegen.h"
#include "table.h"

//Lab 6: your code here
#define BLEN 32 // buffer length

static Temp_temp munchExp(T_exp e);
static void munchStm(T_stm s);
static Temp_tempList munchArgs(int i, T_expList args);
static void emit(AS_instr inst);

extern Temp_map F_tempMap;

Temp_tempList L(Temp_temp h, Temp_tempList t) {return Temp_TempList(h, t);}

static AS_instrList iList=NULL, last=NULL;
static void emit(AS_instr inst)
{
  if (last != NULL)
    last = last->tail = AS_InstrList(inst, NULL);
  else last = iList = AS_InstrList(inst, NULL);
}

AS_instrList F_codegen(F_frame f, T_stmList stmList) {
  AS_instrList list; T_stmList s1;

  Temp_enter(F_tempMap, F_RV(), "%eax");
  Temp_enter(F_tempMap, F_FP(), "%ebp");
  Temp_enter(F_tempMap, F_SP(), "%esp");

  for (s1 = stmList; s1; s1 = s1->tail) munchStm(s1->head);

  // mark caller-save regs as "define"
  emit(AS_Oper("nop", NULL, F_CallerSaves()/* src */, NULL));
  list = iList;
  iList = last = NULL;
  return list;
}


static Temp_temp munchExp(T_exp e)
{
  char *buffer = checked_malloc(BLEN);
  Temp_temp r = Temp_newtemp(); 
  int i; T_exp e1, e2; // only a part of them will be used in each instruction.
  switch (e->kind) {
    case T_MEM: 
      {
        T_exp adr = e->u.MEM;
        switch (adr->kind) { 
          case T_BINOP:
            {
              assert(adr->u.BINOP.op == T_plus);
              T_exp lt = adr->u.BINOP.left, rt = adr->u.BINOP.right;

              if (lt->kind != T_CONST && rt->kind != T_CONST) { // inefficient?
                sprintf(buffer, "\tmovl\t(`s0), `d0\n");
                emit(AS_Oper(buffer, L(r, NULL), L(munchExp(adr), NULL), NULL));
                return r;
              }

              // else: MEM(+, x, x) is the only kind of addressing!
              if (lt->kind == T_CONST) {
                i = lt->u.CONST; e1 = rt;
              } else {
                assert(rt->kind == T_CONST);
                i = rt->u.CONST; e1 = lt;
              }
              sprintf(buffer, "\tmovl\t%d(`s0), `d0\n", i);
              emit(AS_Oper(buffer, L(r, NULL)/* dst */, 
                    L(munchExp(e1) /* src */, NULL), NULL));
              return r; 
            } 
          case T_CONST:
            {
              i = adr->u.CONST;
              sprintf(buffer, "\tmovl\t0x%x, `d0\n", i); // withou $: content of address
              emit(AS_Oper(buffer, L(r, NULL), NULL/* no srcreg */, NULL));
              return r; 
            }
          default: // MEM(e1)
            {
              sprintf(buffer, "\tmovl\t(`s0), `d0\n");
              emit(AS_Oper(buffer, L(r, NULL), L(munchExp(adr), NULL), NULL));
              return r; 
            }
        }
        assert(0); // never reached
      }
    case T_BINOP: 
      {
        T_exp lt = e->u.BINOP.left, rt = e->u.BINOP.right;
        switch (e->u.BINOP.op) {
          case T_plus :
            {
              if (lt->kind == T_CONST && rt->kind == T_CONST) {
                sprintf(buffer, "movl $%d `d0\n", lt->u.CONST + rt->u.CONST);
                emit(AS_Oper(buffer, L(r, NULL), NULL, NULL));
                return r; 
              //} else if (lt->kind == T_CONST) {
              //  // optimized. TODO: rt->kind == T_CONST
              //  sprintf(buffer, "movl $%d, %%eax\n\
              //                   addl `s0, %%eax\n\
              //                   movl %%eax, `d0\n", lt->u.CONST);
              //  emit(AS_Oper(buffer, L(r, NULL), NULL, NULL));
              //  return r; 
              } else {
                sprintf(buffer, "\tmovl\t`s0, `d0\n");
                emit(AS_Oper(buffer, L(r, NULL), L(munchExp(lt), NULL), NULL));

                buffer = checked_malloc(BLEN);
                sprintf(buffer, "\taddl\t`s0, `d0\n");
                emit(AS_Oper(buffer, L(r, NULL), L(munchExp(rt), L(r, NULL)), NULL)); 
                // the second src is strange: x := x+1, x still in "in"
                return r; 
              }
              assert(0);
            }
          case T_minus : 
            {
              // r = s1 - s2
              // subl %s1, %s2 : s2 = s2 - s1
              // Temp_temp l = munchExp(lt);
              // Temp_temp r = munchExp(rt);
              sprintf(buffer, "\tmovl\t`s0, `d0\n");
              emit(AS_Oper(buffer, L(r, NULL), L(munchExp(lt), NULL), NULL));

              buffer = checked_malloc(BLEN);
              sprintf(buffer, "\tsubl\t`s0, `d0\n");
              emit(AS_Oper(buffer, L(r, NULL), L(munchExp(rt), L(r, NULL)), NULL));
              return r; 
            }
          case T_mul :
            {
              // r = s1 * s2
              sprintf(buffer, "\tmovl\t`s0, `d0\n");
              emit(AS_Oper(buffer, L(r, NULL), L(munchExp(lt), NULL), NULL));

              buffer = checked_malloc(BLEN);
              sprintf(buffer, "\timull\t`s0, `d0\n");
              emit(AS_Oper(buffer, L(r, NULL), L(munchExp(rt), L(r, NULL)), NULL));
              return r; 
            }
          case T_div :
            {
              // idiv S: eax <- [edx:eax] / S. ignore edx
              Temp_enter(F_tempMap, r, "%eax");

              sprintf(buffer, "\tmovl\t`s0, `d0\n");
              emit(AS_Oper(buffer, L(r, NULL), L(munchExp(lt), NULL), NULL));

              buffer = checked_malloc(BLEN);
              sprintf(buffer, "\tcltd\n");
              emit(AS_Oper(buffer, NULL, NULL, NULL));

              buffer = checked_malloc(BLEN);
              sprintf(buffer, "\tidiv\t`s0\n");
              AS_instr debug = AS_Oper(buffer, NULL, L(munchExp(rt), L(r, NULL)), NULL);
              emit(debug);
              return r; // result in %eax
            }
          default:
            assert(0); // these ops are not implemented. so never reach here.
        }
      }
    case T_CONST:
      {
        sprintf(buffer, "\tmovl\t$%d, `d0\n", e->u.CONST); // with $: IMMediate.
        // XXX: different from book. I think author is wrong?
        emit(AS_Oper(buffer, L(r, NULL), NULL, NULL));
        return r; 
      }
    case T_TEMP:
      {
        return e->u.TEMP;
      }
    case T_NAME:
      {
        printf("warning: reach T_NAME!\n");
        sprintf(buffer, "\tmovl\t$%s, `d0\n", S_name(e->u.NAME));
        emit(AS_Oper(buffer, L(r, NULL), NULL, NULL));
        return r; 
      }
    case T_CALL:
      {
        assert(e->u.CALL.fun->kind == T_NAME); // we always call by name...(e.g. never "call *%eax")
        Temp_tempList l = munchArgs(0, e->u.CALL.args);
        assert(!l); // never pass by register.

        sprintf(buffer, "\tcall\t%s\n", Temp_labelstring(e->u.CALL.fun->u.NAME));
        emit(AS_Oper(buffer, F_CallerSaves(), l, NULL));
        return F_RV();
      }

    case T_ESEQ: // cano promised no eseq
      assert(0);
  }
}

static Temp_tempList munchArgs(int i, T_expList args)
{
  if (!args) return NULL;
  // we receive a args of reversed order, so just printit here. i=0 for argsN, i=N for args0
  char *buffer = checked_malloc(BLEN);
  sprintf(buffer, "\tpush\t`s0\n");
  emit(AS_Oper(buffer, NULL, L(munchExp(args->head), NULL), NULL));

  return munchArgs(i+1, args->tail);
}

static void munchStm(T_stm s)
{
  char *buffer = checked_malloc(BLEN);
  int i; T_exp e1, e2;
  switch (s->kind) {
    case T_MOVE:
      {
        switch (s->u.MOVE.dst->kind) {
          case T_MEM:
            {
              T_exp mem = s->u.MOVE.dst->u.MEM;
              e2 = s->u.MOVE.src;
              switch (mem->kind)
              {
                case T_BINOP:
                  {
                    assert(mem->u.BINOP.op == T_plus);
                    T_exp lt = mem->u.BINOP.left, rt = mem->u.BINOP.right;

                    if (lt->kind != T_CONST && rt->kind != T_CONST) { // inefficient?
                      sprintf(buffer, "\tmovl\t`s1, (`s0)\n");
                      emit(AS_Oper(buffer, NULL, L(munchExp(mem), L(munchExp(e2), NULL)), NULL));
                      return;
                    }

                    if (lt->kind == T_CONST) {
                      assert(rt->kind != T_CONST); // XXX: is this correct?
                      i = lt->u.CONST; e1 = rt;
                    } else {
                      i = rt->u.CONST; e1 = lt;
                    }
                    sprintf(buffer, "\tmovl\t`s1, %d(`s0)\n", i);
                    emit(AS_Oper(buffer, NULL,
                          L(munchExp(e1), L(munchExp(e2), NULL)), NULL));
                    return; 
                  }
                case T_CONST: // fall through! since we can't movel e, $CONST, but have to store const in a reg.
                default: // MOVE(MEM(e1), e2)
                  {
                    sprintf(buffer, "\tmovl\t`s1, (`s0)\n");
                    emit(AS_Oper(buffer, NULL,
                          L(munchExp(mem), L(munchExp(e2), NULL)), NULL));
                    return; 
                  }
              }
            }
          case T_TEMP:
            {
              Temp_temp dst = s->u.MOVE.dst->u.TEMP;
              sprintf(buffer, "\tmovl\t`s0, `d0\n");
              emit(AS_Move(buffer, L(dst, NULL), L(munchExp(s->u.MOVE.src), NULL)));
              return; 
            }
          default:
            assert(0);
        }
      }
    case T_LABEL:
      {
        // attention to label & call.
        Temp_label lab = s->u.LABEL;
        sprintf(buffer, "%s:\n", Temp_labelstring(lab));
        emit(AS_Label(buffer, lab));
        return;
      }
    case T_JUMP:
      {
        assert(s->u.JUMP.exp->kind == T_NAME);
        sprintf(buffer, "\tjmp\t%s\n", Temp_labelstring(s->u.JUMP.exp->u.NAME));
        emit(AS_Oper(buffer, NULL, NULL, AS_Targets(s->u.JUMP.jumps)));
        return;
      }
    case T_CJUMP:
      {
        sprintf(buffer, "\tcmpl\t`s1, `s0\n"); // .. that's the correct answer
        emit(AS_Oper(buffer, NULL, L(munchExp(s->u.CJUMP.left), L(munchExp(s->u.CJUMP.right), NULL)), NULL));
        char *op;
        switch (s->u.CJUMP.op) {
          case T_eq:
            op = "je";
            break;
          case T_ne:
            op = "jne";
            break;
          case T_lt:
            op = "jl";
            break;
          case T_le:
            op = "jle";
            break;
          case T_gt:
            op = "jg";
            break;
          case T_ge:
            op = "jge";
            break;
          default:
            assert(0); 
        }
        buffer = checked_malloc(BLEN);
        sprintf(buffer, "\t%s\t%s\n", op, Temp_labelstring(s->u.CJUMP.true)); // jump to true
        emit(AS_Oper(buffer, NULL, NULL, AS_Targets(Temp_LabelList(s->u.CJUMP.true, NULL))));

        buffer = checked_malloc(BLEN);
        sprintf(buffer, "\t%s\t%s\n", op, Temp_labelstring(s->u.CJUMP.false)); // jump to false
        emit(AS_Oper(buffer, NULL, NULL, AS_Targets(Temp_LabelList(s->u.CJUMP.false, NULL))));
        return;
      }
    case T_EXP:
      {
        munchExp(s->u.EXP);
        return;
      }
    case T_SEQ:
    case T_PUSH:
      assert(0 && "Should be no T_PUSH or T_SEQ");
  }
}
