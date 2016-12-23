#include "color.h"

#include "flowgraph.h"
#include "move.h"
#include "table.h"
#include "util.h"

#include "string.h"
#include "worklist.h"
#include <stdint.h>

#define K 7 // since we add %ebp as a temp. it can be 6 + 1 =7.

// descibed in P251
// Temp_tempList simplify_worklist, freeze_worklist, spill_worklist;
worklist_t simplify_worklist, freeze_worklist, spill_worklist;

static G_nodeList coalesced_nodes;
static G_nodeList select_stack;
static G_nodeList precolored_nodes;

static G_bmat adjset;

MOV_table move_table; // alias of movelist in book code.

Live_moveList coalesced_moves, constrained_moves, frozen_moves, active_moves,
  worklist_moves;

static TAB_table degree_, heuristic_degree_;
static G_graph ig_;

// convert a temp to a node.
static Temp_temp n2t(G_node node);

inline static Temp_temp
n2t(G_node node)
{
  Temp_temp t = G_nodeInfo(node);
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

    G_bmenter(adjset, u, v, TRUE);
    G_bmenter(adjset, v, u, TRUE);

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
    if (!node_moves(v) && get_degree(v) < K && u != v) {
      WL_delete(&freeze_worklist, n2w(v));
      WL_add(&simplify_worklist, n2w(v));
    }
  }
}

static void
freeze()
{
  assert(freeze_worklist);
  worklist_t u = WL_pop(&freeze_worklist);
  WL_add(&simplify_worklist, u);
  freeze_moves(t2n(u->t));
}

TAB_table alias_table;

inline static G_node
alias(G_node n)
{
  G_node r = TAB_look(alias_table, n);
  assert(r);
  return r;
}

inline static G_node
get_alias(G_node n)
{
  if (G_inlist(coalesced_nodes, n))
    return get_alias(alias(n));
  else
    return n;
}

inline static bool
is_precolored(G_node n)
{
  return G_inlist(precolored_nodes, n);
}

// Use bit matrix for fast lookup.
inline static bool
is_adjacent(G_node n1, G_node n2)
{
  // bool r = G_inlist(G_adj(n1), n2); // old way
  // assert(r == G_bmlook(adjset, n1, n2));

  bool r = G_bmlook(adjset, n1, n2);
  return r;
}

// forall t in adjacent(v), OK(t,u)
inline static bool
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

inline static bool
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
  if (!is_precolored(u) && !is_moverelated(u) && get_degree(u) < K) {
    WL_delete(&freeze_worklist, n2w(u));
    WL_add(&simplify_worklist, n2w(u));
  }
}

static void
combine(G_node u, G_node v)
{
  printf("combine %d and %d\n", Temp_num(n2t(u)), Temp_num(n2t(v)));
  if (WL_in(freeze_worklist, n2w(v))) {
    WL_delete(&freeze_worklist, n2w(v));
  }
  else {
    WL_delete(&spill_worklist, n2w(v));
  }
  assert(!is_precolored(v));

  G_add(&coalesced_nodes, v);

  assert(!WL_in(simplify_worklist, n2w(v)));
  assert(!G_inlist(select_stack, v));

  TAB_enter(alias_table, v, u);

  // ambiguous in book.
  // node_moves = G_union(node_moves(u), node_moves(v));
  MOV_append(move_table, u, MOV_look(move_table, v));
  G_nodeList nl = adjacent(v);
  for (; nl; nl = nl->tail) {
    printf("combine t%d's adjacents: t%d, add it to t%d's\n", Temp_num(n2t(v)),
           Temp_num(n2t(nl->head)), Temp_num(n2t(u)));
    // XXX: Different from book. Switch the sentences.
    // Otherwise the "delete from spill worklist" in decre_degree()
    // might be triggerd twice.
    decre_degree(nl->head);
    addedge(nl->head, u);
  }
  if (get_degree(u) >= K && WL_in(freeze_worklist, n2w(u))) {
    WL_delete(&freeze_worklist, n2w(u));
    WL_add(&spill_worklist, n2w(u));
  }
}

