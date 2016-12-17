#include "flowgraph.h"
#include "absyn.h"
#include "assem.h"
#include "errormsg.h"
#include "frame.h"
#include "graph.h"
#include "symbol.h"
#include "table.h"
#include "temp.h"
#include "tree.h"
#include "util.h"
#include <stdio.h>

Temp_tempList
FG_def(G_node n)
{
  // your code here.
  assert(n && "Invalid n in FG_def");
  AS_instr i = G_nodeInfo(n);
  switch (i->kind) {
    case I_OPER:
      return i->u.OPER.dst;
    case I_LABEL:
      return NULL;
    case I_MOVE:
      return i->u.MOVE.dst;
  }
}

Temp_tempList
FG_use(G_node n)
{
  // your code here.
  assert(n && "Invalid n in FG_use");
  AS_instr i = G_nodeInfo(n);
  switch (i->kind) {
    case I_OPER: {
      return i->u.OPER.src;
    }
    case I_LABEL:
      return NULL;
    case I_MOVE:
      return i->u.MOVE.src;
  }
  return NULL;
}

bool
FG_isMove(G_node n)
{
  // your code here.
  assert(n && "Invalid n in FG_isMove");
  AS_instr i = G_nodeInfo(n);
  assert(i && "Invalid i in FG_isMove");
  return i->kind == I_MOVE;
}

static void
FG_addJumps(TAB_table t, G_node n)
{
  AS_instr i = (AS_instr)G_nodeInfo(n);
  if (!i->u.OPER.jumps) return;

  Temp_labelList tl = i->u.OPER.jumps->labels;
  G_node neighbour = NULL;
  for (; tl; tl = tl->tail) {
    neighbour = (G_node)TAB_look(t, tl->head);
    if (neighbour) {
      if (!G_goesTo(n, neighbour)) G_addEdge(n, neighbour);
    }
    else {
      printf("can't find label %s\n", S_name(tl->head));
      assert(0);
    }
  }
}

void
printInsNode(void* p)
{
  AS_instr i = p;
  char* names[3] = {[I_OPER] = "oper", [I_LABEL] = "label", [I_MOVE] = "move" };
  printf("\n%s", names[i->kind]);

  switch (i->kind) {
    case I_LABEL: {
      printf("%s\n", i->u.LABEL.assem);
      break;
    }
    case I_OPER: {
      printf("%s\n", i->u.OPER.assem);
      printf("FG_def:\n");
      Temp_printList(i->u.OPER.dst);
      printf("FG_use:\n");
      Temp_printList(i->u.OPER.src);
      break;
    }
    case I_MOVE: {
      printf("%s\n", i->u.MOVE.assem);
      printf("(move)FG_def:\n");
      Temp_printList(i->u.MOVE.dst);
      printf("(move)FG_use:\n");
      Temp_printList(i->u.MOVE.src);
      break;
    }
  }
}

static G_node
use_after_def(G_graph g, AS_instr i)
{
  Temp_tempList defs = NULL;
  G_node r = NULL;
  AS_instr pseudo_i;

  switch (i->kind) {
    case I_LABEL: {
      assert(0);
    }
    case I_MOVE: // fall through
      defs = i->u.MOVE.dst;
      // insert use after def.
      if (defs) {
        pseudo_i = AS_Move("", NULL, defs);
        r = G_Node(g, pseudo_i);
      }
      break;
    case I_OPER: {
      defs = i->u.OPER.dst;
      if (defs) {
        pseudo_i = AS_Oper("", NULL, defs, NULL);
        r = G_Node(g, pseudo_i);
      }
      break;
    }
  }
  return r;
}

G_graph
FG_AssemFlowGraph(AS_instrList il, F_frame f)
{
  // your code here.
  G_graph g = G_Graph();
  G_node curr = NULL, prev = NULL;
  G_nodeList nodes = NULL;
  TAB_table label_tb = TAB_empty();

  AS_instr i;
  for (; il; il = il->tail) {
    i = il->head;
    curr = G_Node(g, i);
    if (prev) G_addEdge(prev, curr);
    switch (i->kind) {
      case I_LABEL: {
        TAB_enter(label_tb, i->u.LABEL.label, curr);
        break; // add label->node maping for retrieve the node later
      }
      case I_MOVE: // fall through
      case I_OPER: {
        nodes = G_NodeList(curr, nodes);
        // create a pseudo use after def,
        // so that 0-length live range
        // DO interfere with others.
        // G_node uad = use_after_def(g, i);
        // uad = NULL;
        // if (uad) {
        //  G_addEdge(curr, uad);
        //  curr = uad;
        //}
        break;
      }
    }
    prev = curr;
  }

  for (; nodes; nodes = nodes->tail) {
    curr = nodes->head;
    if (((AS_instr)G_nodeInfo(curr))->kind == I_OPER)
      FG_addJumps(label_tb, curr);
  }
  printf("\nflowgraph:\n");
  G_show(stdout, G_nodes(g), printInsNode);
  printf("end of flowgraph:\n");
  return g;
}
