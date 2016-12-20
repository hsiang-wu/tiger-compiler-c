/*
 * color.h - Data structures and function prototypes for coloring algorithm
 *             to determine register allocation.
 */
#ifndef COLOR_H
#define COLOR_H

#include "graph.h"
#include "symbol.h"
#include "temp.h"

#include "liveness.h"

struct COL_result
{
  Temp_map coloring;
  Temp_tempList spills;
};
struct COL_result COL_color(G_graph ig, Temp_map initial, Temp_tempList regs,
                            Live_moveList movs);

// Provide a interface for regalloc.c to use.
// Help select spill avoid those
// resulting from previously spilled registers.
// Should avoid choosing them
void COL_add_newly_created(Temp_temp t);

#endif
