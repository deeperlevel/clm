#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
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

/** Finds a minimum-cost path with A* over a caller-provided directed graph.
 * Costs and heuristic estimates must be finite and nonnegative. For optimal
 * results, the heuristic must be admissible and return zero at the goal.
 * On success, out_path is heap-allocated and must be freed by the caller; it
 * includes start and goal. A successful search with *out_found=false means
 * no route exists. Node IDs are indices in [0,node_count).
 */
static inline clm_math_status clm_math_pathfinding_a_star(
    size_t node_count, size_t start, size_t goal,
    const clm_math_pathfinding_graph *graph, size_t **out_path,
    size_t *out_path_length, bool *out_found);

#include <math.h>
#include <stdlib.h>

struct clm_private_math_pathfinding_entry {
  size_t node;
  double cost;
  double estimate;
};

static inline clm_math_status clm_private_math_pathfinding_heap_push(
    struct clm_private_math_pathfinding_entry **heap, size_t *count,
    size_t *capacity, struct clm_private_math_pathfinding_entry entry) {
  if (*count == *capacity) {
    size_t next_capacity = *capacity ? *capacity * 2 : 16;
    if (next_capacity < *capacity ||
      next_capacity > ((size_t)-1) / sizeof(**heap))
      return MATH_ERROR_ALLOC;
    struct clm_private_math_pathfinding_entry *resized =
      (struct clm_private_math_pathfinding_entry *)realloc(
        *heap, next_capacity * sizeof(**heap));
    if (!resized)
      return MATH_ERROR_ALLOC;
    *heap = resized;
    *capacity = next_capacity;
  }
  size_t index = (*count)++;
  while (index) {
    size_t parent = (index - 1) / 2;
    double parent_priority = (*heap)[parent].cost + (*heap)[parent].estimate;
    double priority = entry.cost + entry.estimate;
    if (parent_priority <= priority)
      break;
    (*heap)[index] = (*heap)[parent];
    index = parent;
  }
  (*heap)[index] = entry;
  return MATH_SUCCESS;
}

static inline struct clm_private_math_pathfinding_entry
clm_private_math_pathfinding_heap_pop(
    struct clm_private_math_pathfinding_entry *heap, size_t *count) {
  struct clm_private_math_pathfinding_entry result = heap[0];
  struct clm_private_math_pathfinding_entry last = heap[--(*count)];
  if (*count) {
    size_t index = 0;
    while (index < *count / 2) {
      size_t child = index * 2 + 1;
      if (child + 1 < *count &&
          heap[child + 1].cost + heap[child + 1].estimate <
              heap[child].cost + heap[child].estimate)
        ++child;
      if (last.cost + last.estimate <=
          heap[child].cost + heap[child].estimate)
        break;
      heap[index] = heap[child];
      index = child;
    }
    heap[index] = last;
  }
  return result;
}

static inline clm_math_status clm_math_pathfinding_a_star(
    size_t node_count, size_t start, size_t goal,
    const clm_math_pathfinding_graph *graph, size_t **out_path,
    size_t *out_path_length, bool *out_found) {
  if (!graph || !graph->neighbor_count || !graph->neighbor ||
      !graph->heuristic || !out_path || !out_path_length || !out_found)
    return MATH_ERROR_VALUE;
  *out_path = NULL;
  *out_path_length = 0;
  *out_found = false;
  if (!node_count || start >= node_count || goal >= node_count)
    return MATH_ERROR_DIMENSION;
    if (node_count > ((size_t)-1) / sizeof(double) ||
      node_count > ((size_t)-1) / sizeof(size_t))
    return MATH_ERROR_ALLOC;

  double *costs = (double *)malloc(node_count * sizeof(*costs));
  size_t *parents = (size_t *)malloc(node_count * sizeof(*parents));
  if (!costs || !parents) {
    free(costs);
    free(parents);
    return MATH_ERROR_ALLOC;
  }
  for (size_t node = 0; node < node_count; ++node) {
    costs[node] = INFINITY;
    parents[node] = (size_t)-1;
  }

  struct clm_private_math_pathfinding_entry *heap = NULL;
  size_t heap_count = 0, heap_capacity = 0;
  double start_estimate = 0;
  clm_math_status status = graph->heuristic(graph->user, start, goal,
                                             &start_estimate);
  if (status)
    goto cleanup;
  if (!isfinite(start_estimate) || start_estimate < 0) {
    status = MATH_ERROR_VALUE;
    goto cleanup;
  }
  costs[start] = 0;
    struct clm_private_math_pathfinding_entry first_entry = {
      start, 0, start_estimate};
    status = clm_private_math_pathfinding_heap_push(
      &heap, &heap_count, &heap_capacity, first_entry);
  if (status)
    goto cleanup;

  while (heap_count) {
    struct clm_private_math_pathfinding_entry current =
        clm_private_math_pathfinding_heap_pop(heap, &heap_count);
    if (current.cost != costs[current.node])
      continue;
    if (current.node == goal) {
      *out_found = true;
      break;
    }

    size_t edge_count = 0;
    status = graph->neighbor_count(graph->user, current.node, &edge_count);
    if (status)
      goto cleanup;
    for (size_t edge = 0; edge < edge_count; ++edge) {
      size_t neighbor = (size_t)-1;
      double edge_cost = 0;
      status = graph->neighbor(graph->user, current.node, edge, &neighbor,
                               &edge_cost);
      if (status)
        goto cleanup;
      if (neighbor >= node_count) {
        status = MATH_ERROR_DIMENSION;
        goto cleanup;
      }
      if (!isfinite(edge_cost) || edge_cost < 0) {
        status = MATH_ERROR_VALUE;
        goto cleanup;
      }
      double candidate_cost = current.cost + edge_cost;
      if (!isfinite(candidate_cost)) {
        status = MATH_ERROR_VALUE;
        goto cleanup;
      }
      if (candidate_cost >= costs[neighbor])
        continue;

      double estimate = 0;
      status = graph->heuristic(graph->user, neighbor, goal, &estimate);
      if (status)
        goto cleanup;
      if (!isfinite(estimate) || estimate < 0 ||
          !isfinite(candidate_cost + estimate)) {
        status = MATH_ERROR_VALUE;
        goto cleanup;
      }
      costs[neighbor] = candidate_cost;
      parents[neighbor] = current.node;
        struct clm_private_math_pathfinding_entry next_entry = {
          neighbor, candidate_cost, estimate};
        status = clm_private_math_pathfinding_heap_push(
          &heap, &heap_count, &heap_capacity, next_entry);
      if (status)
        goto cleanup;
    }
  }

  if (*out_found) {
    size_t *path = (size_t *)malloc(node_count * sizeof(*path));
    if (!path) {
      status = MATH_ERROR_ALLOC;
      goto cleanup;
    }
    size_t length = 0, node = goal;
    while (length < node_count) {
      path[length++] = node;
      if (node == start)
        break;
      node = parents[node];
      if (node == (size_t)-1)
        break;
    }
    if (path[length - 1] != start) {
      free(path);
      *out_found = false;
      status = MATH_ERROR_VALUE;
      goto cleanup;
    }
    for (size_t left = 0, right = length - 1; left < right;
         ++left, --right) {
      size_t swap = path[left];
      path[left] = path[right];
      path[right] = swap;
    }
    *out_path = path;
    *out_path_length = length;
  }

cleanup:
  free(heap);
  free(costs);
  free(parents);
  return status;
}
