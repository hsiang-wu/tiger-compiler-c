#include <stdio.h>
#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "tree.h"
#include "absyn.h"
#include "assem.h"
#include "frame.h"
#include "graph.h"
#include "flowgraph.h"
#include "liveness.h"
#include "table.h"

static Temp_tempList except(Temp_tempList t1, Temp_tempList t2);
static Temp_tempList unionn(Temp_tempList t1, Temp_tempList t2);
static bool inList(Temp_tempList tl, Temp_temp t);
static bool equals(Temp_tempList t1, Temp_tempList t2);

static bool inList(Temp_tempList tl, Temp_temp t) {
  for (; tl; tl = tl->tail) {
    if (tl->head == t) return TRUE;
  }
  return FALSE;
} 

// t1 == t2
static bool equals(Temp_tempList t1, Temp_tempList t2)
{
  Temp_tempList tmp = t1;
  for (; t2; t2 = t2->tail, t1 = t1->tail) {
    if (!inList(tmp, t2->head))
      return FALSE;
    if (!t1) return FALSE; // t1 too short
  }
  if (t1) return FALSE; // t1 too long

  return TRUE;
}

// t1 - t2
static Temp_tempList except(Temp_tempList t1, Temp_tempList t2)
{
  if (!t2) return t1;

  Temp_tempList r = NULL;
  for (; t1; t1 = t1->tail) {
    if (!inList(t2, t1->head))
      r = Temp_TempList(t1->head, r);
  }
  return r;
}

// t1 U t2
static Temp_tempList unionn(Temp_tempList t1, Temp_tempList t2)
{
  if (!t1) return t2;

  Temp_tempList r = Temp_copyList(t1);
  // assert(equals(r, t1));
  for (; t2; t2 = t2->tail) {
    if (!t1 || !inList(t1, t2->head))
      r = Temp_TempList(t2->head, r);
  }

  //printf("union result :\n");
  //Temp_printList(r);

  return r;
}

Live_moveList Live_MoveList(G_node src, G_node dst, Live_moveList tail) {
	Live_moveList lm = (Live_moveList) checked_malloc(sizeof(*lm));
	lm->src = src;
	lm->dst = dst;
	lm->tail = tail;
	return lm;
}


Temp_temp Live_gtemp(G_node n) {
	//your code here.
	return (Temp_temp)G_nodeInfo(n);
}


/*
 * caculate flow-graph liveness
 * expect in, out to be G_empty()ed.
 * no-return
 */
static void createInOutTable(G_graph flow, G_table in, G_table out)
{
  assert(flow);
  G_nodeList nl_const = G_nodes(flow), nl;

  // init.
  for (nl=nl_const; nl; nl = nl->tail) {
    G_enter(in, nl->head, NULL);
    G_enter(out, nl->head, NULL);
  }
  
  bool DONE=FALSE;
  while (!DONE) {
    nl = nl_const;
    DONE=TRUE;
    for (; nl; nl = nl->tail) {

      /* save in[n], out[n] */
      Temp_tempList intl =  (Temp_tempList)G_look(in, nl->head);
      Temp_tempList outtl =  (Temp_tempList)G_look(out, nl->head);
      Temp_tempList intl_d = Temp_copyList(intl);
      Temp_tempList outtl_d = Temp_copyList(outtl);
      assert(equals(intl, intl_d));
      assert(equals(outtl, outtl_d));

      /*
       * in[n] = use[n] U (out[n] - def[n]) 
       * equation 1
       */
      intl = unionn(FG_use(nl->head), except(outtl, FG_def(nl->head)));
      G_enter(in, nl->head, intl); // update in-table

      /*
       * out[n] = U in[s] {s, s->succ[n]} 
       * equation 2
       */
      G_nodeList succ = G_succ(nl->head);

      for (; succ; succ = succ->tail) {
        outtl = unionn(outtl, (Temp_tempList)G_look(in, succ->head));
      } 
      G_enter(out, nl->head, outtl); // update out-table

      printf("in:\n");
      Temp_printList(intl);
      printf("out:\n");
      Temp_printList(outtl);
      printf("end out\n");
      /*
       * repeat until in = in1, out = out1
       */
      if (!equals(intl_d, intl) || !equals(outtl, outtl_d)) {
        DONE=FALSE;
      }
    }
  }
}

static G_graph initItfGraph(G_nodeList nl, TAB_table tempToNode)
{
  G_graph g = G_Graph();

  Temp_tempList temps = NULL;
  for (; nl; nl = nl->tail) {
    temps = unionn(temps, unionn(FG_def(nl->head), FG_use(nl->head)));
  }

  for (; temps; temps = temps->tail) {
    G_node n = G_Node(g, temps->head);
    TAB_enter(tempToNode, temps->head, n);
  }
  return g;
}

static Live_moveList moves = NULL;

static G_graph inteferenceGraph(G_nodeList nl, G_table liveMap)
{
  /* init a graph node-info save temp type
   * with create a all-regs list
   * with ctrate a temp -> node table
   */
  TAB_table tempMap = TAB_empty(); // g only map node to temp. need a quick lookup for temp to node.
  G_graph g = initItfGraph(nl, tempMap);

  printf("inteferenceGraph:\n");
  G_show(stdout, G_nodes(g), Temp_print); // debug

  for (; nl; nl = nl->tail) {
    AS_instr i = (AS_instr)G_nodeInfo(nl->head);
    assert(i);
    Temp_tempList defs = FG_def(nl->head);
    Temp_tempList liveouts = G_look(liveMap, nl->head);

    if (i->kind == I_MOVE) {
      Temp_tempList srcs = FG_use(nl->head);
      assert(defs->tail == NULL); // our move instruction only have 1 def.
      assert(srcs->tail == NULL); // and from 1 reg.

      G_node dst = (G_node)TAB_look(tempMap, defs->head);
      G_node src = (G_node)TAB_look(tempMap, srcs->head);

      Live_MoveList(dst, src, moves); // add to movelist

      for (; liveouts; liveouts = liveouts->tail) { 
        // look which node by map temp -> node
        G_node t = (G_node)TAB_look(tempMap, liveouts->head);

        if (dst == t) continue;
        if (liveouts->head == srcs->head) continue; // don't have to add
        G_addEdge(dst, t);
      }
    } else {
      for (; defs; defs = defs->tail) {
        assert(i->kind == I_OPER || i->kind == I_LABEL);

        G_node a = (G_node)TAB_look(tempMap, defs->head);
        for (; liveouts; liveouts = liveouts->tail) { 
          G_node b = (G_node)TAB_look(tempMap, liveouts->head);

          if (a == b) continue;
          G_addEdge(a, b);
        }
      } 
    }
  }

  printf("inteferenceGraph:\n");
  G_show(stdout, G_nodes(g), Temp_print); // debug
  return g;
}


struct Live_graph Live_liveness(G_graph flow) {
	//your code here.
	struct Live_graph lg;

    printf("start. liveness\n");
    G_table in = G_empty();
    G_table out = G_empty();
    createInOutTable(flow, in, out); /* solution of data-flow */

    G_graph g = inteferenceGraph(G_nodes(flow), out); // from out 
    lg.graph = g;
    lg.moves = moves;
	return lg;
}

