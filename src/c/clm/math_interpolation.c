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

#include "math_interpolation.h"

double clm_math_interpolation_lerp(double a, double b, double t) {
  return clm_math_lerp(a, b, t);
}

clm_math_status
clm_math_interpolation_inverse_lerp(double a, double b, double v, double *out) {
  return clm_math_inverse_lerp(a, b, v, out);
}

clm_math_status
clm_math_interpolation_smoothstep(double a, double b, double v, double *out) {
  return clm_math_smoothstep(a, b, v, out);
}

double clm_math_interpolation_ease_in(double t) { return t * t; }

double clm_math_interpolation_ease_out(double t) {
  return 1 - (1 - t) * (1 - t);
}

double clm_math_interpolation_ease_in_out(double t) {
  return t * t * (3 - 2 * t);
}

double clm_math_interpolation_cubic(double a, double b, double c,
                                                  double d, double t) {
  return .5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t +
               (-a + 3 * b - 3 * c + d) * t * t * t);
}

clm_math_status clm_math_interpolation_storage_lerp(clm_id first,
                                                                  clm_id second,
                                                                  double t,
                                                                  clm_id dest) {
  return clm_math_vector_lerp(first, second, t, dest);
}
