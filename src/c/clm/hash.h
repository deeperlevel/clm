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
#include "bpa.h"
#include "string.h"
#include "ext/freehash.h"

/** Hash algorithm identifiers; digest values are raw bytes, not text. */
#define CLM_HASH_ALGORITHM_MD2 1
#define CLM_HASH_ALGORITHM_MD4 2
#define CLM_HASH_ALGORITHM_MD5 3
#define CLM_HASH_ALGORITHM_SHA1 4
#define CLM_HASH_ALGORITHM_SHA224 5
#define CLM_HASH_ALGORITHM_SHA256 6
#define CLM_HASH_ALGORITHM_SHA384 7
#define CLM_HASH_ALGORITHM_SHA512 8
#define CLM_HASH_ALGORITHM_SHA512_224 9
#define CLM_HASH_ALGORITHM_SHA512_256 10
#define CLM_HASH_ALGORITHM_SHA3_224 11
#define CLM_HASH_ALGORITHM_SHA3_256 12
#define CLM_HASH_ALGORITHM_SHA3_384 13
#define CLM_HASH_ALGORITHM_SHA3_512 14

#define CLM_HASH_SUCCESS 0
#define CLM_HASH_ERROR_INVALID_ARGUMENT -1
#define CLM_HASH_ERROR_UNSUPPORTED_ALGORITHM -2
#define CLM_HASH_ERROR_BUFFER_TOO_SMALL -3
#define CLM_HASH_ERROR_BACKEND -4
#define CLM_HASH_ERROR_NO_MEMORY -5

#define CLM_HASH_MAX_DIGEST_SIZE 64u

typedef int clm_hash_algorithm;

const struct lfh_hash_descriptor* clm_hash_private_backend_descriptor(
    const clm_hash_algorithm algorithm);

clm_int clm_hash_private_backend_status(const int status);

/** Returns the algorithm's binary digest size.
 * @param algorithm One of the CLM_HASH_ALGORITHM_* constants.
 * @return Digest size in bytes, or zero for an unsupported identifier.
 */
size_t clm_hash_digest_size(clm_hash_algorithm algorithm);

/** Returns the character-buffer capacity required for a lowercase hex digest.
 * @param algorithm One of the CLM_HASH_ALGORITHM_* constants.
 * @return Two characters per digest byte plus the NUL terminator, or zero
 *         for an unsupported identifier.
 */
size_t clm_hash_hex_size(clm_hash_algorithm algorithm);

/** Stores the algorithm's display name in an existing CLM string.
 * @param algorithm One of the CLM_HASH_* constants.
 * @param dest ID of an existing CLM string to receive the name.
 * @return CLM_HASH_SUCCESS, CLM_HASH_ERROR_UNSUPPORTED_ALGORITHM, or
 *         CLM_HASH_ERROR_INVALID_ARGUMENT.
 */
clm_int clm_hash_algorithm_name(clm_hash_algorithm algorithm, clm_id dest);

/**
 * Encodes arbitrary bytes as lowercase hexadecimal text.
 *
 * @param bytes Borrowed binary input; NULL is accepted only when length is zero.
 * @param length Number of input bytes.
 * @param out_hex Caller-owned destination receiving text and NUL terminator.
 * @param capacity Destination capacity in bytes, including NUL.
 * @return SUCCESS, INVALID_ARGUMENT, or BUFFER_TOO_SMALL.
 * @note Source and destination must not overlap. Failure leaves output intact.
 */
clm_int clm_hash_hex_encode(
    const void* bytes,
    size_t length,
    char* out_hex,
    size_t capacity);

/** Hashes a bounded byte sequence into a caller-owned binary digest buffer.
 * @param algorithm Selected hash algorithm.
 * @param bytes Borrowed input; NULL is valid only when length is zero.
 * @param length Number of input bytes.
 * @param out_digest Receives the binary digest.
 * @param capacity Available output bytes; must fit the selected digest.
 * @return CLM_HASH_SUCCESS or a CLM_HASH_ERROR_* result. Output is unchanged
 *         if the backend reports an error.
 */
