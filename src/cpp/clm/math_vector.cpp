#pragma once

/*
 * =============================================================================
 * CLM â€” Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain â€” CC0 1.0 Universal
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

#include "math_core.hpp"
/* Vectors are DOUBLE Storage IDs with exactly 2, 3 or 4 elements. */
/** Replaces destination with count components; count must be 2, 3, or 4. */
static inline clm_math_status
clm_math_vector_set(clm_id destination, const double *components, size_t count);

/** Adds equal-dimension vectors and writes the result to destination. */
static inline clm_math_status clm_math_vector_add(clm_id first, clm_id second,
                                                  clm_id destination);

/** Subtracts second from first; dimensions must match. */
static inline clm_math_status
clm_math_vector_subtract(clm_id first, clm_id second, clm_id destination);

/** Scales a vector by a finite factor. */
static inline clm_math_status
clm_math_vector_scale(clm_id source, double factor, clm_id destination);

/** Computes the dot product of equal-dimension vectors. */
static inline clm_math_status clm_math_vector_dot(clm_id first, clm_id second,
                                                  double *out);

/** Computes the Euclidean magnitude of a 2D, 3D, or 4D vector. */
static inline clm_math_status clm_math_vector_length(clm_id source,
                                                     double *out);

/** Computes Euclidean distance between equal-dimension vectors. */
static inline clm_math_status
clm_math_vector_distance(clm_id first, clm_id second, double *out);

/** Normalizes a nonzero vector; zero vectors return MATH_ERROR_DOMAIN. */
static inline clm_math_status clm_math_vector_normalize(clm_id source,
                                                        clm_id destination);

/** Computes the 3D cross product; both inputs must contain three components. */
static inline clm_math_status clm_math_vector_cross(clm_id first, clm_id second,
                                                    clm_id destination);

/** Reflects direction about a nonzero normal; dimensions must match. */
static inline clm_math_status
clm_math_vector_reflect(clm_id direction, clm_id normal, clm_id destination);

/** Linearly interpolates equal-dimension vectors; t is not clamped. */
static inline clm_math_status
clm_math_vector_lerp(clm_id first, clm_id second, double t, clm_id destination);

/* Inline implementation â€” no separate .c file required. */
#include <math.h>
#include <stdlib.h>
static inline clm_math_status clm_math_vector_set(clm_id id, const double *a,
                                                  size_t n) {
  if (n < 2 || n > 4)
    return MATH_ERROR_DIMENSION;
  return clm_math_write_double(id, a, n);
}
static inline clm_math_status
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
static inline clm_math_status clm_math_vector_add(clm_id x, clm_id y,
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
static inline clm_math_status clm_math_vector_subtract(clm_id x, clm_id y,
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
static inline clm_math_status clm_math_vector_scale(clm_id x, double f,
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
static inline clm_math_status clm_math_vector_dot(clm_id x, clm_id y,
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
static inline clm_math_status clm_math_vector_length(clm_id x, double *out) {
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
static inline clm_math_status clm_math_vector_distance(clm_id x, clm_id y,
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
static inline clm_math_status clm_math_vector_normalize(clm_id x, clm_id dst) {
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
static inline clm_math_status clm_math_vector_cross(clm_id x, clm_id y,
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
static inline clm_math_status clm_math_vector_reflect(clm_id x, clm_id y,
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
static inline clm_math_status clm_math_vector_lerp(clm_id x, clm_id y, double t,
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

