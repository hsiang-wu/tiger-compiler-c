
#ifndef WORKLIST_H
#define WORKLIST_H

#include "color.h"
#include "graph.h"
#include "table.h"
#include "temp.h"
#include "util.h"

static int listcnt = 0;

typedef struct worklist_* worklist_t;
struct worklist_ {
    Temp_temp t;
    worklist_t prev; // if !prev: head
    worklist_t next; // if !next: tail
    int lid;
};

worklist_t WL_newlist();
worklist_t WL_newnode(Temp_temp temp);
bool WL_in(worklist_t head, worklist_t node);
void WL_delete(worklist_t* head, worklist_t node);
// void WL_add(worklist_t* head, Temp_temp t);
void WL_add(worklist_t* head, worklist_t node);
bool WL_empty(worklist_t head);

worklist_t WL_excepttl(worklist_t w1, Temp_tempList w2);
worklist_t WL_cattl(worklist_t w1, Temp_tempList w2);

worklist_t n2w(G_node);
worklist_t WL_pop(worklist_t*);

void WL_newn2w(G_nodeList nl);

#endif // WORKLIST_H
