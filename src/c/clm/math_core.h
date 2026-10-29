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

#include "base.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Status returned by CLM math operations. */
typedef enum clm_math_status {
  MATH_SUCCESS = 0,
  MATH_ERROR_STORAGE = -1,
  MATH_ERROR_DIMENSION = -2,
  MATH_ERROR_VALUE = -3,
  MATH_ERROR_DOMAIN = -4,
  MATH_ERROR_SINGULAR = -5,
  MATH_ERROR_ALLOC = -6
} clm_math_status;

/** Backend adapter for math operations over existing numeric storage IDs.
 * Read callbacks copy exactly count values into caller buffers. Write callbacks
 * replace or resize the entire destination and must support aliased input IDs
 * because math operations may read their inputs before writing the result.
 * The adapter and its user context must outlive all math operations using it.
 */
typedef struct clm_math_storage_adapter {
  clm_math_status (*double_count)(void *user, clm_id id, size_t *count);
  clm_math_status (*double_read)(void *user, clm_id id, double *values,
                                 size_t count);
  clm_math_status (*double_write)(void *user, clm_id id, const double *values,
                                  size_t count);
  clm_math_status (*int64_count)(void *user, clm_id id, size_t *count);
  clm_math_status (*int64_read)(void *user, clm_id id, int64_t *values,
                                size_t count);
  clm_math_status (*int64_write)(void *user, clm_id id, const int64_t *values,
                                 size_t count);
  void *user;
} clm_math_storage_adapter;

/** Installs a storage backend, or clears it when adapter is NULL. */
void clm_math_set_storage_adapter(const clm_math_storage_adapter *adapter);

/** Reads a double storage ID into a newly allocated array.
 * expected==0 accepts any nonempty element count; otherwise the count must
 * match expected. On success, the caller owns *out and must free it.
 */
clm_math_status clm_math_read_double(clm_id id, size_t expected, double **out,
                                     size_t *count);

/** Replaces a double storage ID with count values from values. */
clm_math_status clm_math_write_double(clm_id id, const double *values,
                                      size_t count);

/** Reads an int64 storage ID into a newly allocated array.
 * expected==0 accepts any element count, including zero. On success, the
 * caller owns *out and must free it.
 */
clm_math_status clm_math_read_int64(clm_id id, size_t expected, int64_t **out,
                                    size_t *count);

/** Replaces an int64 storage ID with count values from values. */
clm_math_status clm_math_write_int64(clm_id id, const int64_t *values,
                                     size_t count);

/** Reads a 2D, 3D, or 4D vector from a double storage ID.
 * On success, the caller owns *values and must free it.
 */
clm_math_status clm_math_read_vector(clm_id id, size_t dimension,
                                     double **values, size_t *count);

/* Single-header implementation. Define CLM_MATH_CORE_IMPLEMENTATION in exactly
 * one translation unit, or define CLM_IMPLEMENTATION once to build all modules.
 * Do NOT define it in multiple translation units.
 */
#if defined(CLM_MATH_CORE_IMPLEMENTATION) || defined(CLM_IMPLEMENTATION)
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static clm_math_storage_adapter clm_private_math_core_backend;
void clm_math_set_storage_adapter(const clm_math_storage_adapter *a) {
  if (a)
    clm_private_math_core_backend = *a;
  else
    memset(&clm_private_math_core_backend, 0,
           sizeof clm_private_math_core_backend);
}
clm_math_status clm_math_read_double(clm_id id, size_t expected, double **out,
                                     size_t *count) {
  if (!out || !clm_private_math_core_backend.double_count ||
      !clm_private_math_core_backend.double_read)
    return MATH_ERROR_STORAGE;
  size_t n = 0;
  clm_math_status s = clm_private_math_core_backend.double_count(
      clm_private_math_core_backend.user, id, &n);
  if (s)
    return s;
  if (expected && n != expected)
    return MATH_ERROR_DIMENSION;
  if (!n || n > SIZE_MAX / sizeof(double))
    return MATH_ERROR_DIMENSION;
  double *p = malloc(n * sizeof(double));
  if (!p)
    return MATH_ERROR_ALLOC;
  s = clm_private_math_core_backend.double_read(
      clm_private_math_core_backend.user, id, p, n);
  if (!s)
    for (size_t i = 0; i < n; i++)
      if (!isfinite(p[i])) {
        s = MATH_ERROR_VALUE;
        break;
      }
  if (s) {
    free(p);
    return s;
  }
  *out = p;
  if (count)
    *count = n;
  return MATH_SUCCESS;
}
clm_math_status clm_math_write_double(clm_id id, const double *a, size_t n) {
  if (!clm_private_math_core_backend.double_write || (!a && n))
    return MATH_ERROR_STORAGE;
  for (size_t i = 0; i < n; i++)
    if (!isfinite(a[i]))
      return MATH_ERROR_VALUE;
  return clm_private_math_core_backend.double_write(
      clm_private_math_core_backend.user, id, a, n);
}
clm_math_status clm_math_read_int64(clm_id id, size_t expected, int64_t **out,
                                    size_t *count) {
  if (!out || !clm_private_math_core_backend.int64_count ||
      !clm_private_math_core_backend.int64_read)
    return MATH_ERROR_STORAGE;
  size_t n = 0;
  clm_math_status s = clm_private_math_core_backend.int64_count(
      clm_private_math_core_backend.user, id, &n);
  if (s)
    return s;
  if (expected && n != expected)
    return MATH_ERROR_DIMENSION;
  if (n > SIZE_MAX / sizeof(int64_t))
    return MATH_ERROR_DIMENSION;
  int64_t *p = malloc((n ? n : 1) * sizeof(int64_t));
  if (!p)
    return MATH_ERROR_ALLOC;
  s = clm_private_math_core_backend.int64_read(
      clm_private_math_core_backend.user, id, p, n);
  if (s) {
    free(p);
    return s;
  }
  *out = p;
  if (count)
    *count = n;
  return MATH_SUCCESS;
}
clm_math_status clm_math_write_int64(clm_id id, const int64_t *a, size_t n) {
  if (!clm_private_math_core_backend.int64_write || (!a && n))
    return MATH_ERROR_STORAGE;
  return clm_private_math_core_backend.int64_write(
      clm_private_math_core_backend.user, id, a, n);
}
clm_math_status clm_math_read_vector(clm_id id, size_t dim, double **a,
                                     size_t *count) {
  clm_math_status s = clm_math_read_double(id, dim, a, count);
  if (s)
    return s;
  size_t n = count ? *count : dim;
  if (n < 2 || n > 4) {
    free(*a);
    *a = NULL;
    return MATH_ERROR_DIMENSION;
  }
  return MATH_SUCCESS;
}
#endif
