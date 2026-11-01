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
#define CLM_MATH_PI 3.141592653589793238462643383279502884
#define CLM_MATH_TAU (2.0 * CLM_MATH_PI)
#define CLM_MATH_E 2.718281828459045235360287471352662498

/** Clamps value into [low, high].
 * @param value Finite value to clamp.
 * @param low Finite lower bound; must not exceed high.
 * @param high Finite upper bound.
 * @param[out] out Receives the clamped value.
 * @return MATH_ERROR_VALUE for a null output or nonfinite input,
 * MATH_ERROR_DOMAIN for reversed bounds, or MATH_SUCCESS.
 */
clm_math_status clm_math_clamp(
  const double value,
  const double low,
  const double high,
  double *out
);

/** Linearly interpolates from a to b without clamping t.
 * @param a Start value.
 * @param b End value.
 * @param t Interpolation factor; values outside [0, 1] extrapolate.
 * @return a + (b - a) * t.
 */
double clm_math_lerp(
  const double a,
  const double b,
  const double t
);

/** Computes the unbounded interpolation factor for value between a and b.
 * @param a Start endpoint.
 * @param b End endpoint; must differ from a.
 * @param value Value whose interpolation factor is requested.
 * @param[out] out Receives (value - a) / (b - a).
 * @return MATH_ERROR_VALUE for a null output or nonfinite input,
 * MATH_ERROR_DOMAIN for equal endpoints, or MATH_SUCCESS.
 */
clm_math_status clm_math_inverse_lerp(
  const double a,
  const double b,
  const double value,
  double *out
);

/** Computes cubic smoothstep, clamping the normalized value into [0, 1].
 * @param a Start endpoint.
 * @param b End endpoint; must differ from a.
 * @param value Value to map between the endpoints.
 * @param[out] out Receives the smoothstep result.
 * @return MATH_ERROR_VALUE for a null output or nonfinite input,
 * MATH_ERROR_DOMAIN for equal endpoints, or MATH_SUCCESS.
 */
clm_math_status clm_math_smoothstep(
  const double a,
  const double b,
  const double value,
  double *out
);

/** Converts radians to degrees.
 * @param radians Angle in radians.
 * @return The angle in degrees.
 */
double clm_math_degrees(double radians);

/** Converts degrees to radians.
 * @param degrees Angle in degrees.
 * @return The angle in radians.
 */
double clm_math_radians(double degrees);

/** Computes the sine of an angle in radians. 
 * @param radians Angle in radians.
 * @return Sine of the input angle, in radians.
 */
double clm_math_sin(double radians);

/** Computes the cosine of an angle in radians. 
 * @param radians Angle in radians.
 * @return Cosine of the input angle, in radians.
 */
double clm_math_cos(double radians);

/** Computes the tangent of an angle in radians. 
 * @param radians Angle in radians.
 * @return Tangent of the input angle, in radians.
 */
double clm_math_tan(double radians);

/** Computes the inverse sine; the result is in radians. 
 * @param value Value to process.
 * @return Inverse sine of the input value, in radians.
 */
double clm_math_asin(double value);

/** Computes the inverse cosine; the result is in radians. 
 * @param value Value to process.
 * @return Inverse cosine of the input value, in radians.
 */
double clm_math_acos(double value);

/** Computes the angle in radians for the coordinates y and x.
 * @return An angle in [-pi, pi], preserving the input quadrant.
 * @param y Input value or coordinate.
 * @param x Input value or coordinate.
 */
double clm_math_atan2(double y, double x);

/** Computes the square root using the C math library domain behavior. 
 * @param value Value to process.
 * @return Square root of the input value.
 */
double clm_math_sqrt(double value);

/** Raises x to the power y using the C math library domain behavior. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @return Input value raised to the specified power.
 */
double clm_math_pow(double x, double y);

/** Computes a logarithm in the requested base.
 * @param value Positive value whose logarithm is requested.
 * @param base Positive base other than one.
 * @param[out] out Receives log(value) / log(base).
 * @return MATH_ERROR_VALUE for a null output or nonfinite input,
 * MATH_ERROR_DOMAIN for an invalid value or base, or MATH_SUCCESS.
 */
clm_math_status clm_math_log(double value, double base, double *out);

/** Computes e raised to x using the C math library. 
 * @param x Input value or coordinate.
 * @return Exponential of the input value.
 */
double clm_math_exp(double x);

/** Rounds x toward negative infinity. 
 * @param x Input value or coordinate.
 * @return Input rounded toward negative infinity.
 */
double clm_math_floor(double x);

/** Rounds x toward positive infinity. 
 * @param x Input value or coordinate.
 * @return Input rounded toward positive infinity.
 */
double clm_math_ceil(double x);

/** Returns the absolute value of x. 
 * @param x Input value or coordinate.
 * @return The absolute value of x.
 */
double clm_math_abs(double x);

/** Returns -1 for negative x, 0 for zero, and 1 for positive x. 
 * @param x Input value or coordinate.
 * @return -1 for negative x, 0 for zero, and 1 for positive x.
 */
int clm_math_sign(double x);

/** Wraps value into the half-open interval [low, high).
 * @param value Finite value to wrap.
 * @param low Finite inclusive lower bound.
 * @param high Finite exclusive upper bound; must exceed low.
 * @param[out] out Receives the wrapped value.
 * @return MATH_ERROR_VALUE for a null output or nonfinite input,
 * MATH_ERROR_DOMAIN for invalid bounds, or MATH_SUCCESS.
 */
clm_math_status clm_math_wrap(double value, double low, double high,
                              double *out);

/** Compares two values using the larger of absolute and relative tolerances.
 * @param a First value.
 * @param b Second value.
 * @param absolute_tolerance Nonnegative absolute tolerance.
 * @param relative_tolerance Nonnegative tolerance scaled by max(|a|, |b|).
 * @return Whether the values match within tolerance; false for negative
 * tolerances or NaN values.
 */
bool clm_math_approximately(double a, double b, double absolute_tolerance,
                            double relative_tolerance);
