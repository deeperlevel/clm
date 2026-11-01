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

#include "math.h"
#include "math_vector.h"

/* Inline implementation — no separate .c file required. */
/** Scalar linear interpolation; t is not clamped. 
 * @param a First input value.
 * @param b Second input value.
 * @param t Interpolation factor; see summary for range behavior.
 * @return Interpolated value.
 */
double clm_math_interpolation_lerp(double a, double b, double t);

/** Returns the unbounded interpolation factor; equal endpoints are invalid. 
 * @param a First input value.
 * @param b Second input value.
 * @param v Input parameter described above.
 * @param out Writable output buffer.
 * @return The unbounded interpolation factor; equal endpoints are invalid.
 */
clm_math_status
clm_math_interpolation_inverse_lerp(double a, double b, double v, double *out);

/** Computes smoothstep after clamping the normalized factor to [0,1]. 
 * @param a First input value.
 * @param b Second input value.
 * @param v Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_interpolation_smoothstep(double a, double b, double v, double *out);

/** Computes quadratic ease-in; t is not clamped. 
 * @param t Interpolation factor; see summary for range behavior.
 * @return Computed result described in the summary above.
 */
double clm_math_interpolation_ease_in(double t);

/** Computes quadratic ease-out; t is not clamped. 
 * @param t Interpolation factor; see summary for range behavior.
 * @return Computed result described in the summary above.
 */
double clm_math_interpolation_ease_out(double t);

/** Computes quadratic ease-in-out; t is not clamped. 
 * @param t Interpolation factor; see summary for range behavior.
 * @return Computed result described in the summary above.
 */
double clm_math_interpolation_ease_in_out(double t);

/** Computes four-point Catmull-Rom interpolation; t is not clamped. 
 * @param a First input value.
 * @param b Second input value.
 * @param c Input parameter described above.
 * @param d Input parameter described above.
 * @param t Interpolation factor; see summary for range behavior.
 * @return Computed result described in the summary above.
 */
double clm_math_interpolation_cubic(double a, double b, double c,
                                                  double d, double t);

/** Applies linear interpolation elementwise to equal-sized vector storages. 
 * @param first First operand or source.
 * @param second Second operand or source.
 * @param t Interpolation factor; see summary for range behavior.
 * @param dest Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_interpolation_storage_lerp(clm_id first,
                                                                  clm_id second,
                                                                  double t,
                                                                  clm_id dest);
