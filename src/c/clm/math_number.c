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

#include "math_number.h"

uint64_t clm_private_math_number_magnitude(int64_t x) {
  return x < 0 ? (uint64_t)(-(x + 1)) + 1u : (uint64_t)x;
}

uint64_t clm_private_math_number_ugcd(uint64_t x, uint64_t y) {
  while (y) {
    uint64_t t = x % y;
    x = y;
    y = t;
  }
  return x;
}

clm_math_status clm_math_number_gcd(int64_t a, int64_t b,
                                                  int64_t *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  uint64_t g =
      clm_private_math_number_ugcd(clm_private_math_number_magnitude(a),
                                   clm_private_math_number_magnitude(b));
  if (g > INT64_MAX)
    return MATH_ERROR_DOMAIN;
  *out = (int64_t)g;
  return MATH_SUCCESS;
}

clm_math_status clm_math_number_lcm(int64_t a, int64_t b,
                                                  int64_t *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  uint64_t x = clm_private_math_number_magnitude(a),
           y = clm_private_math_number_magnitude(b);
  if (!x || !y) {
    *out = 0;
    return MATH_SUCCESS;
  }
  uint64_t g = clm_private_math_number_ugcd(x, y);
  x /= g;
  if (x > INT64_MAX / y)
    return MATH_ERROR_DOMAIN;
  *out = (int64_t)(x * y);
  return MATH_SUCCESS;
}

uint64_t clm_private_math_number_addmod(uint64_t a, uint64_t b,
                                                      uint64_t mod) {
  return a >= mod - b ? a - (mod - b) : a + b;
}

uint64_t clm_private_math_number_mulmod(uint64_t a, uint64_t b,
                                                      uint64_t mod) {
  uint64_t out = 0;
  while (b) {
    if (b & 1)
      out = clm_private_math_number_addmod(out, a, mod);
    b >>= 1;
    if (b)
      a = clm_private_math_number_addmod(a, a, mod);
  }
  return out;
}

clm_math_status clm_math_number_mod(int64_t a, int64_t m,
                                                  int64_t *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  if (m <= 0)
    return MATH_ERROR_DOMAIN;
  int64_t r = a % m;
  if (r < 0)
    r += m;
  *out = r;
  return MATH_SUCCESS;
}

clm_math_status clm_math_number_mod_add(int64_t a, int64_t b,
                                                      int64_t m, int64_t *out) {
  int64_t x, y;
  clm_math_status s = clm_math_number_mod(a, m, &x);
  if (s)
    return s;
  s = clm_math_number_mod(b, m, &y);
  if (s)
    return s;
  if (!out)
    return MATH_ERROR_VALUE;
  *out = (int64_t)clm_private_math_number_addmod((uint64_t)x, (uint64_t)y,
                                                 (uint64_t)m);
  return MATH_SUCCESS;
}

clm_math_status clm_math_number_mod_sub(int64_t a, int64_t b,
                                                      int64_t m, int64_t *out) {
  int64_t x, y;
  clm_math_status s = clm_math_number_mod(a, m, &x);
  if (s)
    return s;
  s = clm_math_number_mod(b, m, &y);
  if (s)
    return s;
  if (!out)
    return MATH_ERROR_VALUE;
  *out = x >= y ? x - y : m - (y - x);
  return MATH_SUCCESS;
}

clm_math_status clm_math_number_mod_mul(int64_t a, int64_t b,
                                                      int64_t m, int64_t *out) {
  int64_t x, y;
  clm_math_status s = clm_math_number_mod(a, m, &x);
  if (s)
    return s;
  s = clm_math_number_mod(b, m, &y);
  if (s)
    return s;
  if (!out)
    return MATH_ERROR_VALUE;
  *out = (int64_t)clm_private_math_number_mulmod((uint64_t)x, (uint64_t)y,
                                                 (uint64_t)m);
  return MATH_SUCCESS;
}

clm_math_status clm_math_number_mod_pow(int64_t a, uint64_t exp,
                                                      int64_t m, int64_t *out) {
  int64_t residue;
  clm_math_status s = clm_math_number_mod(a, m, &residue);
  if (s)
    return s;
  if (!out)
    return MATH_ERROR_VALUE;
  uint64_t r = 1 % (uint64_t)m, x = (uint64_t)residue;
  while (exp) {
    if (exp & 1)
      r = clm_private_math_number_mulmod(r, x, (uint64_t)m);
    exp >>= 1;
    if (exp)
      x = clm_private_math_number_mulmod(x, x, (uint64_t)m);
  }
  *out = (int64_t)r;
  return MATH_SUCCESS;
}

