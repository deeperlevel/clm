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
#define CLM_MATH_PI 3.141592653589793238462643383279502884
#define CLM_MATH_TAU (2.0 * CLM_MATH_PI)
#define CLM_MATH_E 2.718281828459045235360287471352662498

/** Clamps value into [low, high]; rejects nonfinite inputs and reversed bounds. */
static inline clm_math_status clm_math_clamp(double value, double low,
                                             double high, double *out);

/** Interpolates linearly; t is not clamped to [0,1]. */
static inline double clm_math_lerp(double a, double b, double t);

/** Returns the unbounded interpolation factor; equal endpoints are a domain error. */
static inline clm_math_status clm_math_inverse_lerp(double a, double b,
                                                    double value, double *out);

/** Computes clamped cubic smoothstep over [a,b]. */
static inline clm_math_status clm_math_smoothstep(double a, double b,
                                                  double value, double *out);

/** Converts radians to degrees. */
static inline double clm_math_degrees(double radians);

/** Converts degrees to radians. */
static inline double clm_math_radians(double degrees);

/** Standard trigonometric functions; inputs and outputs use radians. */
static inline double clm_math_sin(double x);
static inline double clm_math_cos(double x);
static inline double clm_math_tan(double x);

/** Inverse sine and cosine; results are in radians. */
static inline double clm_math_asin(double x);
static inline double clm_math_acos(double x);

/** Returns the angle in radians whose tangent is y/x, preserving the quadrant. */
static inline double clm_math_atan2(double y, double x);

/** Square root and power wrappers using the C math library domain behavior. */
static inline double clm_math_sqrt(double x);
static inline double clm_math_pow(double x, double y);

/** Computes logarithm(value) in the requested base; value/base must be positive
 * and base must not equal one.
 */
static inline clm_math_status clm_math_log(double value, double base,
                                           double *out);

/** Exponential and integral rounding helpers. */
static inline double clm_math_exp(double x);
static inline double clm_math_floor(double x);
static inline double clm_math_ceil(double x);

/** Absolute value and sign (-1, 0, or 1). */
static inline double clm_math_abs(double x);
static inline int clm_math_sign(double x);

/** Wraps value into [low, high); low must be less than high. */
static inline clm_math_status clm_math_wrap(double value, double low,
                                            double high, double *out);

/** Compares values using max(absolute_tolerance, relative_tolerance * scale).
 * Negative tolerances and NaN inputs return false.
 */
static inline bool clm_math_approximately(double a, double b,
                                          double absolute_tolerance,
                                          double relative_tolerance);

/* Inline implementation â€” no separate .c file required. */
#include <math.h>
static inline clm_math_status clm_math_clamp(double v, double lo, double hi,
                                             double *o) {
  if (!o || !isfinite(v) || !isfinite(lo) || !isfinite(hi))
    return MATH_ERROR_VALUE;
  if (lo > hi)
    return MATH_ERROR_DOMAIN;
  *o = fmax(lo, fmin(v, hi));
  return MATH_SUCCESS;
}
static inline double clm_math_lerp(double a, double b, double t) {
  return a + (b - a) * t;
}
static inline clm_math_status clm_math_inverse_lerp(double a, double b,
                                                    double v, double *o) {
  if (!o || !isfinite(a) || !isfinite(b) || !isfinite(v))
    return MATH_ERROR_VALUE;
  if (a == b)
    return MATH_ERROR_DOMAIN;
  *o = (v - a) / (b - a);
  return MATH_SUCCESS;
}
static inline clm_math_status clm_math_smoothstep(double a, double b, double v,
                                                  double *o) {
  double t;
  clm_math_status s = clm_math_inverse_lerp(a, b, v, &t);
  if (s)
    return s;
  t = fmax(0, fmin(1, t));
  *o = t * t * (3 - 2 * t);
  return MATH_SUCCESS;
}
static inline double clm_math_degrees(double a) {
  return a * 180. / CLM_MATH_PI;
}
static inline double clm_math_radians(double a) {
  return a * CLM_MATH_PI / 180.;
}
static inline double clm_math_sin(double x) { return sin(x); }
static inline double clm_math_cos(double x) { return cos(x); }
static inline double clm_math_tan(double x) { return tan(x); }
static inline double clm_math_asin(double x) { return asin(x); }
static inline double clm_math_acos(double x) { return acos(x); }
static inline double clm_math_atan2(double y, double x) { return atan2(y, x); }
static inline double clm_math_sqrt(double x) { return sqrt(x); }
static inline double clm_math_pow(double x, double y) { return pow(x, y); }
static inline clm_math_status clm_math_log(double v, double base, double *o) {
  if (!o || !isfinite(v) || !isfinite(base))
    return MATH_ERROR_VALUE;
  if (v <= 0 || base <= 0 || base == 1)
    return MATH_ERROR_DOMAIN;
  *o = log(v) / log(base);
  return MATH_SUCCESS;
}
static inline double clm_math_exp(double x) { return exp(x); }
static inline double clm_math_floor(double x) { return floor(x); }
static inline double clm_math_ceil(double x) { return ceil(x); }
static inline double clm_math_abs(double x) { return fabs(x); }
static inline int clm_math_sign(double x) { return (x > 0) - (x < 0); }
static inline clm_math_status clm_math_wrap(double v, double lo, double hi,
                                            double *o) {
  if (!o || !isfinite(v) || !isfinite(lo) || !isfinite(hi))
    return MATH_ERROR_VALUE;
  if (lo >= hi)
    return MATH_ERROR_DOMAIN;
  double w = fmod(v - lo, hi - lo);
  if (w < 0)
    w += hi - lo;
  *o = lo + w;
  return MATH_SUCCESS;
}
static inline bool clm_math_approximately(double a, double b, double at,
                                          double rt) {
  if (at < 0 || rt < 0 || isnan(a) || isnan(b))
    return false;
  return a == b || fabs(a - b) <= fmax(at, rt * fmax(fabs(a), fabs(b)));
}