static void
coalesce()
{
  printf("doing coalesce\n");
  G_node x, y, u, v, src, dst;
  Live_moveList ml = worklist_moves;
  //  for (; ml; ml = ml->tail) {
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
  // }
  // assert(!worklist_moves);
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
  worklist_t w = WL_pop(&simplify_worklist);
  assert(w);
  G_node n = t2n(w->t);
  push_stack(&select_stack, n);
  G_nodeList adjs = adjacent(n);
  for (; adjs; adjs = adjs->tail) {
    decre_degree(adjs->head);
  }
  printf("simplify: delete temp:");
  Temp_print(w->t);
}

static TAB_table
build_degreetable(G_nodeList nl)
{
  // because we can't rmEdge(don't know how to
  // restore.. we build a virtual degree:
  // node-int map
  degree_ = TAB_empty();

  // The reason we need two degree table is simple:
  // Our heuristic algorithm favors high-degree nodes.
  // But a high-degree node will be built through
  // coalescing, which is undesiring, for spill these
  // nodes will cause all nodes coalesced to it to spill,
  // which is costly. Or(if spill coalesced nodes is not
  // implemented) we'll suffer infinite spilling,
  // which is the bug I observed.
  heuristic_degree_ = TAB_empty();

  for (; nl; nl = nl->tail) {
    //    assert(nl->head == t2n(n2t(nl->head)));
    TAB_enter(degree_, nl->head, (void*)(intptr_t)G_degree(
                                   nl->head)); // to appease compiler warnings..
    TAB_enter(
      heuristic_degree_, nl->head,
      (void*)(intptr_t)G_degree(nl->head)); // to appease compiler warnings..

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
    if (is_precolored(n)) {
      printf("delete a precolored node...\n");
      return;
    }
    G_nodeList ns = adjacent(n);
    G_add(&ns, n);
    enable_moves(ns);

    //    if (WL_in(spill_worklist, n2w(n))) {
    WL_delete(&spill_worklist, n2w(n));

    if (is_moverelated(n))
      WL_add(&freeze_worklist, n2w(n));
    else
      WL_add(&simplify_worklist, n2w(n));
    //   }
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
  printf("Delete precolored node:\n");
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
  for (nl = head; nl; nl = nl->tail) {
    printf("t%d\n", Temp_num(n2t(nl->head)));
  }
  return head;
}

extern TAB_table tempMap;
inline G_node
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
Temp_tempList newly_created = NULL;
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
select_spill()
{
  worklist_t wl = spill_worklist;
  worklist_t minnode;
  double minval = -1, val;

  // avoid choosing from new temps
  // resulting from previously spilled
  // registers.
  wl = WL_excepttl(wl, newly_created);
  if (WL_empty(wl)) {
    // so we have to choose from them...
    WL_cattl(wl, newly_created);
    newly_created = NULL;
    // assert(0 && "I don't want to see that..");
  }

  for (; !WL_empty(wl); wl = wl->next) {
    val = heuristic(wl->t, heuristic_degree_);
    if (minval < 0 || val < minval) {
      minnode = wl;
      minval = val;
    }
  }
  WL_delete(&spill_worklist, minnode);
  WL_add(&simplify_worklist, minnode);
  freeze_moves(t2n(minnode->t));

  printf("spill ");
  Temp_print(minnode->t);
  printf("value:%f\n", minval);

  return minnode->t;
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
    printf("alias %s:%d\n", alias_color, Temp_num(n2t(get_alias(nl->head))));
    assert(alias_color || inList(spilled_nodes, n2t(get_alias(nl->head))));

    // My alias spills, so I spill too.
    if (!alias_color)
      add(&spilled_nodes, n2t(nl->head));
    else
      Temp_enter(combine, n2t(nl->head), alias_color);
  }

  ret->spills = spilled_nodes;
  ret->coloring = combine;
}

