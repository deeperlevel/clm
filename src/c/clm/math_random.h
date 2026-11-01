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
/* Explicit PCG32 state is an int64 Storage of exactly two elements
 * [state,increment]. */
/* Inline implementation — no separate .c file required. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define clm_private_math_random_MULT UINT64_C(6364136223846793005)
/* Encode signed Storage state bits without out-of-range unsigned->signed
 * conversions. */
int64_t clm_private_math_random_as_i64(uint64_t u);
uint64_t clm_private_math_random_as_u64(int64_t s);
uint32_t clm_private_math_random_step(uint64_t *state,
                                                    uint64_t increment);
clm_math_status
clm_private_math_random_load(clm_id id, uint64_t *state, uint64_t *increment);
clm_math_status
clm_private_math_random_save(clm_id id, uint64_t state, uint64_t inc);
/** Seeds an existing two-element int64 storage with a PCG32 seed/sequence. 
 * @param id CLM ID identifying the id.
 * @param seed Input parameter described above.
 * @param seq Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_random_seed(clm_id id, uint64_t seed,
                                                   uint64_t seq);
/** Advances the generator and returns one 32-bit value. 
 * @param id CLM ID identifying the id.
 * @param out Writable output buffer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_random_uint32(clm_id id, uint32_t *out);
/** Returns a uniform double in [0,1), advancing the supplied state. 
 * @param id CLM ID identifying the id.
 * @param out Writable output buffer.
 * @return A uniform double in [0,1), advancing the supplied state.
 */
clm_math_status clm_math_random_float(clm_id id, double *out);
/** Returns an unbiased integer in the inclusive range [low,high]. 
 * @param id CLM ID identifying the id.
 * @param lo Input parameter described above.
 * @param hi Input parameter described above.
 * @param out Writable output buffer.
 * @return An unbiased integer in the inclusive range [low,high].
 */
clm_math_status
clm_math_random_integer(clm_id id, int64_t lo, int64_t hi, int64_t *out);
/** Returns a uniform double in [low,high); low must not exceed high. 
 * @param id CLM ID identifying the id.
 * @param lo Input parameter described above.
 * @param hi Input parameter described above.
 * @param out Writable output buffer.
 * @return A uniform double in [low,high); low must not exceed high.
 */
clm_math_status clm_math_random_uniform(clm_id id, double lo,
                                                      double hi, double *out);
