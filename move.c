#include "move.h"

#include "temp.h"
static moveList copylist(moveList m);
static void MOV_enter(MOV_table mt, G_node n, moveList ml);

bool
MOV_inlist_brute(moveList ml, G_node src, G_node dst)
{
  for (; ml; ml = ml->tail) {
    if (ml->src == src && ml->dst == dst) {
      return TRUE;
    }
  }
  return FALSE;
}

move
MOV_Move(G_node src, G_node dst, int i)
{
  move m = checked_malloc(sizeof(*m));
  m->src = src;
  m->dst = dst;
  m->sid = i;
  return m;
}

moveList
MOV_MoveList(move m, moveList tail, int i)
{
  moveList ml = checked_malloc(sizeof(*ml));
  ml->head = m;
  ml->tail = tail;
  ml->prev = NULL;
  if (tail) {
    tail->prev = ml;
  }
  ml->sid = i;
  return ml;
}

/*
void
MOV_delete(moveList* pm, G_node src, G_node dst)
{
  assert(MOV_inlist(*pm, src, dst));
  moveList ml, last;
  ml = *pm;
  last = NULL;
  for (; ml; ml = ml->tail) {
    if (ml->src == src && ml->dst == dst) {
      if (!last)
        *pm = (*pm)->tail;
      else
        last->tail = ml->tail;
    }
    last = ml;
  }
  assert(!MOV_inlist(*pm, src, dst));
}

void
MOV_add(moveList* pm, G_node src, G_node dst)
{
  if (!MOV_inlist(*pm, src, dst)) *pm = Live_MoveList(src, dst, *pm);
}
*/

moveList
MOV_intersection(moveList m1, moveList m2)
{
  printf("move intersection\n");
  if (!m1) return NULL;

  moveList r = NULL;
  for (; m2; m2 = m2->tail) {
    if (MOV_inlist_brute(m1, m2->src, m2->dst)) {
      r = Live_MoveList(m2->src, m2->dst, r);
      printf("add to r\n");
    }
  }
  return r;
}

static moveList
copylist(moveList m)
{
  moveList tl = NULL, hd = NULL;
  for (; m; m = m->tail) {
    if (!tl) {
      hd = tl = Live_MoveList(m->src, m->dst, NULL);
    }
    else {
      tl = tl->tail = Live_MoveList(m->src, m->dst, NULL);
    }
  }
  return hd;
}

moveList
MOV_union(moveList m1, moveList m2)
{
  if (!m1) return copylist(m2);

  moveList r = copylist(m1);
  for (; m2; m2 = m2->tail) {
    if (!m1 || !MOV_inlist_brute(m1, m2->src, m2->dst))
      r = Live_MoveList(m2->src, m2->dst, r);
  }
  return r;
}

void
show(void* key, void* value)
{
  printf("G_node:");
  G_node n = (G_node)key;
  printf("%d", Temp_num(G_nodeInfo(n)));
}

// name for moveList in book.
// A mapping from a node to the list of moves it is associated with.
MOV_table MOV_Table(moveList ml) // traverse movelist and build table.
{
  G_node src, dst;
  MOV_table mt = TAB_empty();

  for (; !MOV_empty(ml); ml = ml->tail) {
    src = ml->src;
    dst = ml->dst;
    MOV_append(mt, src, Live_MoveList(src, dst, NULL));
    if (src != dst) {
      MOV_append(mt, dst, Live_MoveList(src, dst, NULL));
    }
  }
  printf("move table:%p\n", mt);
  TAB_dump(mt, show);
  return mt;
}

// caller must call init_map() first
moveList
MOV_look(MOV_table mt, G_node n)
{
  return TAB_look(mt, n);
}

static void
mt_enter(MOV_table mt, G_node n, moveList ml)
{
  TAB_enter(mt, n, ml);
}

static void
mt_add(MOV_table mt, G_node n, G_node src, G_node dst)
{
  moveList newml = Live_MoveList(src, dst, MOV_look(mt, n));
  mt_enter(mt, n, newml);
}

void
MOV_append(MOV_table mt, G_node n, moveList m2)
{
  moveList m1 = MOV_look(mt, n);
  moveList hd = m1;
  if (!m1) {
    mt_enter(mt, n, m2);
    return;
  }
  for (; m1->tail; m1 = m1->tail) {
  }
  m1->tail = m2;
  mt_enter(mt, n, m1);
}

//
//
// Code for sets.
//
//
static int cnt = 0;
moveList
MOV_set()
{
  moveList empty = MOV_MoveList(NULL, NULL, cnt++);
  return empty;
}

bool
MOV_inlist(moveList m1, move m2)
{
  return m1->sid == m2->sid;
}

void
MOV_delete(moveList* l, move m)
{
  assert(MOV_inlist(*l, m));
  m->sid = -1;
  if ((*l)->head == m) {
    *l = (*l)->tail;
    (*l)->prev = NULL;
  }
  else {
    // l is head. if *l != m, m has prev.
    m->prev->tail = m->tail;
    m->tail->prev = m->prev;
  }
}

void
MOV_add(moveList* l, move m)
{
  if (!MOV_inlist(*l, m)) {
    m->id = (*l)->id;
    *l = Live_MoveList(m->src, m->dst, *l);
  }
}

void
MOV_addlist(moveList* l, G_node src, G_node dst)
{
  moveList newnode = Live_MoveList(src, dst, *l);
  newnode->id = (*l)->id;
  *l = newnode;
}

bool
MOV_empty(moveList m)
{
  assert(!m->src == !m->dst);
  return !m->src;
}
