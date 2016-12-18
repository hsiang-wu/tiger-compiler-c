#include "regalloc.h"
#include "absyn.h"
#include "assem.h"
#include "color.h"
#include "frame.h"
#include "graph.h"
#include "liveness.h"
#include "symbol.h"
#include "table.h"
#include "temp.h"
#include "tree.h"
#include "util.h"
#include <stdint.h>
#include <stdio.h>

#include "codegen.h"
#include "flowgraph.h"

static double heuristic(Temp_temp t);
static void insert_after(AS_instrList il, AS_instr i);
static void rewrite_inst(Temp_tempList tl, Temp_temp oldt, Temp_temp newt);

static void
insert_after(AS_instrList il, AS_instr i)
{
  assert(il && "il should not be NULL");
  AS_instrList tmp = AS_InstrList(i, il->tail);
  //  il->tail->head = i;
  //  il->tail->tail = tmp;
  il->tail = tmp;
}

static void
rewrite_inst(Temp_tempList tl, Temp_temp oldt, Temp_temp newt)
{
  bool D_found = FALSE;
  for (; tl; tl = tl->tail) {
    if (tl->head == oldt) {
      tl->head = newt;
      D_found = TRUE;
    }
  }
  assert(D_found);
}

enum { AS_USE, AS_DEF };
static void
rewrite(Temp_temp spill, AS_instrList il, F_frame f)
{
  // re-write code
  AS_instr i;
  AS_instrList last = NULL;
  Temp_tempList uses;
  Temp_tempList defs;
  Temp_tempList nextuses;

  F_access inmem = F_allocLocal(f, TRUE); // ESCAPE=TRUE. it's on stack
  printf("rewrite\n");
  bool D_use_before_def = TRUE;
  for (; il; last = il, il = il->tail) {

    i = il->head;
    switch (i->kind) {
      case I_MOVE:
        uses = i->u.MOVE.src;
        defs = i->u.MOVE.dst;
        break;
      case I_OPER: {
        uses = i->u.OPER.src;
        defs = i->u.OPER.dst;
        break;
      }
      case I_LABEL:
        continue;
    }
    for (; uses; uses = uses->tail) {
      if (spill == uses->head) {
        assert(!D_use_before_def && "Use before def!");
        if (!D_use_before_def) {

          Temp_temp t0 = Temp_newtemp();
          rewrite_inst(uses, spill, t0);
          // load before use.
          char* buffer = checked_malloc(64);
          sprintf(buffer, "movl %d(%%ebp), `d0 #spill\n", F_frameOffset(inmem));
          printf("%s%d\n", buffer, Temp_num(t0));
          insert_after(last, AS_Oper(buffer, L(t0, NULL), NULL, NULL));
        }
        // replace spill with t0 in the remaining of uses list.
        break;
      }
    }
    for (; defs; defs = defs->tail) {
      if (spill == defs->head) {
        D_use_before_def = FALSE;
        // store after use
        char* buffer = checked_malloc(64);
        Temp_temp t0 = Temp_newtemp();
        // replace spill with t0.
        rewrite_inst(defs, spill, t0);
        sprintf(buffer, "movl `s0, %d(%%ebp) #spill\n", F_frameOffset(inmem));
        insert_after(il, AS_Oper(buffer, NULL, L(t0, NULL), NULL));
        printf("%s%d\n", buffer, Temp_num(t0));

        //// usually there's a empty use immedieatly after each def.
        //// refer to codegen.c : emit().
        // assert(il->tail);
        // assert(il->tail->head);
        // assert(il->tail->head->u.OPER.src);
        // rewrite_inst(il->tail->head->u.OPER.src, spill, t0);

        // so insert after that
        // move it to memory
        // replace spill with t0 in the remaining of uses list.
        break;
      }
    }
  }
}

static void
rewrite_programm(Temp_tempList tl, AS_instrList il, F_frame f)
{
  Temp_printList(tl);
  for (; tl; tl = tl->tail) {
    Temp_temp t = tl->head;
    printf("Spill temporary:");
    Temp_print(t);
    rewrite(t, il, f);
    printf("=======\n");
  }
}

struct RA_result
RA_regAlloc(F_frame f, AS_instrList il)
{
  // your code here.
  struct RA_result ret;

  int debug = 0;
  int upper = 5;
  for (; debug < upper; debug++) {
    G_graph flowgraph = FG_AssemFlowGraph(il, f);
    struct Live_graph livegraph = Live_liveness(flowgraph);

    Temp_tempList regs = F_registers();
    struct COL_result cr = COL_color(livegraph.graph, F_tempMap, regs);

    ret.coloring = cr.coloring;

    if (!cr.spills) break;

    printf("====actual spilling====\n");
    rewrite_programm(cr.spills, il, f);
  }
  assert(debug != upper && "spill error");

  ret.il = il;
  return ret;
}
