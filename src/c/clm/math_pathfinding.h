#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

#include "math_core.h"

/* Nodes and directed weighted edges are supplied by the caller. A platformer
 * can model walking and jump arcs as different edges between regions. */
typedef clm_math_status (*clm_math_pathfinding_neighbor_count_fn)(
    void *user, size_t node, size_t *out_count);
typedef clm_math_status (*clm_math_pathfinding_neighbor_fn)(
    void *user, size_t node, size_t edge_index, size_t *out_neighbor,
    double *out_cost);
typedef clm_math_status (*clm_math_pathfinding_heuristic_fn)(
    void *user, size_t node, size_t goal, double *out_estimate);

typedef struct clm_math_pathfinding_graph {
  clm_math_pathfinding_neighbor_count_fn neighbor_count;
  clm_math_pathfinding_neighbor_fn neighbor;
  clm_math_pathfinding_heuristic_fn heuristic;
  void *user;
} clm_math_pathfinding_graph;

#include <math.h>
#include <stdlib.h>

struct clm_private_math_pathfinding_entry {
  size_t node;
  double cost;
  double estimate;
};

clm_math_status clm_private_math_pathfinding_heap_push(
    struct clm_private_math_pathfinding_entry **heap, size_t *count,
    size_t *capacity, struct clm_private_math_pathfinding_entry entry);

struct clm_private_math_pathfinding_entry
clm_private_math_pathfinding_heap_pop(
    struct clm_private_math_pathfinding_entry *heap, size_t *count);

/** Finds a minimum-cost path with A* over a caller-provided directed graph.
 * Costs and heuristic estimates must be finite and nonnegative. For optimal
 * results, the heuristic must be admissible and return zero at the goal.
 * On success, out_path is heap-allocated and must be freed by the caller; it
 * includes start and goal. A successful search with *out_found=false means
 * no route exists. Node IDs are indices in [0,node_count).
 * @param node_count Input parameter described above.
 * @param start Input parameter described above.
 * @param goal Input parameter described above.
 * @param graph Input data or buffer; not modified.
 * @param out_path Receives the path.
 * @param out_path_length Receives the path length.
 * @param out_found Receives the found.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_pathfinding_a_star(
    size_t node_count, size_t start, size_t goal,
    const clm_math_pathfinding_graph *graph, size_t **out_path,
    size_t *out_path_length, bool *out_found);
