#ifndef MOVE_H
#define MOVE_H

#include "table.h"
#include "util.h"

#include "graph.h"
#include "liveness.h"

typedef struct move_* move;
struct move_ {
    G_node src;
    G_node dst;
    int sid; // set id
};

typedef struct moveList_* moveList;
struct moveList_ {
    move head;
    moveList tail;
    moveList prev;
    int sid; // set id
};

move MOV_Move(G_node, G_node, int);
moveList MOV_MoveList(move, moveList, int);

// bool MOV_inlist(moveList, G_node src, G_node dst);
// void MOV_delete(moveList*, G_node, G_node);
void MOV_addlist(moveList*, G_node, G_node);
bool MOV_inlist(moveList, move);
void MOV_delete(moveList*, move);
void MOV_add(moveList*, move);
bool MOV_empty(moveList);
// bool MOV_inlist(moveList, move);
// void MOV_delete(moveList*, move);
// void MOV_add(moveList*, move);

moveList MOV_intersection(moveList, moveList);
moveList MOV_union(moveList, moveList);

// name for moveList in book.
// A mapping from a node to the list of moves it is associated with.
typedef TAB_table MOV_table;
MOV_table MOV_Table(moveList); // traverse movelist and build table.

// caller must call init_map() first
moveList MOV_look(MOV_table, G_node);
void MOV_append(MOV_table, G_node, moveList);

moveList MOV_set();
#endif
