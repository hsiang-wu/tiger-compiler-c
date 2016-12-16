#include "color.h"
#include "table.h"

#include "string.h"

static G_node
popInsig(G_nodeList* nl, int K, TAB_table degree)
{
  assert(nl && "nl is NULL");
  assert(*nl && "*nl is NULL");

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
  // assert(0 && "NEED TO IMPLEMENT SPILL");
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
    // modify nodes in this list also
    // changes the graph. But read, delete&add their references won't.
    nldup = G_NodeList(nl->head, nldup);
  }
  return nldup;
}

static TAB_table
degreeTable(G_nodeList nl)
{
  TAB_table degree = TAB_empty(); // because we can't rmEdge(don't know how to
                                  // restore.. we build a virtual degree:
                                  // node-int map

  for (; nl; nl = nl->tail) {
    TAB_enter(degree, nl->head, (void*)G_degree(nl->head));
    // printf("node:%p.degree:%d\n", nl->head, G_degree(nl->head));
  }
  return degree;
}

static Temp_tempList
nl2tl(G_nodeList nl)
{
  Temp_tempList tl = NULL, hd = NULL;
  for (; nl; nl = nl->tail) {
    Temp_temp reg = G_nodeInfo(nl->head);
    if (!tl)
      hd = tl = Temp_TempList(reg, NULL);
    else
      tl = tl->tail = Temp_TempList(reg, NULL);
  }
  return hd;
}

static G_nodeList
except_precolor(G_nodeList nl, Temp_tempList precolored)
{
  printf("before delete:");
  G_show(stdout, nl, Temp_print);
  G_nodeList newnl = NULL;
  for (; nl; nl = nl->tail) {
    Temp_temp reg = G_nodeInfo(nl->head);
    if (!inList(precolored, reg)) {
      if (!newnl) {
        newnl = G_NodeList(nl->head, NULL);
      } else {
        newnl = newnl->tail = G_NodeList(nl->head, NULL);
      }
    }
  }
  printf("after delete:");
  G_show(stdout, newnl, Temp_print);
  return newnl;
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

  K = 7; // since we add %ebp as a temp. it can be 6 + 1 =7.

  G_nodeList stack = NULL;
  ret.spills = NULL;

  G_nodeList nl = G_nodes(ig);
  nl = copy_nodes(nl); // so it won't affect the graph

  TAB_table degree = degreeTable(nl);
  // After constructing a degree table, we can delete
  // initial regs from nodelists: they can't be spilled
  // and doesn't affect our interfering. (refer to book)
  //  nl = except_precolor(nl, regs);
  while (nl) {
    G_node n = popInsig(
      &nl, K, degree); // pop a insignaficant(degree < K) node from graph

    if (!n) {
      ret.spills = nl2tl(nl);
      return ret;
      // create spill list
    }

    push_stack(&stack, n);
  }

  static string colors[6] = { "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi" };

  while (stack) {
    G_node n = pop_stack(&stack);

    printf("look...");
    Temp_print(G_nodeInfo(n));
    // assign a color
    if (Temp_look(initial, G_nodeInfo(n))) {
      printf("already colored\n");
      continue; // already have a color.(i.e. %eax of idiv)
    }

    G_nodeList adj = G_adj(n);
    TAB_table adjcolors = TAB_empty();
    for (; adj; adj = adj->tail) {
      string color = Temp_look(initial, G_nodeInfo(adj->head));
      printf("%s", color);
      Temp_print(G_nodeInfo(adj->head));
      if (color) {
        TAB_enter(adjcolors, S_Symbol(color), (void*)1);
        printf("enter\n");
      }
    }

    int i = 0;
    for (; i < 6; i++) {
      if (!TAB_look(adjcolors, S_Symbol(colors[i]))) {
        Temp_enter(initial, G_nodeInfo(n), colors[i]);
        printf("i=%d choose %s.\n", i, colors[i]);
        break;
      }
    }
  }

  ret.coloring = initial;

  printf("dump coloring map:\n");
  Temp_dumpMap(stdout, ret.coloring);
  return ret;
}
