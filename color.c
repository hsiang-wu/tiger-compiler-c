#include "color.h"
#include "table.h"

#include "flowgraph.h"
#include "move.h"

#include "string.h"
#include <stdint.h>

#define K 7 // since we add %ebp as a temp. it can be 6 + 1 =7.

// descibed in P251
Temp_tempList simplify_worklist, freeze_worklist, spill_worklist;

static G_nodeList coalesced_nodes;
static G_nodeList select_stack;
static G_nodeList precolored_nodes;

MOV_table move_table; // alias of movelist in book code.

Live_moveList coalesced_moves, constrained_moves, frozen_moves, active_moves,
  worklist_moves;

static TAB_table degree_;
static G_graph ig_;

// convert a temp to a node.
static G_node t2n(Temp_temp temp);
static Temp_temp n2t(G_node node);

inline static Temp_temp
n2t(G_node node)
{
  Temp_temp t = G_nodeInfo(node);
  //  printf("n2t:[t%d:%p]\n", Temp_num(t), node);
  assert(t2n(t) == node);
  return t;
}

static void push_stack(G_nodeList* stack, G_node t);
static void decre_degree(G_node);
static int get_degree(G_node n);
static void init_usesdefs(AS_instrList il);
static bool is_moverelated(G_node n);
static bool is_adjacent(G_node, G_node);
static void assign_colors(Temp_map tmap, struct COL_result* ret);
static Live_moveList node_moves(G_node n);
static G_node get_alias(G_node n);

// In printf(...), "%%" for "%". In char * consts, "%" for "%".
static string colors[6] = { "%eax", "%ebx", "%ecx", "%edx", "%esi", "%edi" };

static G_nodeList adjacent(G_node n);

static void
addedge(G_node u, G_node v)
{
  if (!is_adjacent(u, v) && u != v) {
    // TBD
    // XXX: this changes graph.. is it OK?
    printf("Add edge t%d to t%d\n", Temp_num(n2t(u)), Temp_num(n2t(v)));
    G_addEdge(v, u);
    TAB_enter(degree_, u, (void*)(intptr_t)TAB_look(degree_, u) + 1);
    TAB_enter(degree_, v, (void*)(intptr_t)TAB_look(degree_, v) + 1);
  }
}

static void
freeze_moves(G_node u)
{
  Live_moveList ml = node_moves(u);
  G_node src, dst, v;
  for (; ml; ml = ml->tail) {
    src = ml->src;
    dst = ml->dst;
    if (get_alias(src) == get_alias(dst))
      v = get_alias(src);
    else
      v = get_alias(dst);
    MOV_delete(&active_moves, src, dst);
    MOV_add(&frozen_moves, src, dst);
    if (!node_moves(v) && get_degree(v) < K) {
      deletee(&freeze_worklist, n2t(v));
      add(&simplify_worklist, n2t(v));
    }
  }
}

static void
freeze()
{
  assert(freeze_worklist);
  Temp_temp u = freeze_worklist->head;
  deletee(&freeze_worklist, u);
  add(&spill_worklist, u);
  freeze_moves(t2n(u));
}

TAB_table alias_table;

inline static G_node
alias(G_node n)
{
  G_node r = TAB_look(alias_table, n);
  assert(r);
  return r;
}

static G_node
get_alias(G_node n)
{
  if (G_inlist(coalesced_nodes, n))
    return get_alias(alias(n));
  else
    return n;
}

static bool
is_precolored(G_node n)
{
  return G_inlist(precolored_nodes, n);
}

static bool
is_adjacent(G_node n1, G_node n2)
{
  // TBD: add bit matrix
  return G_inlist(G_adj(n1), n2);
}

// forall t in adjacent(v), OK(t,u)
static bool
is_all_adjs_ok(G_node u, G_node v)
{
  G_nodeList adjl = adjacent(u);
  G_node t;
  for (; adjl; adjl = adjl->tail) {
    t = adjl->head;
    // OK
    if (get_degree(t) < K || is_precolored(t) || is_adjacent(t, v)) {
      continue;
    }
    else {
      return FALSE;
    }
  }
  return TRUE;
}

static bool
conservative(G_nodeList nl)
{
  int k = 0;
  for (; nl; nl = nl->tail) {
    k += (get_degree(nl->head) >= K);
  }
  return (k < K);
}

static void
add_worklist(G_node u)
{
  if (is_precolored(u) && !is_moverelated(u) && get_degree(u) < K) {
    deletee(&freeze_worklist, n2t(u));
    add(&simplify_worklist, n2t(u));
  }
}

