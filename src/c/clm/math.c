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

#include "math.h"

#include <math.h>

clm_math_status clm_math_clamp(const double value, const double low,
                               const double high, double *out)
{
  if (!out || !isfinite(value) || !isfinite(low) || !isfinite(high))
    return MATH_ERROR_VALUE;
  if (low > high)
    return MATH_ERROR_DOMAIN;
  *out = fmax(low, fmin(value, high));
  return MATH_SUCCESS;
}

double clm_math_lerp(const double a, const double b, const double t)
{
  return a + (b - a) * t;
}

clm_math_status clm_math_inverse_lerp(const double a, const double b,
                                      const double value, double *out)
{
  if (!out || !isfinite(a) || !isfinite(b) || !isfinite(value))
    return MATH_ERROR_VALUE;
  if (a == b)
    return MATH_ERROR_DOMAIN;
  *out = (value - a) / (b - a);
  return MATH_SUCCESS;
}

clm_math_status clm_math_smoothstep(const double a, const double b,
                                    const double value, double *out)
{
  double t;
  const clm_math_status status = clm_math_inverse_lerp(a, b, value, &t);
  if (status)
    return status;
  t = fmax(0, fmin(1, t));
  *out = t * t * (3 - 2 * t);
  return MATH_SUCCESS;
}

double clm_math_degrees(double radians)
{
  return radians * 180. / CLM_MATH_PI;
}

double clm_math_radians(double degrees)
{
  return degrees * CLM_MATH_PI / 180.;
}

double clm_math_sin(double radians) { return sin(radians); }
double clm_math_cos(double radians) { return cos(radians); }
double clm_math_tan(double radians) { return tan(radians); }
double clm_math_asin(double value) { return asin(value); }
double clm_math_acos(double value) { return acos(value); }
double clm_math_atan2(double y, double x) { return atan2(y, x); }
double clm_math_sqrt(double value) { return sqrt(value); }
double clm_math_pow(double x, double y) { return pow(x, y); }

clm_math_status clm_math_log(double value, double base, double *out)
{
  if (!out || !isfinite(value) || !isfinite(base))
    return MATH_ERROR_VALUE;
  if (value <= 0 || base <= 0 || base == 1)
    return MATH_ERROR_DOMAIN;
  *out = log(value) / log(base);
  return MATH_SUCCESS;
}

double clm_math_exp(double x) { return exp(x); }
double clm_math_floor(double x) { return floor(x); }
double clm_math_ceil(double x) { return ceil(x); }
double clm_math_abs(double x) { return fabs(x); }
int clm_math_sign(double x) { return (x > 0) - (x < 0); }

clm_math_status clm_math_wrap(double value, double low, double high,
                              double *out)
{
  if (!out || !isfinite(value) || !isfinite(low) || !isfinite(high))
    return MATH_ERROR_VALUE;
  if (low >= high)
    return MATH_ERROR_DOMAIN;
  double wrapped = fmod(value - low, high - low);
  if (wrapped < 0)
    wrapped += high - low;
  *out = low + wrapped;
  return MATH_SUCCESS;
}

bool clm_math_approximately(double a, double b, double absolute_tolerance,
                            double relative_tolerance)
{
  if (absolute_tolerance < 0 || relative_tolerance < 0 || isnan(a) || isnan(b))
    return false;
  return a == b || fabs(a - b) <=
      fmax(absolute_tolerance,
           relative_tolerance * fmax(fabs(a), fabs(b)));
}
