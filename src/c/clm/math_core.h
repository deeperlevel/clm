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

#include "base.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/** Status returned by CLM math operations. */
typedef enum clm_math_status {
  MATH_SUCCESS = 0,
  MATH_ERROR_STORAGE = -1,
  MATH_ERROR_DIMENSION = -2,
  MATH_ERROR_VALUE = -3,
  MATH_ERROR_DOMAIN = -4,
  MATH_ERROR_SINGULAR = -5,
  MATH_ERROR_ALLOC = -6
} clm_math_status;

/** Backend adapter for math operations over existing numeric storage IDs.
 * Read callbacks copy exactly count values into caller buffers. Write callbacks
 * replace or resize the entire destination and must support aliased input IDs
 * because math operations may read their inputs before writing the result.
 * The adapter and its user context must outlive all math operations using it.
 */
typedef struct clm_math_storage_adapter {
  clm_math_status (*double_count)(void *user, clm_id id, size_t *count);
  clm_math_status (*double_read)(void *user, clm_id id, double *values,
                                 size_t count);
  clm_math_status (*double_write)(void *user, clm_id id, const double *values,
                                  size_t count);
  clm_math_status (*int64_count)(void *user, clm_id id, size_t *count);
  clm_math_status (*int64_read)(void *user, clm_id id, int64_t *values,
                                size_t count);
  clm_math_status (*int64_write)(void *user, clm_id id, const int64_t *values,
                                 size_t count);
  void *user;
} clm_math_storage_adapter;

/** Installs a storage backend, or clears it when adapter is NULL. 
 * @param adapter Input data or buffer; not modified.
 */
void clm_math_set_storage_adapter(const clm_math_storage_adapter *adapter);

/** Reads a double storage ID into a newly allocated array.
 * expected==0 accepts any nonempty element count; otherwise the count must
 * match expected. On success, the caller owns *out and must free it.
 * @param id CLM ID identifying the id.
 * @param expected Input parameter described above.
 * @param out Writable output buffer.
 * @param count Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_read_double(clm_id id, size_t expected, double **out,
                                     size_t *count);

/** Replaces a double storage ID with count values from values. 
 * @param id CLM ID identifying the id.
 * @param values Input data or buffer; not modified.
 * @param count Number of elements or items.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_write_double(clm_id id, const double *values,
                                      size_t count);

/** Reads an int64 storage ID into a newly allocated array.
 * expected==0 accepts any element count, including zero. On success, the
 * caller owns *out and must free it.
 * @param id CLM ID identifying the id.
 * @param expected Input parameter described above.
 * @param out Writable output buffer.
 * @param count Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_read_int64(clm_id id, size_t expected, int64_t **out,
                                    size_t *count);

/** Replaces an int64 storage ID with count values from values. 
 * @param id CLM ID identifying the id.
 * @param values Input data or buffer; not modified.
 * @param count Number of elements or items.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_write_int64(clm_id id, const int64_t *values,
                                     size_t count);

/** Reads a 2D, 3D, or 4D vector from a double storage ID.
 * On success, the caller owns *values and must free it.
 * @param id CLM ID identifying the id.
 * @param dimension Input parameter described above.
 * @param values Writable output or in/out pointer.
 * @param count Writable output or in/out pointer.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_read_vector(clm_id id, size_t dimension,
                                     double **values, size_t *count);

/* Single-header implementation. Define CLM_MATH_CORE_IMPLEMENTATION in exactly
 * one translation unit, or define CLM_IMPLEMENTATION once to build all modules.
 * Do NOT define it in multiple translation units.
 */
