#include "color.h"
#include "table.h"

#include "string.h"
#include <stdint.h>

static G_node temp2node(Temp_temp temp);
// Delete it from the graph.
// Need to know last nodelist(so can we know this, and
// append next to last. Also needs degree table to
// change the degree of the deleted's neighbors.
//
// @param
//  nl: in case it's the head. not used otherwise
// @return the deleted node.
static G_node
delete_node(G_nodeList* nl, G_nodeList last, TAB_table degree)
{
  G_node tmp;

  if (last) { // remove this node
    tmp = last->tail->head;
    last->tail = last->tail->tail;
  }
  else {
    tmp = (*nl)->head;
    *nl = (*nl)->tail;
  }

  G_nodeList adjs;
  for (adjs = G_adj(tmp); adjs; adjs = adjs->tail) {
    TAB_enter(degree, adjs->head, TAB_look(degree, adjs->head) - 1);
  }

  // if (*nl) G_show(stdout, *nl, Temp_print);
  return tmp;
}

// Always pop a node(temp) in the graph,
// even it's a spilling(optimistic spill).
// @return 1 on node spilling. 0 if the popped node
// doesn't spill.
static bool
popInsig(G_nodeList* nl, int K, TAB_table degree, G_node* pop)
{
  assert(nl && "nl is NULL");
  assert(*nl && "*nl is NULL");

  G_nodeList last = NULL, nll = *nl;
  for (nll = *nl; nll; nll = nll->tail) {
    int d = (int)TAB_look(degree, nll->head);
    if (d < K) {
      *pop = delete_node(nl, last, degree);
      return FALSE;
    }
    last = nll;
  }

  // So we encounter a (potential) spill.
  //
  // Pop the head and mark it as a spill.
  // last=NULL means we delete the first node of nl.
  *pop = delete_node(nl, NULL, degree);
  return TRUE;
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
  printf("Delete precolored node:");
  G_nodeList newnl = NULL, head;
  for (; nl; nl = nl->tail) {
    Temp_temp reg = G_nodeInfo(nl->head);
    if (!inList(precolored, reg)) {
      if (newnl) {
        newnl = newnl->tail = G_NodeList(nl->head, NULL);
      }
      else {
        head = newnl = G_NodeList(nl->head, NULL);
      }
    }
    else {
      printf("delete:");
      Temp_print(reg);
    }
  }
  return head;
  return nl;
}

extern TAB_table tempMap;
static G_node
temp2node(Temp_temp temp)
{
  return (G_node)TAB_look(tempMap, temp);
}

static double
heuristic(Temp_temp t, TAB_table degree)
{
  assert(t);
  G_node n = temp2node(t);
  // int uses_defs = temp_uses(t) + temp_defs(t); // uses and defs in flowgraph

  //// adding t->num into consideration is a hack so that
  //// newly created temp(in rewriting phase) will have
  //// higher heuristic and thus less likely to be chosen and
  //// rewrite again.
  double v = Temp_num(t) + 100.0 / (intptr_t)TAB_look(degree, n);
  printf("heuristic: %f |", v);
  Temp_print(t);
  return v;
}

static Temp_temp
select_spill(Temp_tempList tl, TAB_table degree)
{
  Temp_temp mintemp;
  TAB_table heur_table;
  double minval = -1, val;

  for (; tl; tl = tl->tail) {
    val = heuristic(tl->head, degree);
    if (minval < 0 || val < minval) {
      mintemp = tl->head;
      minval = val;
    }
  }
  printf("spill ");
  Temp_print(mintemp);
  printf("value:%f\n", minval);
  return mintemp;
}

TAB_table potential_spill;

// descibed in P251
G_nodeList simplify_worklist, worklist_moves, freeze_worklist, spill_worklist;

#define K 7 // since we add %ebp as a temp. it can be 6 + 1 =7.

void
make_worklist(G_graph g)
{
  simplify_worklist = worklist_moves = freeze_worklist = spill_worklist = NULL;
  G_nodeList nl = G_nodes(g);
  for (; nl; nl = nl->tail) {
    if (G_degree(nl->head) >= K) {
      simplify_worklist = G_NodeList(nl->head, simplify_worklist);
    }
    else {
      spill_worklist = G_NodeList(nl->head, spill_worklist);
    }
  }
}

struct COL_result
COL_color(G_graph ig, Temp_map initial, Temp_tempList regs)
{
  // your code here.
  struct COL_result ret;

  // start simplify
  // int K = 0;
  // for (; regs; regs = regs->tail) {
  //  K++;
  //}

  // K = 7;
  printf("K=%d\n", K);

  G_nodeList stack = NULL;      // low-degree nodes
  G_nodeList spillstack = NULL; // potential spill nodes
  ret.spills = NULL;

  G_nodeList nl = G_nodes(ig);
  nl = copy_nodes(nl); // so it won't affect the graph

  TAB_table degree = degreeTable(nl);
  potential_spill = TAB_empty();

  make_worklist(ig);

  // it's assumed not delete any node.
  // now nl doesn't have any pre-colored node.
  nl = except_precolor(nl, regs);
  while (nl) {
    G_node n;
    bool spill = popInsig(
      &nl, K, degree, &n); // pop a insignaficant(degree < K) node from graph

    if (spill) {
      TAB_enter(potential_spill, n, (void*)1);

      printf("====potential spilling====\n");
      Temp_tempList tl = nl2tl(nl);
      Temp_printList(tl);
      // assert(0); // debug. stop here

      Temp_temp t = select_spill(tl, degree);
      printf("select potential spill:\n");
      Temp_print(t);
      push_stack(&spillstack, temp2node(t));
      printf("=======\n");
    }
    // else {
    push_stack(&stack, n);
    //}
  }

  static string colors[6] = { "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi" };

  G_nodeList spilllist = NULL;
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
    G_nodeList DEBUG_adj_head = adj;
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
    if (i == 6) {
      assert(0 && "low degree nodes is supposed to be all colored!");
    }
  }

  while (spillstack) {
    G_node n = pop_stack(&spillstack);

    printf("look...");
    Temp_print(G_nodeInfo(n));
    // assign a color
    if (Temp_look(initial, G_nodeInfo(n))) {
      printf("already colored\n");
      continue; // already have a color.(i.e. %eax of idiv)
    }

    G_nodeList adj = G_adj(n);
    G_nodeList D_adj_head = adj;
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
    if (i == 6) {
      // assert(TAB_look(potential_spill, n));
      // can't color this node .spill
      // it'll never be a pre-colored node
      spilllist = G_NodeList(n, spilllist);
      printf("\nSpill node[");
      Temp_print(G_nodeInfo(n));
      printf("]\n Adjacent to:");
      G_show(stdout, D_adj_head, Temp_print);
      printf("\n\n");
    }
  }

  ret.spills = nl2tl(spilllist);
  ret.coloring = initial;

  printf("dump coloring map:\n");
  Temp_dumpMap(stdout, ret.coloring);
  return ret;
}