clm_int clm_hash_bytes(
    const clm_hash_algorithm algorithm,
    const void* bytes,
    const size_t length,
    void* out_digest,
    const size_t capacity);

/** Hashes a NUL-terminated C string as raw bytes, excluding its terminator.
 * This variant does not validate text encoding.
 * @param algorithm Selected hash algorithm.
 * @param text Borrowed NUL-terminated input string.
 * @param out_digest Receives the binary digest.
 * @param capacity Available output bytes.
 * @return CLM_HASH_SUCCESS or a CLM_HASH_ERROR_* result.
 */
clm_int clm_hash_bytes_as(
    const clm_hash_algorithm algorithm,
    const char* text,
    void* out_digest,
    const size_t capacity);

/** Hashes the UTF-8 bytes held by an existing CLM string into a binary digest.
 * @param algorithm Selected hash algorithm.
 * @param source_id ID of an existing CLM string.
 * @param out_digest Receives the binary digest.
 * @param capacity Available output bytes.
 * @return CLM_HASH_SUCCESS or a CLM_HASH_ERROR_* result.
 */
clm_int clm_hash_bytes_from(
    const clm_hash_algorithm algorithm,
    const clm_id source_id,
    void* out_digest,
    const size_t capacity);

/** Internal adapter that stores a successful digest as lowercase hex text. 
 * @param algorithm Input parameter described above.
 * @param bytes Input data or buffer; not modified.
 * @param length Length in bytes or elements, as specified above.
 * @param destination_id CLM ID identifying the destination.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_hash_write_string(
    const clm_hash_algorithm algorithm,
    const void* bytes,
    const size_t length,
    const clm_id destination_id);

/** Hashes arbitrary bytes and stores the lowercase hex digest in a CLM string.
 * @param algorithm Selected hash algorithm.
 * @param bytes Borrowed input; NULL is valid only when length is zero.
 * @param length Number of input bytes.
 * @param destination_id ID of an existing CLM string to replace with the hex digest.
 * @return CLM_HASH_SUCCESS or a CLM_HASH_ERROR_* result.
 */
clm_int clm_hash_bytes_to(
    const clm_hash_algorithm algorithm,
    const void* bytes,
    const size_t length,
    const clm_id destination_id);

/** Hashes a NUL-terminated UTF-8 C string into an existing CLM string.
 * The destination receives lowercase hexadecimal text.
 * @param algorithm Selected hash algorithm.
 * @param text Borrowed NUL-terminated UTF-8 input.
 * @param destination_id ID of an existing CLM string to receive the hex digest.
 * @return CLM_HASH_SUCCESS or a CLM_HASH_ERROR_* result.
 */
clm_int clm_hash_string_to(
    const clm_hash_algorithm algorithm,
    const char* text,
    const clm_id destination_id);

/** Hashes one CLM string and stores lowercase hexadecimal output in another.
 * @param algorithm Selected hash algorithm.
 * @param source_id ID of the CLM string to hash.
 * @param destination_id ID of an existing CLM string to receive the digest.
 * @return CLM_HASH_SUCCESS or a CLM_HASH_ERROR_* result.
 */
clm_int clm_hash_string_from(
    const clm_hash_algorithm algorithm,
    const clm_id source_id,
    const clm_id destination_id);

/** Hashes a NUL-terminated UTF-8 C string into a caller-owned lowercase hex buffer.
 * @param algorithm Selected hash algorithm.
 * @param text Borrowed NUL-terminated UTF-8 input.
 * @param out_hex Receives two lowercase hex characters per digest byte plus NUL.
 * @param capacity Available output bytes, including NUL.
 * @return CLM_HASH_SUCCESS or a CLM_HASH_ERROR_* result.
 */
clm_int clm_hash_string_as(
    const clm_hash_algorithm algorithm,
    const char* text,
    char* out_hex,
    const size_t capacity);
