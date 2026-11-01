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

#include "math_matrix.h"

clm_math_status clm_private_math_matrix_shape(size_t r,
                                                            size_t c) {
  return !r || !c || r > SIZE_MAX / c || r * c > SIZE_MAX / sizeof(double)
             ? MATH_ERROR_DIMENSION
             : MATH_SUCCESS;
}

clm_math_status clm_math_matrix_identity(clm_id dst, size_t n) {
  if (clm_private_math_matrix_shape(n, n))
    return MATH_ERROR_DIMENSION;
  double *a = calloc(n * n, sizeof *a);
  if (!a)
    return MATH_ERROR_ALLOC;
  for (size_t i = 0; i < n; i++)
    a[i * n + i] = 1;
  clm_math_status s = clm_math_write_double(dst, a, n * n);
  free(a);
  return s;
}

clm_math_status clm_math_matrix_multiply(clm_id x, size_t ra,
                                                       size_t ca, clm_id y,
                                                       size_t rb, size_t cb,
                                                       clm_id dst) {
  if (clm_private_math_matrix_shape(ra, ca) ||
      clm_private_math_matrix_shape(rb, cb) ||
      clm_private_math_matrix_shape(ra, cb) || ca != rb)
    return MATH_ERROR_DIMENSION;
  double *a, *b, *o;
  clm_math_status s = clm_math_read_double(x, ra * ca, &a, NULL);
  if (s)
    return s;
  s = clm_math_read_double(y, rb * cb, &b, NULL);
  if (s) {
    free(a);
    return s;
  }
  o = calloc(ra * cb, sizeof *o);
  if (!o) {
    free(a);
    free(b);
    return MATH_ERROR_ALLOC;
  }
  for (size_t r = 0; r < ra; r++)
    for (size_t c = 0; c < cb; c++)
      for (size_t k = 0; k < ca; k++)
        o[r * cb + c] += a[r * ca + k] * b[k * cb + c];
  s = clm_math_write_double(dst, o, ra * cb);
  free(a);
  free(b);
  free(o);
  return s;
}

clm_math_status clm_math_matrix_transpose(clm_id x, size_t r,
                                                        size_t c, clm_id dst) {
  if (clm_private_math_matrix_shape(r, c))
    return MATH_ERROR_DIMENSION;
  double *a, *o;
  clm_math_status s = clm_math_read_double(x, r * c, &a, NULL);
  if (s)
    return s;
  o = malloc(r * c * sizeof *o);
  if (!o) {
    free(a);
    return MATH_ERROR_ALLOC;
  }
  for (size_t i = 0; i < r; i++)
    for (size_t j = 0; j < c; j++)
      o[j * r + i] = a[i * c + j];
  s = clm_math_write_double(dst, o, r * c);
  free(a);
  free(o);
  return s;
}

clm_math_status
clm_math_matrix_transform_vector(clm_id x, size_t r, size_t c, clm_id y,
                                 clm_id dst) {
  if (clm_private_math_matrix_shape(r, c))
    return MATH_ERROR_DIMENSION;
  double *a, *b, *o;
  clm_math_status s = clm_math_read_double(x, r * c, &a, NULL);
  if (s)
    return s;
  s = clm_math_read_double(y, c, &b, NULL);
  if (s) {
    free(a);
    return s;
  }
  o = calloc(r, sizeof *o);
  if (!o) {
    free(a);
    free(b);
    return MATH_ERROR_ALLOC;
  }
  for (size_t i = 0; i < r; i++)
    for (size_t j = 0; j < c; j++)
      o[i] += a[i * c + j] * b[j];
  s = clm_math_write_double(dst, o, r);
  free(a);
  free(b);
  free(o);
  return s;
}

clm_math_status clm_private_math_matrix_elimination(double *a,
                                                                  size_t n,
                                                                  double *det,
                                                                  double *inv) {
  if (inv && n > SIZE_MAX / (2 * n * sizeof(double)))
    return MATH_ERROR_DIMENSION;
  size_t width = inv ? 2 * n : n;
  double *v = calloc(n * width, sizeof *v);
  if (!v)
    return MATH_ERROR_ALLOC;
  for (size_t r = 0; r < n; r++) {
    for (size_t c = 0; c < n; c++)
      v[r * width + c] = a[r * n + c];
    if (inv)
      v[r * width + n + r] = 1;
  }
  double d = 1;
  for (size_t col = 0; col < n; col++) {
    size_t pivot = col;
    for (size_t r = col + 1; r < n; r++)
      if (fabs(v[r * width + col]) > fabs(v[pivot * width + col]))
        pivot = r;
    if (v[pivot * width + col] == 0) {
      free(v);
      if (det) {
        *det = 0;
        return MATH_SUCCESS;
      }
      return MATH_ERROR_SINGULAR;
    }
    if (pivot != col) {
      for (size_t j = 0; j < width; j++) {
        double t = v[col * width + j];
        v[col * width + j] = v[pivot * width + j];
        v[pivot * width + j] = t;
      }
      d = -d;
    }
    double p = v[col * width + col];
    d *= p;
    if (inv) {
      for (size_t j = 0; j < width; j++)
        v[col * width + j] /= p;
      p = 1;
    }
    for (size_t r = inv ? 0 : col + 1; r < n; r++) {
      if (r == col)
        continue;
      double f = v[r * width + col] / p;
      for (size_t j = col; j < width; j++)
        v[r * width + j] -= f * v[col * width + j];
    }
  }
  if (det)
    *det = d;
  if (inv) {
    for (size_t r = 0; r < n; r++)
      for (size_t c = 0; c < n; c++)
        inv[r * n + c] = v[r * width + n + c];
  }
  free(v);
  return MATH_SUCCESS;
}

clm_math_status clm_math_matrix_determinant(clm_id x, size_t n,
                                                          double *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  if (clm_private_math_matrix_shape(n, n))
    return MATH_ERROR_DIMENSION;
  double *a;
  clm_math_status s = clm_math_read_double(x, n * n, &a, NULL);
  if (s)
    return s;
  double v = 0;
  s = clm_private_math_matrix_elimination(a, n, &v, NULL);
  free(a);
  if (!s)
    *out = v;
  return s;
}

clm_math_status clm_math_matrix_inverse(clm_id x, size_t n,
                                                      clm_id dst) {
  if (clm_private_math_matrix_shape(n, n) ||
      n > SIZE_MAX / (2 * sizeof(double)))
    return MATH_ERROR_DIMENSION;
  double *a, *inv;
  clm_math_status s = clm_math_read_double(x, n * n, &a, NULL);
  if (s)
    return s;
  inv = malloc(n * n * sizeof *inv);
  if (!inv) {
    free(a);
    return MATH_ERROR_ALLOC;
  }
  s = clm_private_math_matrix_elimination(a, n, NULL, inv);
  if (!s)
    s = clm_math_write_double(dst, inv, n * n);
  free(a);
  free(inv);
  return s;
}

clm_math_status clm_math_matrix_rotation2d(double t, clm_id dst) {
  double a[] = {cos(t), -sin(t), sin(t), cos(t)};
  return clm_math_write_double(dst, a, 4);
}

clm_math_status clm_math_matrix_translation2d(double x, double y,
                                                            clm_id dst) {
  double a[] = {1, 0, x, 0, 1, y, 0, 0, 1};
  return clm_math_write_double(dst, a, 9);
}

clm_math_status clm_math_matrix_scale2d(double x, double y,
                                                      clm_id dst) {
  double a[] = {x, 0, 0, 0, y, 0, 0, 0, 1};
  return clm_math_write_double(dst, a, 9);
}
