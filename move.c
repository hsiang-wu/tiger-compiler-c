#include "move.h"

#include "temp.h"
static Live_moveList copylist(Live_moveList m);
static void MOV_enter(MOV_table mt, G_node n, Live_moveList ml);

bool
MOV_inlist(Live_moveList ml, G_node src, G_node dst)
{
  for (; ml; ml = ml->tail) {
    if (ml->src == src && ml->dst == dst) {
      return TRUE;
    }
  }
  return FALSE;
}

void
MOV_delete(Live_moveList* pm, G_node src, G_node dst)
{
  assert(MOV_inlist(*pm, src, dst));
  Live_moveList ml, last;
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
MOV_add(Live_moveList* pm, G_node src, G_node dst)
{
  if (!MOV_inlist(*pm, src, dst)) *pm = Live_MoveList(src, dst, *pm);
}

Live_moveList
MOV_intersection(Live_moveList m1, Live_moveList m2)
{
  printf("move intersection\n");
  if (!m1) return NULL;

  Live_moveList r = NULL;
  for (; m2; m2 = m2->tail) {
    if (MOV_inlist(m1, m2->src, m2->dst)) {
      r = Live_MoveList(m2->src, m2->dst, r);
      printf("add to r\n");
    }
  }
  return r;
}

static Live_moveList
copylist(Live_moveList m)
{
  Live_moveList tl = NULL, hd = NULL;
  for (; m; m = m->tail) {
    if (!tl) {
      hd = tl = Live_MoveList(m->src, m->dst, NULL);
    } else {
      tl = tl->tail = Live_MoveList(m->src, m->dst, NULL);
    }
  }
  return hd;
}

Live_moveList
MOV_union(Live_moveList m1, Live_moveList m2)
{
  if (!m1) return copylist(m2);

  Live_moveList r = copylist(m1);
  for (; m2; m2 = m2->tail) {
    if (!m1 || !MOV_inlist(m1, m2->src, m2->dst))
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
MOV_table MOV_Table(Live_moveList ml) // traverse movelist and build table.
{
  G_node src, dst;
  MOV_table mt = TAB_empty();

  for (; ml; ml = ml->tail) {
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
Live_moveList
MOV_look(MOV_table mt, G_node n)
{
  return TAB_look(mt, n);
}

static void
mt_enter(MOV_table mt, G_node n, Live_moveList ml)
{
  TAB_enter(mt, n, ml);
}

static void
mt_add(MOV_table mt, G_node n, G_node src, G_node dst)
{
  Live_moveList newml = Live_MoveList(src, dst, MOV_look(mt, n));
  mt_enter(mt, n, newml);
}

void
MOV_append(MOV_table mt, G_node n, Live_moveList m2)
{
  Live_moveList m1 = MOV_look(mt, n);
  Live_moveList hd = m1;
  if (!m1) {
    mt_enter(mt, n, m2);
    return;
  }
  for (; m1->tail; m1 = m1->tail) {
  }
  m1->tail = m2;
  mt_enter(mt, n, m1);
}
