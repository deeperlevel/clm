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

#include "math_vector.h"

clm_math_status clm_math_vector_set(clm_id id, const double *a,
                                                  size_t n) {
  if (n < 2 || n > 4)
    return MATH_ERROR_DIMENSION;
  return clm_math_write_double(id, a, n);
}

clm_math_status
clm_private_math_vector_two(clm_id first, clm_id second, double **a, double **b,
                            size_t *n) {
  clm_math_status s = clm_math_read_vector(first, 0, a, n);
  if (s)
    return s;
  s = clm_math_read_vector(second, *n, b, NULL);
  if (s) {
    free(*a);
    return s;
  }
  return MATH_SUCCESS;
}

clm_math_status clm_math_vector_add(clm_id x, clm_id y,
                                                  clm_id dst) {
  double *a, *b, out[4];
  size_t n;
  clm_math_status s = clm_private_math_vector_two(x, y, &a, &b, &n);
  if (s)
    return s;
  for (size_t i = 0; i < n; i++)
    out[i] = a[i] + b[i];
  free(a);
  free(b);
  return clm_math_write_double(dst, out, n);
}

clm_math_status clm_math_vector_subtract(clm_id x, clm_id y,
                                                       clm_id dst) {
  double *a, *b, out[4];
  size_t n;
  clm_math_status s = clm_private_math_vector_two(x, y, &a, &b, &n);
  if (s)
    return s;
  for (size_t i = 0; i < n; i++)
    out[i] = a[i] - b[i];
  free(a);
  free(b);
  return clm_math_write_double(dst, out, n);
}

clm_math_status clm_math_vector_scale(clm_id x, double f,
                                                    clm_id dst) {
  if (!isfinite(f))
    return MATH_ERROR_VALUE;
  double *a, out[4];
  size_t n;
  clm_math_status s = clm_math_read_vector(x, 0, &a, &n);
  if (s)
    return s;
  for (size_t i = 0; i < n; i++)
    out[i] = a[i] * f;
  free(a);
  return clm_math_write_double(dst, out, n);
}

clm_math_status clm_math_vector_dot(clm_id x, clm_id y,
                                                  double *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *a, *b, v = 0;
  size_t n;
  clm_math_status s = clm_private_math_vector_two(x, y, &a, &b, &n);
  if (s)
    return s;
  for (size_t i = 0; i < n; i++)
    v += a[i] * b[i];
  free(a);
  free(b);
  *out = v;
  return MATH_SUCCESS;
}

clm_math_status clm_math_vector_length(clm_id x, double *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *a, v = 0;
  size_t n;
  clm_math_status s = clm_math_read_vector(x, 0, &a, &n);
  if (s)
    return s;
  for (size_t i = 0; i < n; i++)
    v = hypot(v, a[i]);
  free(a);
  *out = v;
  return MATH_SUCCESS;
}

clm_math_status clm_math_vector_distance(clm_id x, clm_id y,
                                                       double *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *a, *b, v = 0;
  size_t n;
  clm_math_status s = clm_private_math_vector_two(x, y, &a, &b, &n);
  if (s)
    return s;
  for (size_t i = 0; i < n; i++)
    v = hypot(v, a[i] - b[i]);
  free(a);
  free(b);
  *out = v;
  return MATH_SUCCESS;
}

clm_math_status clm_math_vector_normalize(clm_id x, clm_id dst) {
  double *a, out[4], mag = 0;
  size_t n;
  clm_math_status s = clm_math_read_vector(x, 0, &a, &n);
  if (s)
    return s;
  for (size_t i = 0; i < n; i++)
    mag = hypot(mag, a[i]);
  if (mag == 0) {
    free(a);
    return MATH_ERROR_DOMAIN;
  }
  for (size_t i = 0; i < n; i++)
    out[i] = a[i] / mag;
  free(a);
  return clm_math_write_double(dst, out, n);
}

clm_math_status clm_math_vector_cross(clm_id x, clm_id y,
                                                    clm_id dst) {
  double *a, *b;
  clm_math_status s = clm_math_read_vector(x, 3, &a, NULL);
  if (s)
    return s;
  s = clm_math_read_vector(y, 3, &b, NULL);
  if (s) {
    free(a);
    return s;
  }
  double v[] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                a[0] * b[1] - a[1] * b[0]};
  free(a);
  free(b);
  return clm_math_write_double(dst, v, 3);
}

clm_math_status clm_math_vector_reflect(clm_id x, clm_id y,
                                                      clm_id dst) {
  double *a, *b, out[4], nn = 0, dot = 0;
  size_t n;
  clm_math_status s = clm_private_math_vector_two(x, y, &a, &b, &n);
  if (s)
    return s;
  for (size_t i = 0; i < n; i++) {
    nn += b[i] * b[i];
    dot += a[i] * b[i];
  }
  if (nn == 0) {
    free(a);
    free(b);
    return MATH_ERROR_DOMAIN;
  }
  for (size_t i = 0; i < n; i++)
    out[i] = a[i] - 2 * dot / nn * b[i];
  free(a);
  free(b);
  return clm_math_write_double(dst, out, n);
}

clm_math_status clm_math_vector_lerp(clm_id x, clm_id y, double t,
                                                   clm_id dst) {
  if (!isfinite(t))
    return MATH_ERROR_VALUE;
  double *a, *b, out[4];
  size_t n;
  clm_math_status s = clm_private_math_vector_two(x, y, &a, &b, &n);
  if (s)
    return s;
  for (size_t i = 0; i < n; i++)
    out[i] = a[i] + (b[i] - a[i]) * t;
  free(a);
  free(b);
  return clm_math_write_double(dst, out, n);
}
