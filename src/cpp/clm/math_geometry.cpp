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
/* Geometry: 2D points [x,y], polygons [x0,y0,...] in DOUBLE Storage. */
/** Computes the shortest distance from a point to a closed line segment. */
static inline clm_math_status
clm_math_geometry_distance_point_segment(clm_id point, clm_id start, clm_id end,
                                         double *out);

/** Computes the signed 2D cross product (orientation) of three points. */
static inline clm_math_status clm_math_geometry_orientation(clm_id first,
                                                            clm_id second,
                                                            clm_id third,
                                                            double *out);

/** Tests polygon membership using even/odd crossing; boundary points count inside. */
static inline clm_math_status
clm_math_geometry_point_in_polygon(clm_id point, clm_id polygon, bool *out);

/** Computes unsigned shoelace area; polygon needs at least three 2D points. */
static inline clm_math_status clm_math_geometry_polygon_area(clm_id polygon,
                                                             double *out);

/** Writes the closest point on a segment to a 2D point into destination. */
static inline clm_math_status
clm_math_geometry_closest_point_segment(clm_id point, clm_id start, clm_id end,
                                        clm_id destination);

/* Inline implementation â€” no separate .c file required. */
#include <math.h>
#include <stdlib.h>
static inline clm_math_status
clm_private_math_geometry_trip(clm_id x, clm_id y, clm_id z, double **a,
                               double **b, double **c) {
  clm_math_status s = clm_math_read_double(x, 2, a, NULL);
  if (s)
    return s;
  s = clm_math_read_double(y, 2, b, NULL);
  if (s) {
    free(*a);
    return s;
  }
  s = clm_math_read_double(z, 2, c, NULL);
  if (s) {
    free(*a);
    free(*b);
  }
  return s;
}
static inline void clm_private_math_geometry_closest(double *p, double *a,
                                                     double *b, double *out) {
  double dx = b[0] - a[0], dy = b[1] - a[1], len = dx * dx + dy * dy,
         t = len ? ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / len : 0;
  t = fmax(0, fmin(1, t));
  out[0] = a[0] + t * dx;
  out[1] = a[1] + t * dy;
}
static inline clm_math_status
clm_math_geometry_distance_point_segment(clm_id x, clm_id y, clm_id z,
                                         double *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *a, *b, *c, v[2];
  clm_math_status s = clm_private_math_geometry_trip(x, y, z, &a, &b, &c);
  if (s)
    return s;
  clm_private_math_geometry_closest(a, b, c, v);
  *out = hypot(a[0] - v[0], a[1] - v[1]);
  free(a);
  free(b);
  free(c);
  return MATH_SUCCESS;
}
static inline clm_math_status
clm_math_geometry_orientation(clm_id x, clm_id y, clm_id z, double *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *a, *b, *c;
  clm_math_status s = clm_private_math_geometry_trip(x, y, z, &a, &b, &c);
  if (s)
    return s;
  *out = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
  free(a);
  free(b);
  free(c);
  return MATH_SUCCESS;
}
static inline clm_math_status
clm_math_geometry_closest_point_segment(clm_id x, clm_id y, clm_id z,
                                        clm_id dst) {
  double *a, *b, *c, v[2];
  clm_math_status s = clm_private_math_geometry_trip(x, y, z, &a, &b, &c);
  if (s)
    return s;
  clm_private_math_geometry_closest(a, b, c, v);
  free(a);
  free(b);
  free(c);
  return clm_math_write_double(dst, v, 2);
}
static inline clm_math_status
clm_private_math_geometry_poly(clm_id id, double **v, size_t *n) {
  clm_math_status s = clm_math_read_double(id, 0, v, n);
  if (s)
    return s;
  if (*n < 6 || *n % 2) {
    free(*v);
    return MATH_ERROR_DIMENSION;
  }
  return MATH_SUCCESS;
}
static inline clm_math_status
clm_math_geometry_point_in_polygon(clm_id p, clm_id polygon, bool *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *pt, *v;
  size_t count;
  clm_math_status s = clm_math_read_double(p, 2, &pt, NULL);
  if (s)
    return s;
  s = clm_private_math_geometry_poly(polygon, &v, &count);
  if (s) {
    free(pt);
    return s;
  }
  bool inside = false;
  size_t n = count / 2;
  double x = pt[0], y = pt[1];
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    double x1 = v[2 * j], y1 = v[2 * j + 1], x2 = v[2 * i], y2 = v[2 * i + 1];
    double cross = (x - x1) * (y2 - y1) - (y - y1) * (x2 - x1);
    if (fabs(cross) <= 1e-12 && x >= fmin(x1, x2) && x <= fmax(x1, x2) &&
        y >= fmin(y1, y2) && y <= fmax(y1, y2)) {
      inside = true;
      break;
    }
    if (((y1 > y) != (y2 > y)) && x < (x2 - x1) * (y - y1) / (y2 - y1) + x1)
      inside = !inside;
  }
  free(pt);
  free(v);
  *out = inside;
  return MATH_SUCCESS;
}
static inline clm_math_status clm_math_geometry_polygon_area(clm_id poly_id,
                                                             double *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *v, sum = 0;
  size_t count;
  clm_math_status s = clm_private_math_geometry_poly(poly_id, &v, &count);
  if (s)
    return s;
  size_t n = count / 2;
  for (size_t i = 0; i < n; i++) {
    size_t j = (i + 1) % n;
    sum += v[2 * i] * v[2 * j + 1] - v[2 * j] * v[2 * i + 1];
  }
  free(v);
  *out = fabs(sum) / 2;
  return MATH_SUCCESS;
}

