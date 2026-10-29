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

#include "math.h"
#include "math_vector.h"

/** Scalar linear interpolation; t is not clamped. */
static inline double clm_math_interpolation_lerp(double a, double b, double t);

/** Returns the unbounded interpolation factor; equal endpoints are invalid. */
static inline clm_math_status
clm_math_interpolation_inverse_lerp(double a, double b, double v, double *out);

/** Computes smoothstep after clamping the normalized factor to [0,1]. */
static inline clm_math_status
clm_math_interpolation_smoothstep(double a, double b, double v, double *out);

/** Quadratic easing functions; t is not clamped. */
static inline double clm_math_interpolation_ease_in(double t);
static inline double clm_math_interpolation_ease_out(double t);
static inline double clm_math_interpolation_ease_in_out(double t);

/** Four-point Catmull-Rom interpolation; t is not clamped. */
static inline double clm_math_interpolation_cubic(double a, double b, double c,
                                                  double d, double t);

/** Applies linear interpolation elementwise to equal-sized vector storages. */
static inline clm_math_status
clm_math_interpolation_storage_lerp(clm_id first, clm_id second, double t,
                                    clm_id destination);

/* Inline implementation — no separate .c file required. */
static inline double clm_math_interpolation_lerp(double a, double b, double t) {
  return clm_math_lerp(a, b, t);
}
static inline clm_math_status
clm_math_interpolation_inverse_lerp(double a, double b, double v, double *out) {
  return clm_math_inverse_lerp(a, b, v, out);
}
static inline clm_math_status
clm_math_interpolation_smoothstep(double a, double b, double v, double *out) {
  return clm_math_smoothstep(a, b, v, out);
}
static inline double clm_math_interpolation_ease_in(double t) { return t * t; }
static inline double clm_math_interpolation_ease_out(double t) {
  return 1 - (1 - t) * (1 - t);
}
static inline double clm_math_interpolation_ease_in_out(double t) {
  return t * t * (3 - 2 * t);
}
static inline double clm_math_interpolation_cubic(double a, double b, double c,
                                                  double d, double t) {
  return .5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t +
               (-a + 3 * b - 3 * c + d) * t * t * t);
}
static inline clm_math_status clm_math_interpolation_storage_lerp(clm_id first,
                                                                  clm_id second,
                                                                  double t,
                                                                  clm_id dest) {
  return clm_math_vector_lerp(first, second, t, dest);
}
