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
#include <stdio.h>

#include "flowgraph.h"

struct RA_result
RA_regAlloc(F_frame f, AS_instrList il)
{
  // your code here.
  struct RA_result ret;
  G_graph flowgraph = FG_AssemFlowGraph(il, f);
  struct Live_graph livegraph = Live_liveness(flowgraph);

  Temp_tempList regs = F_registers();
  struct COL_result cr = COL_color(livegraph.graph, F_tempMap, regs);

  ret.coloring = cr.coloring;

  if (cr.spills) {
    printf("====need spilling====\n");
    Temp_printList(cr.spills);
    printf("=======\n");
  }
  cr.spills = NULL;

  ret.il = il;
  return ret;
}