static void
make_worklist(G_nodeList nl)
{
  simplify_worklist = WL_newlist();
  freeze_worklist = WL_newlist();
  spill_worklist = WL_newlist();
  for (; nl; nl = nl->tail) {
    if (G_degree(nl->head) >= K) {
      WL_add(&spill_worklist, n2w(nl->head));
      printf("add to spill_worklist\n");
    }
    else if (is_moverelated(nl->head)) {
      WL_add(&freeze_worklist, n2w(nl->head));
      printf("add to freeze_worklist\n");
    }
    else {
      WL_add(&simplify_worklist, n2w(nl->head));
      printf("add %d to simplify_worklist\n", Temp_num(n2t(nl->head)));
    }
  }
}

static void
D_check()
{
  printf("[DEBUG] K=%d\n", K);

  G_nodeList nl = G_nodes(ig_), D_nl, nl1, nl2;
  D_nl = nl;
  printf("check n2t and t2n:\n");
  for (; D_nl; D_nl = D_nl->tail) {
    Temp_temp t = n2t(D_nl->head); // check
    //  printf("n2t:[t%d:%p]\n", Temp_num(t), node);
    assert(t2n(t) == D_nl->head && "n2t and t2n error.");
  }

  printf("[DEBUG] show graph:\n");
  G_show(stdout, nl, Temp_print);

  printf("[DEBUG] Check bitmatrix:\n");
  for (nl1 = nl; nl1; nl1 = nl1->tail) {
    for (nl2 = nl; nl2; nl2 = nl2->tail) {
      bool in = G_inlist(G_adj(nl2->head), nl1->head);
      bool bmin = G_bmlook(adjset, nl1->head, nl2->head);
      if (in != bmin) {
        printf("\n[DEBUG] Bad: bitmatrix error at t%d - t%d, expect %d\n",
               Temp_num(n2t(nl1->head)), Temp_num(n2t(nl2->head)), in);

        assert(0 && "Bit matrix check error");
      }
      printf("%s", in ? "." : "x");
    }
    printf("\n");
  }
  printf("[DEBUG] pass all check\n");
}

struct COL_result
COL_color(G_graph ig, Temp_map initial, Temp_tempList regs, Live_moveList moves)
{
  // your code here.
  struct COL_result ret;

  {
    // make it global
    ig_ = ig;
    adjset = G_Bitmatrix(ig); // construct bit matrix

    // Build table from moves.
    move_table = MOV_Table(moves);
    alias_table = TAB_empty();
    worklist_moves = moves;
    coalesced_moves = constrained_moves = frozen_moves = active_moves = NULL;

    // After build degree table. Make nl have no pre-colored node.
    G_nodeList nl = G_nodes(ig);
    WL_newn2w(nl); // create nodes

    // debug checks
    D_check();

    build_degreetable(nl);
    nl = except_precolor(nl, regs);
    make_worklist(nl);

    // Clear all static variable
    select_stack = coalesced_nodes = precolored_nodes = NULL;
    for (; regs; regs = regs->tail) {
      G_add(&precolored_nodes, t2n(regs->head));
    }
  }

  printf("coalesced_nodes:%p\n", coalesced_nodes);
  while (!WL_empty(simplify_worklist) || worklist_moves ||
         !WL_empty(freeze_worklist) || !WL_empty(spill_worklist)) {
    if (!WL_empty(simplify_worklist))
      simplify();
    else if (worklist_moves)
      coalesce();
    else if (!WL_empty(freeze_worklist))
      freeze();
    else if (!WL_empty(spill_worklist))
      select_spill();
  }

  assign_colors(initial, &ret); // save spilling and color to ret.

  // XXX:this is redundant?
  //  unionn(newly_created, ret.spills);

  printf("dump coloring map:\n");
  Temp_dumpMap(stdout, ret.coloring);
  return ret;
}
