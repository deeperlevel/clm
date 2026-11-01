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

#include "math_collision.h"

clm_math_status clm_private_math_collision_box(clm_id x,
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

clm_math_status clm_private_math_collision_circle(clm_id x,
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

clm_math_status clm_math_collision_aabb_aabb(clm_id x, clm_id y,
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

clm_math_status clm_math_collision_point_aabb(clm_id x, clm_id y,
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

clm_math_status
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

clm_math_status clm_math_collision_circle_aabb(clm_id x, clm_id y,
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

clm_math_status
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

clm_math_status clm_math_collision_ray_aabb(clm_id x, clm_id y,
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