static void
combine(G_node u, G_node v)
{
  printf("combine %d and %d\n", Temp_num(n2t(u)), Temp_num(n2t(v)));
  if (inList(freeze_worklist, n2t(v))) {
    deletee(&freeze_worklist, n2t(v));
  }
  else {
    deletee(&spill_worklist, n2t(v));
  }
  assert(!is_precolored(v));

  G_add(&coalesced_nodes, v);

  assert(!inList(simplify_worklist, n2t(v)));
  assert(!G_inlist(select_stack, v));

  TAB_enter(alias_table, v, u);

  // ambiguous in book.
  // node_moves = G_union(node_moves(u), node_moves(v));
  MOV_append(move_table, u, MOV_look(move_table, v));
  G_nodeList nl = adjacent(v);
  for (; nl; nl = nl->tail) {
    printf("combine t%d's adjacents: t%d, add it to t%d's\n", Temp_num(n2t(v)), Temp_num(n2t(nl->head)), Temp_num(n2t(u)));
    addedge(nl->head, u);
    decre_degree(nl->head);
  }
  if (get_degree(u) >= K && inList(freeze_worklist, n2t(u))) {
    deletee(&freeze_worklist, n2t(u));
    add(&spill_worklist, n2t(u));
  }
}

static void
coalesce()
{
  printf("doing coalesce\n");
  G_node x, y, u, v, src, dst;
  Live_moveList ml = worklist_moves;
  for (; ml; ml = ml->tail) {
    src = ml->src;
    dst = ml->dst;
    x = get_alias(src);
    y = get_alias(dst);

    if (is_precolored(y)) {
      u = y;
      v = x;
    }
    else {
      u = x;
      v = y;
    }

    printf("coalesce t%d and t%d\n", Temp_num(n2t(u)), Temp_num(n2t(v)));
    MOV_delete(&worklist_moves, src, dst); // remove m
    if (u == v) {
      MOV_add(&coalesced_moves, src, dst);
      add_worklist(u);
      printf("Same. add to coalesced_moves and worklist\n");
    }
    else if (is_precolored(v) || is_adjacent(u, v)) {
      MOV_add(&constrained_moves, src, dst);
      add_worklist(u);
      add_worklist(v);
      printf("Coalesced. add to coalesced_moves and worklist\n");
    }
    else if ((is_precolored(u) && is_all_adjs_ok(v, u)) ||
             (!is_precolored(u) &&
              conservative(G_union(adjacent(u), adjacent(v))))) {
      MOV_add(&coalesced_moves, src, dst);
      combine(u, v);
      add_worklist(u);
    }
    else {
      MOV_add(&active_moves, src, dst);
      printf("Add to active_moves\n");
    }
  }
  assert(!worklist_moves);
}

