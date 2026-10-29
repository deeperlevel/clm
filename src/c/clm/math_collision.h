#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
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

#include "math_core.h"
/* AABB [min_x,min_y,max_x,max_y]; circle [cx,cy,r]. Touching counts as
 * collision. */
/** Tests overlap of two axis-aligned bounding boxes. */
static inline clm_math_status
clm_math_collision_aabb_aabb(clm_id first, clm_id second, bool *out);

/** Tests whether a point lies within or on an axis-aligned bounding box. */
static inline clm_math_status
clm_math_collision_point_aabb(clm_id point, clm_id box, bool *out);

/** Tests overlap or touching of two circles. */
static inline clm_math_status
clm_math_collision_circle_circle(clm_id first, clm_id second, bool *out);

/** Tests overlap or touching between a circle and an axis-aligned box. */
static inline clm_math_status
clm_math_collision_circle_aabb(clm_id circle, clm_id box, bool *out);

/** Tests whether a point lies within or on a circle. */
static inline clm_math_status
clm_math_collision_point_circle(clm_id point, clm_id circle, bool *out);

/** Ray/AABB intersection for a ray origin and direction in 2D.
 * A miss returns MATH_SUCCESS with *hit=false; a hit distance is nonnegative.
 * A zero direction vector is a domain error.
 */
static inline clm_math_status clm_math_collision_ray_aabb(clm_id origin,
                                                          clm_id direction,
                                                          clm_id box, bool *hit,
                                                          double *distance);

/* Inline implementation — no separate .c file required. */
#include <math.h>
#include <stdlib.h>
static inline clm_math_status clm_private_math_collision_box(clm_id x,
                                                             double **out) {
  clm_math_status s = clm_math_read_double(x, 4, out, NULL);
  if (s)
    return s;
  if ((*out)[0] > (*out)[2] || (*out)[1] > (*out)[3]) {
    free(*out);
    return MATH_ERROR_DOMAIN;
  }
  return MATH_SUCCESS;
}
static inline clm_math_status clm_private_math_collision_circle(clm_id x,
                                                                double **out) {
  clm_math_status s = clm_math_read_double(x, 3, out, NULL);
  if (s)
    return s;
  if ((*out)[2] < 0) {
    free(*out);
    return MATH_ERROR_DOMAIN;
  }
  return MATH_SUCCESS;
}
static inline clm_math_status clm_math_collision_aabb_aabb(clm_id x, clm_id y,
                                                           bool *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *a, *b;
  clm_math_status s = clm_private_math_collision_box(x, &a);
  if (s)
    return s;
  s = clm_private_math_collision_box(y, &b);
  if (s) {
    free(a);
    return s;
  }
  *out = !(a[2] < b[0] || b[2] < a[0] || a[3] < b[1] || b[3] < a[1]);
  free(a);
  free(b);
  return MATH_SUCCESS;
}
static inline clm_math_status clm_math_collision_point_aabb(clm_id x, clm_id y,
                                                            bool *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *p, *b;
  clm_math_status s = clm_math_read_double(x, 2, &p, NULL);
  if (s)
    return s;
  s = clm_private_math_collision_box(y, &b);
  if (s) {
    free(p);
    return s;
  }
  *out = p[0] >= b[0] && p[0] <= b[2] && p[1] >= b[1] && p[1] <= b[3];
  free(p);
  free(b);
  return MATH_SUCCESS;
}
static inline clm_math_status
clm_math_collision_circle_circle(clm_id x, clm_id y, bool *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *a, *b;
  clm_math_status s = clm_private_math_collision_circle(x, &a);
  if (s)
    return s;
  s = clm_private_math_collision_circle(y, &b);
  if (s) {
    free(a);
    return s;
  }
  *out = hypot(a[0] - b[0], a[1] - b[1]) <= a[2] + b[2];
  free(a);
  free(b);
  return MATH_SUCCESS;
}
static inline clm_math_status clm_math_collision_circle_aabb(clm_id x, clm_id y,
                                                             bool *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *c, *b;
  clm_math_status s = clm_private_math_collision_circle(x, &c);
  if (s)
    return s;
  s = clm_private_math_collision_box(y, &b);
  if (s) {
    free(c);
    return s;
  }
  double u = fmax(b[0], fmin(c[0], b[2])), v = fmax(b[1], fmin(c[1], b[3]));
  *out = (u - c[0]) * (u - c[0]) + (v - c[1]) * (v - c[1]) <= c[2] * c[2];
  free(c);
  free(b);
  return MATH_SUCCESS;
}
static inline clm_math_status
clm_math_collision_point_circle(clm_id x, clm_id y, bool *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  double *p, *c;
  clm_math_status s = clm_math_read_double(x, 2, &p, NULL);
  if (s)
    return s;
  s = clm_private_math_collision_circle(y, &c);
  if (s) {
    free(p);
    return s;
  }
  *out = hypot(p[0] - c[0], p[1] - c[1]) <= c[2];
  free(p);
  free(c);
  return MATH_SUCCESS;
}
static inline clm_math_status clm_math_collision_ray_aabb(clm_id x, clm_id y,
                                                          clm_id z, bool *hit,
                                                          double *distance) {
  if (!hit || !distance)
    return MATH_ERROR_VALUE;
  double *o, *d, *b;
  clm_math_status s = clm_math_read_double(x, 2, &o, NULL);
  if (s)
    return s;
  s = clm_math_read_double(y, 2, &d, NULL);
  if (s) {
    free(o);
    return s;
  }
  s = clm_private_math_collision_box(z, &b);
  if (s) {
    free(o);
    free(d);
    return s;
  }
  if (d[0] == 0 && d[1] == 0) {
    free(o);
    free(d);
    free(b);
    return MATH_ERROR_DOMAIN;
  }
  double lo = -INFINITY, hi = INFINITY;
  bool ok = true;
  for (int i = 0; i < 2; i++) {
    if (d[i] == 0) {
      if (o[i] < b[i] || o[i] > b[i + 2]) {
        ok = false;
        break;
      }
    } else {
      double a = (b[i] - o[i]) / d[i], q = (b[i + 2] - o[i]) / d[i];
      lo = fmax(lo, fmin(a, q));
      hi = fmin(hi, fmax(a, q));
    }
  }
  double t = fmax(0, lo);
  *hit = ok && hi >= t;
  if (*hit)
    *distance = t;
  free(o);
  free(d);
  free(b);
  return MATH_SUCCESS;
}
