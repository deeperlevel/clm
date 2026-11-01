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
/* Quaternion order: [x,y,z,w]. Axis and vectors: 3 doubles. */
/* Inline implementation — no separate .c file required. */
#include <math.h>
#include <stdlib.h>
/** Writes the identity quaternion [0,0,0,1]. 
 * @param d Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_quaternion_identity(clm_id d);
/** Writes quaternion components in [x,y,z,w] order. 
 * @param d Input parameter described above.
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param z Input value or coordinate.
 * @param w Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_quaternion_set(clm_id d, double x, double y, double z, double w);
/** Normalizes a nonzero quaternion. 
 * @param x Input value or coordinate.
 * @param d Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_quaternion_normalize(clm_id x,
                                                            clm_id d);
/** Writes the quaternion conjugate. 
 * @param x Input value or coordinate.
 * @param d Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_quaternion_conjugate(clm_id x,
                                                            clm_id d);
/** Computes the Hamilton product of two quaternions. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param d Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_quaternion_multiply(clm_id x, clm_id y,
                                                           clm_id d);
/** Builds a quaternion from an axis and radian angle; the axis is normalized. 
 * @param axis Input parameter described above.
 * @param angle Angle in radians.
 * @param d Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_quaternion_from_axis_angle(clm_id axis, double angle, clm_id d);
/** Rotates a three-element vector; nonunit quaternions are supported. 
 * @param q Input parameter described above.
 * @param x Input value or coordinate.
 * @param d Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_quaternion_rotate_vector(clm_id q, clm_id x, clm_id d);
/** Performs shortest-arc spherical interpolation; t is not clamped. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param t Interpolation factor; see summary for range behavior.
 * @param d Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_quaternion_slerp(clm_id x, clm_id y,
                                                        double t, clm_id d);
