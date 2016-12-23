#include "worklist.h"

worklist_t
WL_newlist()
{
  // always have empty node(t = NULL)
  // you'll never delete this node
  // so a list can't be NULL
  worklist_t wl = checked_malloc(sizeof(*wl));
  wl->lid = listcnt++;
  wl->prev = wl->next = NULL;
  wl->t = NULL;
}

worklist_t
WL_newnode(Temp_temp temp)
{
  worklist_t wl = checked_malloc(sizeof(*wl));
  wl->lid = -1; // when add, it'll be set
  wl->prev = wl->next = NULL;
  wl->t = temp;
}

void
WL_delete(worklist_t* head, worklist_t node)
{
  assert(*head);
   assert( (*head)->t);
  assert(!(*head)->prev); // is it really head?
  if ((*head)->lid != node->lid) {
    //    assert(0);
    return;
  }

  node->lid = -1;
  if (node == *head) {
    // new head
    *head = (*head)->next;
    (*head)->prev = NULL;
  }
  else {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }
}

// void
// WL_add(worklist_t* head, Temp_temp t)
//{
//  WL_addn(head, WL_newnode(t));
//}

void
WL_add(worklist_t* head, worklist_t node)
{
  // add to new head.
  if ((*head)->lid == node->lid) {
    //    assert(0);
    return;
  }

  (*head)->prev = node;
  node->next = *head;
  node->prev = NULL;
  node->lid = (*head)->lid;
  *head = node;
}

inline bool
WL_empty(worklist_t head)
{
  // always have empty node(see WL_newlist)
  return !head->t;
}

static TAB_table n2wmap;
void
WL_newn2w(G_nodeList nl)
{
  n2wmap = TAB_empty();
  for (; nl; nl = nl->tail) {
    if (nl) {
      TAB_enter(n2wmap, nl->head, WL_newnode(G_nodeInfo(nl->head)));
    }
  }
}

worklist_t
n2w(G_node n)
{
  return TAB_look(n2wmap, n);
}

worklist_t
WL_pop(worklist_t* head)
{
  if ((*head)->t) {
    worklist_t w = *head;
    *head = w->next;
    (*head)->prev = NULL;
    w->next = NULL;
    w->lid = -1;
    return w;
  }
  return NULL;
}

bool
WL_in(worklist_t head, worklist_t node)
{
  return head->lid == node->lid;
}

inline static worklist_t
t2w(Temp_temp t)
{
  return n2w(t2n(t));
}

worklist_t
WL_excepttl(worklist_t w1, Temp_tempList tl)
{
  for (; tl; tl = tl->tail) {
    if (WL_in(w1, t2w(tl->head))) WL_delete(&w1, t2w(tl->head));
  }
  return w1;
}

worklist_t
WL_cattl(worklist_t w1, Temp_tempList tl)
{
  for (; tl; tl = tl->tail) {
    if (!WL_in(w1, t2w(tl->head))) WL_add(&w1, t2w(tl->head));
  }
  return w1;
}
