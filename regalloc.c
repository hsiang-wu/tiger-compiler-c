#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "color.h"
#include "liveness.h"
#include "regalloc.h"
#include "table.h"

#include "flowgraph.h"

struct RA_result RA_regAlloc(F_frame f, AS_instrList il) {
	//your code here.
	struct RA_result ret;
    G_graph flowgraph = FG_AssemFlowGraph(il, f);
    struct Live_graph livegraph = Live_liveness(flowgraph);

    struct COL_result cr = COL_color(livegraph.graph, F_tempMap, F_registers());

    ret.coloring = cr.coloring;
    ret.il = il;
	return ret;
}
