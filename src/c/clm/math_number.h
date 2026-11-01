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
/* Inline implementation — no separate .c file required. */
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
uint64_t clm_private_math_number_magnitude(int64_t x);
uint64_t clm_private_math_number_ugcd(uint64_t x, uint64_t y);
/** Computes the nonnegative greatest common divisor. 
 * @param a First input value.
 * @param b Second input value.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_gcd(int64_t a, int64_t b,
                                                  int64_t *out);
/** Computes the nonnegative least common multiple; overflow is an error. 
 * @param a First input value.
 * @param b Second input value.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_lcm(int64_t a, int64_t b,
                                                  int64_t *out);
/* Overflow-safe modular operations on uint64 residues, no compiler extensions.
 */
uint64_t clm_private_math_number_addmod(uint64_t a, uint64_t b,
                                                      uint64_t mod);
uint64_t clm_private_math_number_mulmod(uint64_t a, uint64_t b,
                                                      uint64_t mod);
/** Computes the normalized remainder in [0,m); m must be positive. 
 * @param a First input value.
 * @param m Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_mod(int64_t a, int64_t m,
                                                  int64_t *out);
/** Computes modular addition without signed overflow; m must be positive. 
 * @param a First input value.
 * @param b Second input value.
 * @param m Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_mod_add(int64_t a, int64_t b,
                                                      int64_t m, int64_t *out);
/** Computes modular subtraction with a normalized nonnegative result. 
 * @param a First input value.
 * @param b Second input value.
 * @param m Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_mod_sub(int64_t a, int64_t b,
                                                      int64_t m, int64_t *out);
/** Computes modular multiplication without signed overflow. 
 * @param a First input value.
 * @param b Second input value.
 * @param m Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_mod_mul(int64_t a, int64_t b,
                                                      int64_t m, int64_t *out);
/** Computes a^exponent modulo positive m using exponentiation by squaring. 
 * @param a First input value.
 * @param exp Input parameter described above.
 * @param m Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_mod_pow(int64_t a, uint64_t exp,
                                                      int64_t m, int64_t *out);
/* Signed 64-bit extended Euclid: report unrepresentable coefficients instead
   of performing overflowing signed arithmetic. */
bool clm_private_math_number_sub_i64(int64_t a, int64_t b,
                                                   int64_t *out);
bool clm_private_math_number_mul_i64(int64_t a, int64_t b,
                                                   int64_t *out);
/** Computes g,x,y such that a*x + b*y == g, where g is nonnegative. 
 * @param a First input value.
 * @param b Second input value.
 * @param g Writable output or in/out pointer.
 * @param x Writable output or in/out pointer.
 * @param y Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_extended_gcd(int64_t a, int64_t b,
                                                           int64_t *g,
                                                           int64_t *x,
                                                           int64_t *y);
/** Computes a modular inverse; the values must be coprime and m > 1. 
 * @param a First input value.
 * @param m Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_mod_inverse(int64_t a, int64_t m,
                                                          int64_t *out);
/** Returns whether the greatest common divisor is one. 
 * @param a First input value.
 * @param b Second input value.
 * @return Whether the greatest common divisor is one.
 */
bool clm_math_number_is_coprime(int64_t a, int64_t b);
/** Tests whether value is even. 
 * @param x Input value or coordinate.
 * @return True if value is even; otherwise false.
 */
bool clm_math_number_is_even(int64_t x);
/** Tests whether value is odd. 
 * @param x Input value or coordinate.
 * @return True if value is odd; otherwise false.
 */
bool clm_math_number_is_odd(int64_t x);
/** Tests primality using trial division; this is not cryptographic. 
 * @param n Input parameter described above.
 * @return Whether the condition described above holds.
 */
bool clm_math_number_is_prime(int64_t n);
/** Finds the smallest prime strictly greater than value. 
 * @param n Input parameter described above.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_number_next_prime(int64_t n,
                                                         int64_t *out);
/** Writes repeated prime factors in ascending order to int64 destination. 
 * @param x Input value or coordinate.
 * @param dst Input parameter described above.
 * @param out_count Receives the count.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status
clm_math_number_prime_factors(int64_t x, clm_id dst, size_t *out_count);
