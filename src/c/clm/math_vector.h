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
/* Vectors are DOUBLE Storage IDs with exactly 2, 3 or 4 elements. */
/* Inline implementation — no separate .c file required. */
#include <math.h>
#include <stdlib.h>
/** Replaces destination with count components; count must be 2, 3, or 4. 
 * @param id CLM ID identifying the id.
 * @param a Input data or buffer; not modified.
 * @param n Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_set(clm_id id, const double *a,
                                                  size_t n);
clm_math_status
clm_private_math_vector_two(clm_id first, clm_id second, double **a, double **b,
                            size_t *n);
/** Adds equal-dimension vectors and writes the result to destination. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_add(clm_id x, clm_id y,
                                                  clm_id dst);
/** Subtracts second from first; dimensions must match. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_subtract(clm_id x, clm_id y,
                                                       clm_id dst);
/** Scales a vector by a finite factor. 
 * @param x Input value or coordinate.
 * @param f Input parameter described above.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_scale(clm_id x, double f,
                                                    clm_id dst);
/** Computes the dot product of equal-dimension vectors. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_dot(clm_id x, clm_id y,
                                                  double *out);
/** Computes the Euclidean magnitude of a 2D, 3D, or 4D vector. 
 * @param x Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_length(clm_id x, double *out);
/** Computes Euclidean distance between equal-dimension vectors. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_distance(clm_id x, clm_id y,
                                                       double *out);
/** Normalizes a nonzero vector; zero vectors return MATH_ERROR_DOMAIN. 
 * @param x Input value or coordinate.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_normalize(clm_id x, clm_id dst);
clm_math_status clm_math_vector_cross(clm_id x, clm_id y,
                                                    clm_id dst);
/** Reflects direction about a nonzero normal; dimensions must match. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_reflect(clm_id x, clm_id y,
                                                      clm_id dst);
/** Linearly interpolates equal-dimension vectors; t is not clamped. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param t Interpolation factor; see summary for range behavior.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_vector_lerp(clm_id x, clm_id y, double t,
                                                   clm_id dst);
