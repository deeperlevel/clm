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
/* AABB [min_x,min_y,max_x,max_y]; circle [cx,cy,r]. Touching counts as
 * collision. */
/* Inline implementation — no separate .c file required. */
#include <math.h>
#include <stdlib.h>
clm_math_status clm_private_math_collision_box(clm_id x,
                                                             double **out);
clm_math_status clm_private_math_collision_circle(clm_id x,
                                                                double **out);
/** Tests overlap of two axis-aligned bounding boxes. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_collision_aabb_aabb(clm_id x, clm_id y,
                                                           bool *out);
/** Tests whether a point lies within or on an axis-aligned bounding box. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_collision_point_aabb(clm_id x, clm_id y,
                                                            bool *out);
/** Tests overlap or touching of two circles. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_collision_circle_circle(clm_id x, clm_id y, bool *out);
/** Tests overlap or touching between a circle and an axis-aligned box. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_collision_circle_aabb(clm_id x, clm_id y,
                                                             bool *out);
/** Tests whether a point lies within or on a circle. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_collision_point_circle(clm_id x, clm_id y, bool *out);
/** Ray/AABB intersection for a 2D origin and direction.
 * A miss returns MATH_SUCCESS with *hit=false; a hit distance is nonnegative.
 * A zero direction vector is a domain error.
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param z Input value or coordinate.
 * @param hit Writable output or in/out pointer.
 * @param distance Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_collision_ray_aabb(clm_id x, clm_id y,
                                                          clm_id z, bool *hit,
                                                          double *distance);
