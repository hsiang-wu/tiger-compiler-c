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
#include "table.h"

#include "string.h"

static G_node popInsig(G_nodeList *nl, int K)
{
  if (!nl) return NULL;

  G_nodeList last = NULL, nll=*nl;
  for (; nll; nll=nll->tail) {
    if (G_degree(nll->head) < K) {
      G_node tmp = nll->head;
      if (last) { // remove this node
        last->tail = nll->tail;
      } else {
        *nl = (*nl)->tail;
      }
      printf("after pop node: %s\n", (*nl)?"":"empty");
      if (*nl) G_show(stdout, *nl, Temp_print);
      printf("%s\n", tmp?"":"empty tmp"); Temp_print(tmp);
      return tmp;
    }
    last = nll;
  }
  printf("remain node:\n");
  G_show(stdout, *nl, Temp_print);
  assert(0 && "NEED TO IMPLEMENT SPILL");
}

static void push_stack(G_nodeList *stack, G_node t)
{
  *stack = G_NodeList(t, *stack);
}

static G_node pop_stack(G_nodeList *stack)
{
  G_node t = (*stack)->head;
  *stack = (*stack)->tail;
  return t;
}

// reversed
static G_nodeList copy_nodes(G_nodeList nl)
{
  G_nodeList nldup = NULL;
  for (; nl; nl=nl->tail) {
    nldup = G_NodeList(nl->head, nldup);
  }
  return nldup;
}

struct COL_result COL_color(G_graph ig, Temp_map initial, Temp_tempList regs) {
	//your code here.
	struct COL_result ret;
    
    // start simplify
    int K = 0;
    for (;regs;regs=regs->tail) K++;

    K=6;
    printf("K=%d\n", K);

    G_nodeList stack = NULL;

    G_nodeList nl = G_nodes(ig);
    nl = copy_nodes(nl); // so it won't affect the graph
    
    while (nl) {
      G_node n = popInsig(&nl, K); // pop a insignaficant(degree < K) node from graph
      
      push_stack(&stack, n);
    }


    static string colors[6] = {"%eax", "%ebx", "%ecx", "%edx", "%edi", "%ebi"};

    while (stack) {
      G_node n = pop_stack(&stack);

      G_nodeList adj = G_adj(n);
      TAB_table adjcolors = TAB_empty();
      for (; adj; adj=adj->tail) {
        string color = Temp_look(initial, G_nodeInfo(adj->head));
        if ( color ) {
          TAB_enter(adjcolors, color, (void*)1);
        }
      }

      // assign a color
      int i = 0;
      for (; i < 6; i++) {
        if (!TAB_look(adjcolors, colors[i])) {
          Temp_enter(initial, G_nodeInfo(n), colors[i]);
        }
      }
    }


    ret.coloring = initial;
    ret.spills = NULL;
    
    printf("dump coloring map:\n");
    Temp_dumpMap(stdout, ret.coloring);
	return ret;
}
