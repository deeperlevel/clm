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

#include "math_quaternion.h"

clm_math_status clm_math_quaternion_identity(clm_id d) {
  double a[] = {0, 0, 0, 1};
  return clm_math_write_double(d, a, 4);
}

clm_math_status
clm_math_quaternion_set(clm_id d, double x, double y, double z, double w) {
  double a[] = {x, y, z, w};
  return clm_math_write_double(d, a, 4);
}

clm_math_status clm_math_quaternion_normalize(clm_id x,
                                                            clm_id d) {
  double *a, b[4], n = 0;
  clm_math_status s = clm_math_read_double(x, 4, &a, NULL);
  if (s)
    return s;
  for (int i = 0; i < 4; i++)
    n = hypot(n, a[i]);
  if (n == 0) {
    free(a);
    return MATH_ERROR_DOMAIN;
  }
  for (int i = 0; i < 4; i++)
    b[i] = a[i] / n;
  free(a);
  return clm_math_write_double(d, b, 4);
}

clm_math_status clm_math_quaternion_conjugate(clm_id x,
                                                            clm_id d) {
  double *a, b[4];
  clm_math_status s = clm_math_read_double(x, 4, &a, NULL);
  if (s)
    return s;
  for (int i = 0; i < 4; i++)
    b[i] = i == 3 ? a[i] : -a[i];
  free(a);
  return clm_math_write_double(d, b, 4);
}

clm_math_status clm_math_quaternion_multiply(clm_id x, clm_id y,
                                                           clm_id d) {
  double *a, *b;
  clm_math_status s = clm_math_read_double(x, 4, &a, NULL);
  if (s)
    return s;
  s = clm_math_read_double(y, 4, &b, NULL);
  if (s) {
    free(a);
    return s;
  }
  double X = a[0], Y = a[1], Z = a[2], W = a[3], x2 = b[0], y2 = b[1],
         z2 = b[2], w2 = b[3];
  double v[] = {
      W * x2 + X * w2 + Y * z2 - Z * y2, W * y2 - X * z2 + Y * w2 + Z * x2,
      W * z2 + X * y2 - Y * x2 + Z * w2, W * w2 - X * x2 - Y * y2 - Z * z2};
  free(a);
  free(b);
  return clm_math_write_double(d, v, 4);
}

clm_math_status
clm_math_quaternion_from_axis_angle(clm_id axis, double angle, clm_id d) {
  if (!isfinite(angle))
    return MATH_ERROR_VALUE;
  double *a, n = 0;
  clm_math_status s = clm_math_read_double(axis, 3, &a, NULL);
  if (s)
    return s;
  for (int i = 0; i < 3; i++)
    n = hypot(n, a[i]);
  if (n == 0) {
    free(a);
    return MATH_ERROR_DOMAIN;
  }
  double v[] = {a[0] * sin(angle / 2) / n, a[1] * sin(angle / 2) / n,
                a[2] * sin(angle / 2) / n, cos(angle / 2)};
  free(a);
  return clm_math_write_double(d, v, 4);
}

clm_math_status
clm_math_quaternion_rotate_vector(clm_id q, clm_id x, clm_id d) {
  double *a, *v;
  clm_math_status s = clm_math_read_double(q, 4, &a, NULL);
  if (s)
    return s;
  s = clm_math_read_double(x, 3, &v, NULL);
  if (s) {
    free(a);
    return s;
  }
  double u2 = 0, dot = 0, norm = 0;
  for (int i = 0; i < 3; i++) {
    u2 += a[i] * a[i];
    dot += a[i] * v[i];
  }
  for (int i = 0; i < 4; i++)
    norm += a[i] * a[i];
  if (norm == 0) {
    free(a);
    free(v);
    return MATH_ERROR_DOMAIN;
  }
  double cross[] = {a[1] * v[2] - a[2] * v[1], a[2] * v[0] - a[0] * v[2],
                    a[0] * v[1] - a[1] * v[0]};
  double o[3];
  for (int i = 0; i < 3; i++)
    o[i] = ((a[3] * a[3] - u2) * v[i] + 2 * dot * a[i] + 2 * a[3] * cross[i]) /
           norm;
  free(a);
  free(v);
  return clm_math_write_double(d, o, 3);
}

clm_math_status clm_math_quaternion_slerp(clm_id x, clm_id y,
                                                        double t, clm_id d) {
  if (!isfinite(t))
    return MATH_ERROR_VALUE;
  double *a, *b, na = 0, nb = 0, dot = 0, out[4];
  clm_math_status s = clm_math_read_double(x, 4, &a, NULL);
  if (s)
    return s;
  s = clm_math_read_double(y, 4, &b, NULL);
  if (s) {
    free(a);
    return s;
  }
  for (int i = 0; i < 4; i++) {
    na = hypot(na, a[i]);
    nb = hypot(nb, b[i]);
  }
  if (na == 0 || nb == 0) {
    free(a);
    free(b);
    return MATH_ERROR_DOMAIN;
  }
  for (int i = 0; i < 4; i++) {
    a[i] /= na;
    b[i] /= nb;
    dot += a[i] * b[i];
  }
  if (dot < 0) {
    dot = -dot;
    for (int i = 0; i < 4; i++)
      b[i] = -b[i];
  }
  if (dot > 1)
    dot = 1;
  if (dot > .9995) {
    double mag = 0;
    for (int i = 0; i < 4; i++) {
      out[i] = a[i] + t * (b[i] - a[i]);
      mag = hypot(mag, out[i]);
    }
    for (int i = 0; i < 4; i++)
      out[i] /= mag;
  } else {
    double theta = acos(dot), st = sin(theta), p = sin((1 - t) * theta) / st,
           q = sin(t * theta) / st;
    for (int i = 0; i < 4; i++)
      out[i] = p * a[i] + q * b[i];
  }
  free(a);
  free(b);
  return clm_math_write_double(d, out, 4);
}
