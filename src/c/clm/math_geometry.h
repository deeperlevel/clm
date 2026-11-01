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
/* Geometry: 2D points [x,y], polygons [x0,y0,...] in DOUBLE Storage. */
/* Inline implementation — no separate .c file required. */
#include <math.h>
#include <stdlib.h>
clm_math_status
clm_private_math_geometry_trip(clm_id x, clm_id y, clm_id z, double **a,
                               double **b, double **c);
void clm_private_math_geometry_closest(double *p, double *a,
                                                     double *b, double *out);
/** Computes the shortest distance from a point to a closed line segment. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param z Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_geometry_distance_point_segment(clm_id x, clm_id y, clm_id z,
                                         double *out);
/** Computes the signed 2D cross product (orientation) of three points. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param z Input value or coordinate.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_geometry_orientation(clm_id x, clm_id y, clm_id z, double *out);
/** Writes the closest point on a segment to a 2D point into destination. 
 * @param x Input value or coordinate.
 * @param y Input value or coordinate.
 * @param z Input value or coordinate.
 * @param dst Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_geometry_closest_point_segment(clm_id x, clm_id y, clm_id z,
                                        clm_id dst);
clm_math_status
clm_private_math_geometry_poly(clm_id id, double **v, size_t *n);
/** Tests polygon membership using even/odd crossing; boundary points count inside. 
 * @param p Input parameter described above.
 * @param polygon Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_geometry_point_in_polygon(clm_id p, clm_id polygon, bool *out);
/** Computes unsigned shoelace area; polygon needs at least three 2D points. 
 * @param poly_id CLM ID identifying the poly.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_geometry_polygon_area(clm_id poly_id,
                                                             double *out);
