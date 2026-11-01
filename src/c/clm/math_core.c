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

