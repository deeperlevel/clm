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
/* Matrices are row-major DOUBLE Storage, with explicit positive dimensions. */
/* Inline implementation — no separate .c file required. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
clm_math_status clm_private_math_matrix_shape(size_t r,
                                                            size_t c);
/** Writes a size-by-size identity matrix. 
 * @param dst Input parameter described above.
 * @param n Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_matrix_identity(clm_id dst, size_t n);
/** Multiplies row-major A(ra x ca) by B(rb x cb); ca must equal rb. 
 * @param x Input value or coordinate.
 * @param ra Input parameter described above.
 * @param ca Input parameter described above.
 * @param y Input value or coordinate.
 * @param rb Input parameter described above.
 * @param cb Input parameter described above.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_matrix_multiply(clm_id x, size_t ra,
                                                       size_t ca, clm_id y,
                                                       size_t rb, size_t cb,
                                                       clm_id dst);
/** Transposes a row-major rows-by-cols matrix. 
 * @param x Input value or coordinate.
 * @param r Input parameter described above.
 * @param c Input parameter described above.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_matrix_transpose(clm_id x, size_t r,
                                                        size_t c, clm_id dst);
/** Multiplies a rows-by-cols matrix by a vector of length cols. 
 * @param x Input value or coordinate.
 * @param r Input parameter described above.
 * @param c Input parameter described above.
 * @param y Input value or coordinate.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_matrix_transform_vector(clm_id x, size_t r, size_t c, clm_id y,
                                 clm_id dst);
clm_math_status clm_private_math_matrix_elimination(double *a,
                                                                  size_t n,
                                                                  double *det,
                                                                  double *inv);
/** Computes the determinant of a size-by-size matrix. 
 * @param x Input value or coordinate.
 * @param n Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_matrix_determinant(clm_id x, size_t n,
                                                          double *out);
/** Inverts a nonsingular size-by-size matrix. Singular input returns an error. 
 * @param x Input value or coordinate.
 * @param n Input parameter described above.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_matrix_inverse(clm_id x, size_t n,
                                                      clm_id dst);
/** Writes a 2D rotation matrix; angle is in radians. 
 * @param t Interpolation factor; see summary for range behavior.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_matrix_rotation2d(double t, clm_id dst);
/** Writes a 3x3 homogeneous 2D translation matrix. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_matrix_translation2d(double x, double y,
                                                            clm_id dst);
/** Writes a 3x3 homogeneous 2D scale matrix. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_matrix_scale2d(double x, double y,
                                                      clm_id dst);
