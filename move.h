#ifndef MOVE_H
#define MOVE_H

#include "table.h"
#include "util.h"

#include "graph.h"
#include "liveness.h"

// typedef struct move_* move;
// struct move_
//{
//  G_node src;
//  G_node dst;
//};
//
// typedef struct moveList_* moveList;
// struct moveList_
//{
//  move head;
//  moveList tail;
//};

bool MOV_inlist(Live_moveList, G_node src, G_node dst);
void MOV_delete(Live_moveList*, G_node, G_node);
void MOV_add(Live_moveList*, G_node, G_node);
// bool MOV_inlist(Live_moveList, move);
// void MOV_delete(Live_moveList*, move);
// void MOV_add(Live_moveList*, move);

Live_moveList MOV_intersection(Live_moveList, Live_moveList);
Live_moveList MOV_union(Live_moveList, Live_moveList);

// name for moveList in book.
// A mapping from a node to the list of moves it is associated with.
typedef TAB_table MOV_table;
MOV_table MOV_Table(Live_moveList); // traverse movelist and build table.

// caller must call init_map() first
Live_moveList MOV_look(MOV_table, G_node);
void MOV_append(MOV_table, G_node, Live_moveList);

#endif