bool clm_private_math_number_sub_i64(int64_t a, int64_t b,
                                                   int64_t *out) {
  if ((b > 0 && a < INT64_MIN + b) || (b < 0 && a > INT64_MAX + b))
    return false;
  *out = a - b;
  return true;
}

bool clm_private_math_number_mul_i64(int64_t a, int64_t b,
                                                   int64_t *out) {
  if (!a || !b) {
    *out = 0;
    return true;
  }
  bool negative = (a < 0) != (b < 0);
  uint64_t aa = clm_private_math_number_magnitude(a),
           bb = clm_private_math_number_magnitude(b);
  uint64_t limit = negative ? (uint64_t)INT64_MAX + 1u : (uint64_t)INT64_MAX;
  if (aa > limit / bb)
    return false;
  uint64_t mag = aa * bb;
  *out = negative
             ? (mag == (uint64_t)INT64_MAX + 1u ? INT64_MIN : -(int64_t)mag)
             : (int64_t)mag;
  return true;
}

clm_math_status clm_math_number_extended_gcd(int64_t a, int64_t b,
                                                           int64_t *g,
                                                           int64_t *x,
                                                           int64_t *y) {
  if (!g || !x || !y)
    return MATH_ERROR_VALUE;
  if (a == INT64_MIN || b == INT64_MIN)
    return MATH_ERROR_DOMAIN;
  int64_t r0 = a, r1 = b, s0 = 1, s1 = 0, t0 = 0, t1 = 1;
  while (r1) {
    int64_t q = r0 / r1, qr, qs, qt, rr, ss, tt;
    if (!clm_private_math_number_mul_i64(q, r1, &qr) ||
        !clm_private_math_number_sub_i64(r0, qr, &rr) ||
        !clm_private_math_number_mul_i64(q, s1, &qs) ||
        !clm_private_math_number_sub_i64(s0, qs, &ss) ||
        !clm_private_math_number_mul_i64(q, t1, &qt) ||
        !clm_private_math_number_sub_i64(t0, qt, &tt))
      return MATH_ERROR_DOMAIN;
    r0 = r1;
    r1 = rr;
    s0 = s1;
    s1 = ss;
    t0 = t1;
    t1 = tt;
  }
  if (r0 < 0) {
    r0 = -r0;
    s0 = -s0;
    t0 = -t0;
  }
  *g = r0;
  *x = s0;
  *y = t0;
  return MATH_SUCCESS;
}

clm_math_status clm_math_number_mod_inverse(int64_t a, int64_t m,
                                                          int64_t *out) {
  if (m <= 1)
    return MATH_ERROR_DOMAIN;
  int64_t g, x, y;
  clm_math_status s = clm_math_number_extended_gcd(a, m, &g, &x, &y);
  if (s)
    return s;
  if (g != 1)
    return MATH_ERROR_DOMAIN;
  return clm_math_number_mod(x, m, out);
}

bool clm_math_number_is_coprime(int64_t a, int64_t b) {
  return clm_private_math_number_ugcd(clm_private_math_number_magnitude(a),
                                      clm_private_math_number_magnitude(b)) ==
         1;
}

bool clm_math_number_is_even(int64_t x) { return x % 2 == 0; }

bool clm_math_number_is_odd(int64_t x) { return x % 2 != 0; }

bool clm_math_number_is_prime(int64_t n) {
  if (n < 2)
    return false;
  if (n % 2 == 0)
    return n == 2;
  for (int64_t i = 3; i <= n / i; i += 2)
    if (n % i == 0)
      return false;
  return true;
}

clm_math_status clm_math_number_next_prime(int64_t n,
                                                         int64_t *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  if (n >= INT64_MAX - 1)
    return MATH_ERROR_DOMAIN;
  int64_t x = n < 2 ? 2 : n + 1;
  while (!clm_math_number_is_prime(x)) {
    if (x >= INT64_MAX - 2)
      return MATH_ERROR_DOMAIN;
    x++;
  }
  *out = x;
  return MATH_SUCCESS;
}

clm_math_status
clm_math_number_prime_factors(int64_t x, clm_id dst, size_t *out_count) {
  if (x < 2)
    return MATH_ERROR_DOMAIN;
  int64_t n = x, a[64];
  size_t size = 0;
  for (int64_t d = 2; d <= n / d; d = d == 2 ? 3 : d + 2) {
    while (n % d == 0) {
      a[size++] = d;
      n /= d;
    }
  }
  if (n > 1)
    a[size++] = n;
  clm_math_status s = clm_math_write_int64(dst, a, size);
  if (!s && out_count)
    *out_count = size;
  return s;
}
