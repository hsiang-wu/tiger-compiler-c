#include "color.h"
#include "table.h"

#include "string.h"

static G_node
popInsig(G_nodeList* nl, int K, TAB_table degree)
{
  assert(nl);

  G_nodeList last = NULL, nll = *nl;
  for (nll = *nl; nll; nll = nll->tail) {
    int d;
    d = (int)TAB_look(degree, nll->head);
    if (d < K) {
      G_node tmp = nll->head;
      G_nodeList adjs;
      for (adjs = G_adj(tmp); adjs; adjs = adjs->tail) {
        TAB_enter(degree, adjs->head, TAB_look(degree, adjs->head) - 1);
      }

      if (last) { // remove this node
        last->tail = nll->tail;
      } else {
        *nl = (*nl)->tail;
      }
      // if (*nl) G_show(stdout, *nl, Temp_print);
      return tmp;
    } else {
      // printf("degree %d,", d);
      // Temp_print(G_nodeInfo(nll->head));
    }
    last = nll;
  }

  printf("NEED TO IMPLEMENT SPILL");
  return NULL;
  assert(0 && "NEED TO IMPLEMENT SPILL");
}

static void
push_stack(G_nodeList* stack, G_node t)
{
  *stack = G_NodeList(t, *stack);
}

static G_node
pop_stack(G_nodeList* stack)
{
  G_node t = (*stack)->head;
  *stack = (*stack)->tail;
  return t;
}

// reversed
static G_nodeList
copy_nodes(G_nodeList nl)
{
  G_nodeList nldup = NULL;
  for (; nl; nl = nl->tail) {
    nldup = G_NodeList(nl->head, nldup);
  }
  return nldup;
}

static TAB_table
degreeTable(G_nodeList nl)
{
  TAB_table degree = TAB_empty(); // because we can't rmEdge(don't know how to
                                  // store.. we build a virtual degree:
                                  // node-int map

  for (; nl; nl = nl->tail) {
    TAB_enter(degree, nl->head, (void*)G_degree(nl->head));
    // printf("node:%p.degree:%d\n", nl->head, G_degree(nl->head));
  }
  return degree;
}

struct COL_result
COL_color(G_graph ig, Temp_map initial, Temp_tempList regs)
{
  // your code here.
  struct COL_result ret;

  // start simplify
  int K = 0;
  for (; regs; regs = regs->tail) {
    K++;
  }

  K = 6;
  printf("K=%d\n", K);

  G_nodeList stack = NULL;

  G_nodeList nl = G_nodes(ig);
  nl = copy_nodes(nl); // so it won't affect the graph

  TAB_table degree = degreeTable(nl);
  while (nl) {
    G_node n = popInsig(
      &nl, K, degree); // pop a insignaficant(degree < K) node from graph

    if (!n) goto spill;

    push_stack(&stack, n);
  }

  static string colors[6] = { "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi" };

  while (stack) {
    G_node n = pop_stack(&stack);

    G_nodeList adj = G_adj(n);
    TAB_table adjcolors = TAB_empty();
    for (; adj; adj = adj->tail) {
      string color = Temp_look(initial, G_nodeInfo(adj->head));
      if (color) {
        TAB_enter(adjcolors, color, (void*)1);
      }
    }

    // assign a color
    if (Temp_look(initial, G_nodeInfo(n)))
      continue; // already have a color.(i.e. %eax of idiv)

    int i = 0;
    for (; i < 6; i++) {
      if (!TAB_look(adjcolors, colors[i])) {
        Temp_enter(initial, G_nodeInfo(n), colors[i]);
        break;
      }
    }
  }

  ret.coloring = initial;
  ret.spills = NULL;

  printf("dump coloring map:\n");
  Temp_dumpMap(stdout, ret.coloring);
  return ret;
}
