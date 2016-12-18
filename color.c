#include "color.h"
#include "table.h"

#include "flowgraph.h"

#include "string.h"
#include <stdint.h>

#define K 7 // since we add %ebp as a temp. it can be 6 + 1 =7.

// descibed in P251
Temp_tempList simplify_worklist, worklist_moves, freeze_worklist,
  spill_worklist;
G_nodeList select_stack;

// convert a temp to a node.
static G_node t2n(Temp_temp temp);
#define n2t(node) G_nodeInfo(node)
static void push_stack(G_nodeList* stack, G_node t);
static void decre_degree(G_node);
static void init_usesdefs(AS_instrList il);
static Temp_tempList assign_colors(Temp_map tmap);

// In printf(...), "%%" for "%". In char * consts, "%" for "%".
static string colors[6] = { "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi" };

static void
freeze()
{
  assert(0 && "not implemented!");
}

static void
coalesce()
{
  assert(0 && "not implemented!");
}

static G_nodeList
adjacent(G_node n)
{
  return G_except(G_adj(n), select_stack);
}

static void
simplify()
{
  Temp_temp t = simplify_worklist->head;
  G_node n = t2n(t);
  deletee(&simplify_worklist, t);
  push_stack(&select_stack, n);
  G_nodeList adjs = adjacent(n);
  for (; adjs; adjs = adjs->tail) {
    decre_degree(adjs->head);
  }
  printf("simplify: delete temp:");
  Temp_print(t);
}

static TAB_table degree_;
static TAB_table
degreeTable(G_nodeList nl)
{
  degree_ = TAB_empty(); // because we can't rmEdge(don't know how to
                         // restore.. we build a virtual degree:
                         // node-int map

  for (; nl; nl = nl->tail) {
    //    assert(nl->head == t2n(n2t(nl->head)));
    TAB_enter(degree_, nl->head, (void*)(intptr_t)G_degree(
                                   nl->head)); // to appease compiler warnings..
    printf("node:%p.degree:%d\n", nl->head, G_degree(nl->head));
  }
  return degree_;
}

static void
decre_degree(G_node n)
{
  intptr_t newd = (intptr_t)TAB_look(degree_, n) - 1;
  TAB_enter(degree_, n, (void*)newd);
  //  assert(newd >= 0); // XXX:this is an error..
  if (newd == K - 1) {
    deletee(&spill_worklist, n2t(n));

    add(&simplify_worklist, n2t(n));
  }
}

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
  } else {
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
      } else {
        head = newnl = G_NodeList(nl->head, NULL);
      }
    } else {
      printf("delete:");
      Temp_print(reg);
    }
  }
  return head;
  return nl;
}

extern TAB_table tempMap;
static G_node
t2n(Temp_temp temp)
{
  return (G_node)TAB_look(tempMap, temp);
}

// Resulting from previously spilled registers.
// Should avoid choosing them
static Temp_tempList newly_created = NULL;
void
COL_add_newly_created(Temp_temp t)
{
  newly_created = Temp_TempList(t, newly_created);
}

static double
heuristic(Temp_temp t, TAB_table degree)
{
  assert(t);
  G_node n = t2n(t);
  int uses_defs = temp_uses(t) + temp_defs(t); // uses and defs in flowgraph

  //// adding t->num into consideration is a hack so that
  //// newly created temp(in rewriting phase) will have
  //// higher heuristic and thus less likely to be chosen and
  //// rewrite again.

  // double v = 0.2 * Temp_num(t) + 100.0 / (intptr_t)TAB_look(degree, n);
  double v = 1.0 * uses_defs / (intptr_t)TAB_look(degree, n);
  printf("heuristic: %f degree: %ld |", v, (intptr_t)TAB_look(degree, n));
  Temp_print(t);
  return v;
}

// select a potential spill node.
static Temp_temp
select_spill(TAB_table degree)
{
  Temp_tempList tl = spill_worklist;
  Temp_temp mintemp;
  double minval = -1, val;

  // avoid choosing from new temps
  // resulting from previously spilled
  // registers.
  tl = except(tl, newly_created);
  if (!tl) {
    // so we have to choose from them...
    tl = newly_created;
    newly_created = NULL;
    assert(0 && "I don't want to see that..");
  }

  for (; tl; tl = tl->tail) {
    val = heuristic(tl->head, degree);
    if (minval < 0 || val < minval) {
      mintemp = tl->head;
      minval = val;
    }
  }
  deletee(&spill_worklist, mintemp);
  add(&simplify_worklist, mintemp);

  printf("spill ");
  Temp_print(mintemp);
  printf("value:%f\n", minval);

  return mintemp;
}

static Temp_tempList
assign_colors(Temp_map tmap)
{
  Temp_tempList spilled_nodes = NULL;
  while (select_stack) {
    G_node n = pop_stack(&select_stack);

    printf("look...");
    Temp_print(G_nodeInfo(n));
    // assign a color
    if (Temp_look(tmap, G_nodeInfo(n))) {
      printf("already colored\n");
      continue; // already have a color.(i.e. %eax of idiv)
    }

    G_nodeList adj = G_adj(n);
    G_nodeList DEBUG_adj_head = adj;
    TAB_table adjcolors = TAB_empty();
    for (; adj; adj = adj->tail) {
      string color = Temp_look(tmap, G_nodeInfo(adj->head));
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
        Temp_enter(tmap, G_nodeInfo(n), colors[i]);
        printf("i=%d choose %s.\n", i, colors[i]);
        break;
      }
    }

    if (i == 6) {
      add(&spilled_nodes, n2t(n));
    }
  }
  return spilled_nodes;
}

static void
make_worklist(G_graph g)
{
  simplify_worklist = worklist_moves = freeze_worklist = spill_worklist = NULL;
  G_nodeList nl = G_nodes(g);
  for (; nl; nl = nl->tail) {
    if (G_degree(nl->head) < K) {
      add(&simplify_worklist, n2t(nl->head));
      printf("add to simplify_worklist\n");
    } else {
      add(&spill_worklist, n2t(nl->head));
      printf("add to spill_worklist\n");
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

  G_nodeList nl = G_nodes(ig);

  TAB_table degree = degreeTable(nl);

  // it's assumed not delete any node.
  // now nl doesn't have any pre-colored node.
  nl = except_precolor(nl, regs);

  make_worklist(ig);
  select_stack = NULL;
  while (simplify_worklist || worklist_moves || freeze_worklist ||
         spill_worklist) {
    if (simplify_worklist)
      simplify();
    else if (worklist_moves)
      coalesce();
    else if (freeze_worklist)
      freeze();
    else if (spill_worklist)
      select_spill(degree);
  }

  ret.spills = assign_colors(initial);

  ret.coloring = initial;
  unionn(newly_created, ret.spills);

  printf("dump coloring map:\n");
  Temp_dumpMap(stdout, ret.coloring);
  return ret;
}
