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
/* Explicit PCG32 state is an int64 Storage of exactly two elements
 * [state,increment]. */
/** Seeds an existing two-element int64 storage with a PCG32 seed/sequence. */
static inline clm_math_status
clm_math_random_seed(clm_id state_storage, uint64_t seed, uint64_t sequence);

/** Advances the generator and returns one 32-bit value. */
static inline clm_math_status clm_math_random_uint32(clm_id state_storage,
                                                     uint32_t *out);

/** Returns a uniform double in [0,1), advancing the supplied state. */
static inline clm_math_status clm_math_random_float(clm_id state_storage,
                                                    double *out);

/** Returns an unbiased integer in the inclusive range [low,high]. */
static inline clm_math_status clm_math_random_integer(clm_id state_storage,
                                                      int64_t low, int64_t high,
                                                      int64_t *out);

/** Returns a uniform double in [low,high); low must not exceed high. */
static inline clm_math_status clm_math_random_uniform(clm_id state_storage,
                                                      double low, double high,
                                                      double *out);

/* Inline implementation — no separate .c file required. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define clm_private_math_random_MULT UINT64_C(6364136223846793005)
/* Encode signed Storage state bits without out-of-range unsigned->signed
 * conversions. */
static inline int64_t clm_private_math_random_as_i64(uint64_t u) {
  int64_t s;
  memcpy(&s, &u, sizeof s);
  return s;
}
static inline uint64_t clm_private_math_random_as_u64(int64_t s) {
  uint64_t u;
  memcpy(&u, &s, sizeof u);
  return u;
}
static inline uint32_t clm_private_math_random_step(uint64_t *state,
                                                    uint64_t increment) {
  uint64_t old = *state;
  *state = old * clm_private_math_random_MULT + increment;
  uint32_t x = (uint32_t)(((old >> 18) ^ old) >> 27);
  uint32_t rot = (uint32_t)(old >> 59);
  return (x >> rot) | (x << ((-rot) & 31));
}
static inline clm_math_status
clm_private_math_random_load(clm_id id, uint64_t *state, uint64_t *increment) {
  int64_t *x;
  clm_math_status s = clm_math_read_int64(id, 2, &x, NULL);
  if (s)
    return s;
  *state = clm_private_math_random_as_u64(x[0]);
  *increment = clm_private_math_random_as_u64(x[1]);
  free(x);
  return MATH_SUCCESS;
}
static inline clm_math_status
clm_private_math_random_save(clm_id id, uint64_t state, uint64_t inc) {
  int64_t x[] = {clm_private_math_random_as_i64(state),
                 clm_private_math_random_as_i64(inc)};
  return clm_math_write_int64(id, x, 2);
}
static inline clm_math_status clm_math_random_seed(clm_id id, uint64_t seed,
                                                   uint64_t seq) {
  uint64_t old, inc;
  clm_math_status s = clm_private_math_random_load(id, &old, &inc);
  if (s)
    return s;
  inc = (seq << 1) | 1;
  uint64_t state = 0;
  clm_private_math_random_step(&state, inc);
  state += seed;
  clm_private_math_random_step(&state, inc);
  return clm_private_math_random_save(id, state, inc);
}
static inline clm_math_status clm_math_random_uint32(clm_id id, uint32_t *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  uint64_t s, inc;
  clm_math_status rc = clm_private_math_random_load(id, &s, &inc);
  if (rc)
    return rc;
  uint32_t v = clm_private_math_random_step(&s, inc);
  rc = clm_private_math_random_save(id, s, inc);
  if (!rc)
    *out = v;
  return rc;
}
static inline clm_math_status clm_math_random_float(clm_id id, double *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  uint32_t v;
  clm_math_status s = clm_math_random_uint32(id, &v);
  if (!s)
    *out = (double)v / 4294967296.0;
  return s;
}
static inline clm_math_status
clm_math_random_integer(clm_id id, int64_t lo, int64_t hi, int64_t *out) {
  if (!out)
    return MATH_ERROR_VALUE;
  if (lo > hi || ((uint64_t)hi - (uint64_t)lo) + 1 == 0)
    return MATH_ERROR_DOMAIN;
  uint64_t bound = (uint64_t)hi - (uint64_t)lo + 1;
  if (bound > UINT64_C(4294967296))
    return MATH_ERROR_DOMAIN;
  uint64_t threshold = UINT64_C(4294967296) % bound;
  uint32_t v;
  clm_math_status s;
  do {
    s = clm_math_random_uint32(id, &v);
    if (s)
      return s;
  } while (v < threshold);
  *out = lo + (int64_t)(v % bound);
  return MATH_SUCCESS;
}
static inline clm_math_status clm_math_random_uniform(clm_id id, double lo,
                                                      double hi, double *out) {
  if (!out || !isfinite(lo) || !isfinite(hi))
    return MATH_ERROR_VALUE;
  if (lo > hi)
    return MATH_ERROR_DOMAIN;
  double v;
  clm_math_status s = clm_math_random_float(id, &v);
  if (!s)
    *out = lo + (hi - lo) * v;
  return s;
}