static G_nodeList
adjacent(G_node n)
{
  // adjlist[n] \ (selectstack U coalescenodes)
  return G_except(G_except(G_adj(n), select_stack), coalesced_nodes);
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

static TAB_table
build_degreetable(G_nodeList nl)
{
  degree_ = TAB_empty(); // because we can't rmEdge(don't know how to
                         // restore.. we build a virtual degree:
                         // node-int map

  for (; nl; nl = nl->tail) {
    //    assert(nl->head == t2n(n2t(nl->head)));
    TAB_enter(degree_, nl->head, (void*)(intptr_t)G_degree(
                                   nl->head)); // to appease compiler warnings..
    printf("node:[t%d:%p].degree:%d\n", Temp_num(n2t(nl->head)), nl->head,
           G_degree(nl->head));
  }
  return degree_;
}

static Live_moveList
node_moves(G_node n)
{
  return MOV_intersection(MOV_look(move_table, n),
                          MOV_union(active_moves, worklist_moves));
}

static bool
is_moverelated(G_node n)
{
  return (node_moves(n) != NULL);
}

static void
enable_moves(G_nodeList nl)
{
  printf("enable moves\n");
  for (; nl; nl = nl->tail) {
    Live_moveList movs = node_moves(nl->head);
    for (; movs; movs = movs->tail) {
      if (MOV_inlist(active_moves, movs->src, movs->dst)) {
        MOV_delete(&active_moves, movs->src, movs->dst);
        MOV_add(&worklist_moves, movs->src, movs->dst);
        printf("enable %d to %d\n", Temp_num(n2t(movs->src)),
               Temp_num(n2t(movs->dst)));
      }
    }
  }
}

static int
get_degree(G_node n)
{
  return (intptr_t)TAB_look(degree_, n);
}

static void
decre_degree(G_node n)
{
  intptr_t newd = (intptr_t)TAB_look(degree_, n) - 1;
  TAB_enter(degree_, n, (void*)newd);
  //  assert(newd >= 0); // XXX:this is an error..
  if (newd == K - 1) {
    deletee(&spill_worklist, n2t(n));

    G_nodeList ns = adjacent(n);
    G_add(&ns, n);
    enable_moves(ns);

    if (is_moverelated(n))
      add(&freeze_worklist, n2t(n));
    else
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
t2n(Temp_temp temp)
{
  assert(temp);
  G_node n = (G_node)TAB_look(tempMap, temp);
  assert(n);
  //  printf("t2n:[t%d:%p]\n", Temp_num(temp), n);
  return n;
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

static void
assign_colors(Temp_map initial, struct COL_result* ret)
{
  Temp_map combine = Temp_layerMap(Temp_empty(), initial);
  Temp_tempList spilled_nodes = NULL;
  while (select_stack) {
    G_node n = pop_stack(&select_stack);

    if (G_inlist(coalesced_nodes, n)) {
      printf("%d\n", Temp_num(n2t(n)));
      assert(0);
    }

    printf("look...");
    Temp_print(G_nodeInfo(n));
    // assign a color
    if (Temp_look(combine, G_nodeInfo(n))) {
      printf("pre-colored\n");
      continue; // already have a color.(i.e. %eax of idiv)
    }

    G_nodeList adj = G_adj(n);
    G_nodeList DEBUG_adj_head = adj;
    TAB_table adjcolors = TAB_empty();
    for (; adj; adj = adj->tail) {
      string color = Temp_look(combine, n2t(get_alias(adj->head)));
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
        Temp_enter(combine, G_nodeInfo(n), colors[i]);
        printf("i=%d choose %s.\n", i, colors[i]);
        break;
      }
    }

    if (i == 6) {
      add(&spilled_nodes, n2t(n));
    }
  }

  G_nodeList nl = coalesced_nodes;
  for (; nl; nl = nl->tail) {
    assert(!is_precolored(nl->head));
    printf("check unique[t%d:%p]:", Temp_num(n2t(nl->head)), nl->head);
    assert(!G_inlist(nl->tail, nl->head));
    printf("pass\n");

    string this_color = Temp_look(combine, n2t(nl->head));
    printf("%s:%d\n", this_color, Temp_num(n2t(nl->head)));
    assert(!this_color);
    string alias_color = Temp_look(combine, n2t(get_alias(nl->head)));
    assert(alias_color);
    Temp_enter(combine, n2t(nl->head), alias_color);
  }

  ret->spills = spilled_nodes;
  ret->coloring = combine;
}

static void
make_worklist(G_nodeList nl)
{
  simplify_worklist = freeze_worklist = spill_worklist = NULL;
  for (; nl; nl = nl->tail) {
    if (G_degree(nl->head) >= K) {
      add(&spill_worklist, n2t(nl->head));
      printf("add to spill_worklist\n");
    }
    else if (is_moverelated(nl->head)) {
      add(&freeze_worklist, n2t(nl->head));
      printf("add to freeze_worklist\n");
    }
    else {
      add(&simplify_worklist, n2t(nl->head));
      printf("add to simplify_worklist\n");
    }
  }
}


struct COL_result
COL_color(G_graph ig, Temp_map initial, Temp_tempList regs, Live_moveList moves)
{
  // your code here.
  struct COL_result ret;

  // start simplify
  printf("K=%d\n", K);

  G_nodeList nl = G_nodes(ig);

  G_nodeList D_nl = nl;
  printf("check n2t and t2n:\n");
  for (; D_nl; D_nl = D_nl->tail) {
    n2t(D_nl->head); // check
  }

  G_show(stdout, nl, Temp_print);

  TAB_table degree = build_degreetable(nl);

  // it's assumed not delete any node.
  // now nl doesn't have any pre-colored node.
  //
  // Precolored nodelist is created in this call.
  precolored_nodes = NULL;
  nl = except_precolor(nl, regs);

  // build table from moves.
  move_table = MOV_Table(moves);
  alias_table = TAB_empty();
  ig_ = ig;

  // clear all static variable
  worklist_moves = moves;
  coalesced_moves = constrained_moves = frozen_moves = active_moves = NULL;

  make_worklist(nl);

  select_stack = NULL;
  coalesced_nodes = NULL;

  precolored_nodes = NULL;
  for (; regs; regs = regs->tail) {
    G_add(&precolored_nodes, t2n(regs->head));
  }

  printf("coalesced_nodes:%p\n", coalesced_nodes);
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

  assign_colors(initial, &ret); // save spilling and color to ret.

  unionn(newly_created, ret.spills);

  printf("dump coloring map:\n");
  Temp_dumpMap(stdout, ret.coloring);
  return ret;
}
