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

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"
#include "bpa.h"
#include "string.h"
#include "ext/monocypher.h"

#if defined(_WIN32)
# include <windows.h>
# include <bcrypt.h>
# if defined(_MSC_VER)
#  pragma comment(lib, "bcrypt.lib")
# endif
#endif

#define CLM_CRYPTO_ALGORITHM_XCHACHA20_POLY1305 1
#define CLM_CRYPTO_KEY_SIZE 32u
#define CLM_CRYPTO_NONCE_SIZE 24u
#define CLM_CRYPTO_TAG_SIZE 16u
#define CLM_CRYPTO_X25519_KEY_SIZE 32u
#define CLM_CRYPTO_EDDSA_PUBLIC_KEY_SIZE 32u
#define CLM_CRYPTO_EDDSA_SECRET_KEY_SIZE 64u
#define CLM_CRYPTO_EDDSA_SIGNATURE_SIZE 64u

#define CLM_CRYPTO_SUCCESS 0
#define CLM_CRYPTO_ERROR_INVALID_ARGUMENT (-1)
#define CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM (-2)
#define CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL (-3)
#define CLM_CRYPTO_ERROR_BACKEND (-4)
#define CLM_CRYPTO_ERROR_AUTHENTICATION (-5)
#define CLM_CRYPTO_ERROR_NO_MEMORY (-6)
#define CLM_CRYPTO_ERROR_INVALID_TEXT (-7)

/** CLM-level cipher identifier, distinct from backend implementation IDs. */
typedef int clm_crypto_algorithm;

/** Returns the 32-byte key length for supported ciphers, or zero if unknown.
 * @param algorithm CLM_CRYPTO_ALGORITHM_* identifier.
 * @return Required number of key bytes, or zero for unsupported IDs.
 */
size_t clm_crypto_key_size(clm_crypto_algorithm algorithm);

/** Returns the encryption nonce size in bytes, or zero if unsupported.
 * @param algorithm CLM_CRYPTO_ALGORITHM_* identifier.
 * @return Required nonce bytes; nonce is public but must be unique per key.
 */
size_t clm_crypto_nonce_size(clm_crypto_algorithm algorithm);

/** Returns the authentication-tag size in bytes, or zero if unsupported.
 * @param algorithm CLM_CRYPTO_ALGORITHM_* identifier.
 * @return Required tag bytes.
 */
size_t clm_crypto_tag_size(clm_crypto_algorithm algorithm);

/** Writes the supported cipher's human-readable name to an existing CLM string.
 * @param algorithm CLM cipher ID.
 * @param destination_id Existing CLM string ID; no new ID is created.
 * @return SUCCESS, UNSUPPORTED_ALGORITHM, or INVALID_ARGUMENT.
 */
clm_int clm_crypto_algorithm_name(
    clm_crypto_algorithm algorithm,
    clm_id destination_id);

/** Overwrites caller-owned sensitive memory using Monocypher's wipe primitive.
 * @param buffer Writable memory, or NULL only when length is zero.
 * @param length Number of bytes to erase.
 * @return SUCCESS or INVALID_ARGUMENT. Copies elsewhere remain unaffected.
 */
clm_int clm_crypto_secure_zero(void* buffer, size_t length);

/** Fills a caller-owned buffer from the OS cryptographically secure RNG.
 * @param output Writable destination; NULL permitted only for zero length.
 * @param length Exact number of random bytes requested.
 * @return SUCCESS, INVALID_ARGUMENT, NO_MEMORY, or BACKEND on RNG failure.
 * @note This function stages output so RNG errors leave the destination intact.
 *       Windows uses BCryptGenRandom (link bcrypt); Unix uses /dev/urandom.
 *       Other platforms fail closed. This does not use rand().
 */
clm_int clm_crypto_random_bytes(void* output, size_t length);

/** Generates a new 32-byte symmetric encryption key from the OS CSPRNG.
 * @param algorithm Supported cipher ID.
 * @param output Caller-owned secret-key buffer.
 * @param capacity Available buffer bytes, at least clm_crypto_key_size().
 * @return SUCCESS or a CLM_CRYPTO_ERROR_* code; unchanged on error.
 * @note Callers must wipe keys after use; ordinary BPA isn't secure memory.
 */
clm_int clm_crypto_key_generate(
    clm_crypto_algorithm algorithm, void* output, size_t capacity);

/** Generates a 24-byte XChaCha20-Poly1305 nonce from the OS CSPRNG.
 * @param algorithm Supported cipher ID.
 * @param output Writable nonce destination.
 * @param capacity Available bytes, at least clm_crypto_nonce_size().
 * @return SUCCESS or a CLM_CRYPTO_ERROR_* code; unchanged on error.
 * @warning Callers MUST avoid nonce reuse with the same key. Randomness makes
 *          collisions negligible for 24-byte nonces, not mathematically impossible.
 */
clm_int clm_crypto_nonce_generate(
    clm_crypto_algorithm algorithm, void* output, size_t capacity);

/** Alias for nonce generation; the IV in this cipher is a 24-byte nonce.
 * @param algorithm Supported cipher ID.
 * @param output Writable IV destination.
 * @param capacity Available IV bytes.
 * @return Same status as clm_crypto_nonce_generate().
 */
clm_int clm_crypto_iv_generate(
    clm_crypto_algorithm algorithm, void* output, size_t capacity);

/** Tests whether two nonempty caller-owned address ranges overlap.
 * @param a First range start, non-NULL when a_size is nonzero.
 * @param a_size First range length.
 * @param b Second range start, non-NULL when b_size is nonzero.
 * @param b_size Second range length.
 * @return Nonzero on overlap (for CLM input validation only).
 */
int clm_crypto_private_overlap(
    const void* a, size_t a_size, const void* b, size_t b_size);

/** Encrypts arbitrary bytes with XChaCha20-Poly1305 authenticated encryption.
 * @param algorithm Supported cipher ID.
 * @param key Borrowed 32-byte secret key; key_length must be exact.
 * @param key_length Key byte length.
 * @param nonce Borrowed unique 24-byte nonce; nonce_length must be exact.
 * @param nonce_length Nonce byte length.
 * @param aad Optional associated data, authenticated but not encrypted.
 * @param aad_length Associated-data byte length; NULL is allowed for zero.
 * @param plaintext Borrowed arbitrary bytes; NULL allowed if length is zero.
 * @param plaintext_length Number of bytes to encrypt.
 * @param ciphertext Caller-owned destination with plaintext_length capacity.
 * @param ciphertext_capacity Output buffer capacity in bytes.
 * @param tag Caller-owned destination receiving the 16-byte authentication tag.
 * @param tag_capacity Capacity for the authentication tag.
 * @return SUCCESS or invalid argument/algorithm/buffer status.
 * @note This API does not allow overlapping input/output ranges. Both outputs
 *       are unchanged when validation fails. Never reuse a (key, nonce) pair.
 */
clm_int clm_crypto_encrypt_bytes(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const void* plaintext, size_t plaintext_length,
    void* ciphertext, size_t ciphertext_capacity,
    void* tag, size_t tag_capacity);

/** Authenticates and decrypts binary ciphertext without exposing unauthenticated data.
 * @param algorithm Supported cipher ID.
 * @param key Borrowed 32-byte secret key.
 * @param key_length Exact key size.
 * @param nonce Borrowed nonce used for encryption.
 * @param nonce_length Exact 24-byte nonce size.
 * @param aad Borrowed associated data matching the encryption call, or NULL.
 * @param aad_length Associated-data length.
 * @param ciphertext Borrowed ciphertext; NULL allowed only if empty.
 * @param ciphertext_length Ciphertext byte count.
 * @param tag Borrowed 16-byte authentication tag.
 * @param tag_length Authentication-tag length.
 * @param plaintext Caller-owned decrypted-byte destination.
 * @param plaintext_capacity Writable output capacity.
 * @return SUCCESS, AUTHENTICATION, NO_MEMORY, or argument/capacity error.
 * @note Decryption stages plaintext and copies it only after authentication;
 *       plaintext output is unchanged on failure. No buffer overlap permitted.
 */
clm_int clm_crypto_decrypt_bytes(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const void* ciphertext, size_t ciphertext_length,
    const void* tag, size_t tag_length,
    void* plaintext, size_t plaintext_capacity);

/** Encrypts the bytes of a NUL-terminated C string (excluding the terminator).
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret encryption key and its exact size.
 * @param nonce, nonce_length Unique nonce and its exact size.
 * @param aad, aad_length Optional authenticated associated data and byte size.
 * @param text Non-NULL NUL-terminated C string; no UTF-8 conversion occurs.
 * @param ciphertext, ciphertext_capacity Binary ciphertext output and capacity.
 * @param tag, tag_capacity Binary authentication-tag output and capacity.
 * @return Same errors as clm_crypto_encrypt_bytes().
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param ciphertext_capacity Input parameter described above.
 * @param tag_capacity Input parameter described above.
 */
clm_int clm_crypto_encrypt_bytes_as(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* text,
    void* ciphertext, size_t ciphertext_capacity,
    void* tag, size_t tag_capacity);

/** Borrows UTF-8 bytes from a CLM string ID and encrypts them to a binary buffer.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and size.
 * @param nonce, nonce_length Unique nonce and size.
 * @param aad, aad_length Optional associated data and length.
 * @param source_id Existing CLM string ID to encrypt without normalization.
 * @param ciphertext, ciphertext_capacity Caller-owned binary ciphertext.
 * @param tag, tag_capacity Authentication-tag output and capacity.
 * @return SUCCESS or argument/capacity/error status.
 * @note The borrowed string must not be mutated concurrently with this call.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param ciphertext_capacity Input parameter described above.
 * @param tag_capacity Input parameter described above.
 */
clm_int clm_crypto_encrypt_bytes_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id,
    void* ciphertext, size_t ciphertext_capacity,
    void* tag, size_t tag_capacity);

/** Returns required lowercase-hex buffer size including a NUL terminator.
 * @param byte_length Number of binary bytes to encode.
 * @return 2 * byte_length + 1, or zero when that would overflow size_t.
 */
size_t clm_crypto_hex_size(size_t byte_length);

/** Encodes arbitrary bytes to lowercase hexadecimal text.
 * @param bytes Borrowed bytes; NULL accepted only when length is zero.
 * @param length Number of input bytes.
 * @param out_hex Caller-owned text buffer including space for NUL.
 * @param capacity Available output bytes.
 * @return SUCCESS, INVALID_ARGUMENT, or BUFFER_TOO_SMALL; no output on error.
 * @note Input and output buffers must not overlap.
 */
clm_int clm_crypto_hex_encode(
    const void* bytes, size_t length, char* out_hex, size_t capacity);

/** Decodes lowercase or uppercase hex, rejecting invalid or odd-length text.
 * @param text NUL-terminated hex text with no whitespace or prefix.
 * @param output Caller-owned byte buffer, or NULL if text is empty.
 * @param capacity Available destination bytes.
 * @param out_length Receives decoded byte count on success; may be NULL.
 * @return SUCCESS, INVALID_TEXT, INVALID_ARGUMENT, or BUFFER_TOO_SMALL.
 * @note Output is unchanged on validation failure; input/output must not overlap.
 */
clm_int clm_crypto_hex_decode(
    const char* text, void* output, size_t capacity, size_t* out_length);

/** Encrypts raw bytes and stores the ciphertext as lowercase hex in a CLM string.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and size.
 * @param nonce, nonce_length Unique nonce and size.
 * @param aad, aad_length Associated data and size.
 * @param plaintext, plaintext_length Borrowed binary input and its size.
 * @param destination_id Existing CLM string ID receiving hex ciphertext.
 * @param tag, tag_capacity Caller-owned 16-byte binary tag buffer.
 * @return SUCCESS or a CLM_CRYPTO_ERROR_* code.
 * @note Destination is not modified on encryption or encoding failure.
 *       The nonce and tag must be saved alongside the ciphertext.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param plaintext_length Input parameter described above.
 * @param tag_capacity Input parameter described above.
 */
clm_int clm_crypto_encrypt_bytes_to(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const void* plaintext, size_t plaintext_length,
    clm_id destination_id,
    void* tag, size_t tag_capacity);

/** Encrypts a NUL-terminated UTF-8 text string to an existing CLM hex string.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and its size.
 * @param nonce, nonce_length Unique nonce and its size.
 * @param aad, aad_length Optional associated data and byte count.
 * @param text Non-NULL NUL-terminated UTF-8 input; no normalization.
 * @param destination_id Existing CLM string receiving ciphertext hex.
 * @param tag, tag_capacity Binary authentication tag destination and size.
 * @return SUCCESS or argument/UTF-8/capacity/backend error.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_capacity Input parameter described above.
 */
clm_int clm_crypto_encrypt_string_to(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* text, clm_id destination_id,
    void* tag, size_t tag_capacity);

/** Encrypts the contents of a CLM UTF-8 string to another CLM hex string.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and its size.
 * @param nonce, nonce_length Unique nonce and its size.
 * @param aad, aad_length Optional associated data and byte count.
 * @param source_id Existing CLM string holding plaintext.
 * @param destination_id Different existing CLM string to receive ciphertext hex.
 * @param tag, tag_capacity Binary tag destination and capacity.
 * @return SUCCESS or argument/capacity/error status.
 * @note IDs must differ to prevent invalidating the borrowed source pointer.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_capacity Input parameter described above.
 */
clm_int clm_crypto_encrypt_string_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, clm_id destination_id,
    void* tag, size_t tag_capacity);

/** Decrypts a hex-encoded ciphertext to a caller-owned binary byte buffer.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and exact size.
 * @param nonce, nonce_length Original nonce and exact size.
 * @param aad, aad_length Original associated data and byte length.
 * @param hex_text NUL-terminated hexadecimal ciphertext.
 * @param tag, tag_length Original binary 16-byte tag and exact length.
 * @param plaintext Caller-owned output buffer (NULL allowed for empty text).
 * @param plaintext_capacity Writable bytes available.
 * @return SUCCESS, INVALID_TEXT, AUTHENTICATION, or another CLM error.
 * @note Output is unchanged for invalid ciphertext/tag. No text decoding occurs.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_length Input parameter described above.
 */
clm_int clm_crypto_decrypt_bytes_as(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* hex_text, const void* tag, size_t tag_length,
    void* plaintext, size_t plaintext_capacity);

/** Decrypts ciphertext hex held by a CLM string ID into caller-owned bytes.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and size.
 * @param nonce, nonce_length Original nonce and size.
 * @param aad, aad_length Original associated data and size.
 * @param source_id Existing CLM hex string ID.
 * @param tag, tag_length Original binary tag and size.
 * @param plaintext, plaintext_capacity Binary output buffer and capacity.
 * @return SUCCESS or a CLM_CRYPTO_ERROR_* result; output unchanged on failure.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_length Input parameter described above.
 * @param plaintext_capacity Input parameter described above.
 */
clm_int clm_crypto_decrypt_bytes_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, const void* tag, size_t tag_length,
    void* plaintext, size_t plaintext_capacity);

/** Decrypts a hex ciphertext C string to a pre-existing CLM UTF-8 string ID.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and size.
 * @param nonce, nonce_length Original nonce and size.
 * @param aad, aad_length Original associated data and size.
 * @param hex_text Hexadecimal ciphertext, NUL-terminated.
 * @param tag, tag_length Original binary tag and size.
 * @param destination_id Existing CLM string receiving validated UTF-8 text.
 * @return SUCCESS, AUTHENTICATION, INVALID_TEXT, or another CLM error.
 * @note Validates UTF-8 and rejects embedded NUL. Binary plaintext requires
 *       clm_crypto_decrypt_bytes_as(), not this string adapter.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_length Input parameter described above.
 */
clm_int clm_crypto_decrypt_string_to(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* hex_text, const void* tag, size_t tag_length,
    clm_id destination_id);

/** Decrypts one CLM ciphertext hex string to another CLM UTF-8 string.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and size.
 * @param nonce, nonce_length Original nonce and size.
 * @param aad, aad_length Original associated data and byte size.
 * @param source_id Existing ciphertext hex string ID.
 * @param tag, tag_length Original 16-byte binary tag and length.
 * @param destination_id Different CLM UTF-8 string ID receiving plaintext.
 * @return SUCCESS or a CLM_CRYPTO_ERROR_* result; no write on auth failure.
 * @note Source and destination IDs must differ.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_length Input parameter described above.
 */
clm_int clm_crypto_decrypt_string_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, const void* tag, size_t tag_length,
    clm_id destination_id);

/** Fills an existing BPA fixed-size block with OS cryptographic random bytes.
 * @param destination_id Existing non-string BPA block to fill exactly.
 * @return SUCCESS, INVALID_ARGUMENT, NO_MEMORY, or BACKEND.
 * @note Not a secured memory type: normal BPA storage may retain key material.
 */
clm_int clm_crypto_random_to(clm_id destination_id);

/** Generates a symmetric key into an existing 32-byte BPA fixed-size block.
 * @param algorithm Supported cipher ID.
 * @param destination_id Pre-existing non-string BPA ID of exact key size.
 * @return SUCCESS or argument/algorithm/RNG failure.
 */
clm_int clm_crypto_key_generate_to(
    clm_crypto_algorithm algorithm, clm_id destination_id);

/** Generates a nonce into an existing 24-byte BPA fixed-size block.
 * @param algorithm Supported cipher ID.
 * @param destination_id Pre-existing non-string BPA ID of exact nonce size.
 * @return SUCCESS or argument/algorithm/RNG failure.
 * @warning Still requires nonce uniqueness under the same encryption key.
 */
clm_int clm_crypto_nonce_generate_to(
    clm_crypto_algorithm algorithm, clm_id destination_id);

/** Encrypts a binary BPA block into another existing same-sized binary BPA block.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Borrowed key and exact size.
 * @param nonce, nonce_length Unique nonce and exact size.
 * @param aad, aad_length Optional associated data and length.
 * @param source_id Pre-existing fixed-size BPA plaintext block ID.
 * @param destination_id Different existing same-sized BPA ciphertext block ID.
 * @param tag, tag_capacity Caller-owned 16-byte tag buffer and capacity.
 * @return SUCCESS or a CLM_CRYPTO_ERROR_* code.
 * @note Destination is written only after encryption; blocks are not resized.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_capacity Input parameter described above.
 */
clm_int clm_crypto_encrypt_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, clm_id destination_id,
    void* tag, size_t tag_capacity);

/** Decrypts binary BPA ciphertext to another existing same-sized BPA block.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and exact length.
 * @param nonce, nonce_length Original nonce and exact length.
 * @param aad, aad_length Original associated data and its byte length.
 * @param source_id Pre-existing ciphertext block ID.
 * @param tag, tag_length Binary authentication tag and exact length.
 * @param destination_id Different existing plaintext BPA block ID.
 * @return SUCCESS, AUTHENTICATION or another error. Destination unchanged on
 *         authentication failure.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_length Input parameter described above.
 */
clm_int clm_crypto_decrypt_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, const void* tag, size_t tag_length,
    clm_id destination_id);

/** Derives a password-based key using Monocypher's Argon2id (memory-hard KDF).
 * @param password Arbitrary password bytes, NULL only if password_length zero.
 * @param password_length Password byte count, not Unicode character count.
 * @param salt Borrowed random salt, at least 16 bytes; store it with parameters.
 * @param salt_length Salt byte count (16 recommended or greater).
 * @param memory_kib Memory cost in 1024-byte blocks; at least 8 and bounded
 *                   by caller's memory budget. E.g. 65536 for 64 MiB.
 * @param passes CPU cost, at least 1; application chooses deployment policy.
 * @param output Caller-owned derived key destination, at least output_length.
 * @param output_length Desired derived key length (at least 4 bytes).
 * @return SUCCESS, INVALID_ARGUMENT, NO_MEMORY, or BACKEND.
 * @note Work memory is allocated, used and securely wiped before release.
 *       This operation is intentionally computationally expensive; throttle
 *       attacker-controlled requests and do not use it on an event-loop thread.
 */
clm_int clm_crypto_derive_key_bytes(
    const void* password, size_t password_length,
    const void* salt, size_t salt_length,
    uint32_t memory_kib, uint32_t passes,
    void* output, size_t output_length);

/** Derives Argon2id key bytes from a NUL-terminated password C string.
 * @param password Borrowed NUL-terminated password; embedded NUL is excluded.
 * @param salt, salt_length Random salt and size, at least 16 bytes.
 * @param memory_kib Memory cost in KiB, at least eight.
 * @param passes Positive work-factor pass count.
 * @param output, output_length Caller-owned derived-key buffer and exact length.
 * @return Same statuses as clm_crypto_derive_key_bytes().
 * @param salt_length Input parameter described above.
 * @param output_length Input parameter described above.
 */
clm_int clm_crypto_derive_key_as(
    const char* password, const void* salt, size_t salt_length,
    uint32_t memory_kib, uint32_t passes,
    void* output, size_t output_length);

/** Derives Argon2id key bytes from an existing CLM UTF-8 password string ID.
 * @param password_id Existing CLM UTF-8 string ID; read as raw UTF-8 bytes.
 * @param salt, salt_length Random salt and size.
 * @param memory_kib Work-memory cost in KiB.
 * @param passes CPU work factor.
 * @param output, output_length Derived-key destination and length.
 * @return Same statuses as clm_crypto_derive_key_bytes().
 * @param salt_length Input parameter described above.
 * @param output_length Input parameter described above.
 */
clm_int clm_crypto_derive_key_from(
    clm_id password_id, const void* salt, size_t salt_length,
    uint32_t memory_kib, uint32_t passes,
    void* output, size_t output_length);

/** Generates X25519 private/public key pair using OS randomness and Monocypher.
 * @param secret_key Writable 32-byte private-key output.
 * @param secret_capacity Secret-key destination capacity in bytes.
 * @param public_key Writable 32-byte public-key output.
 * @param public_capacity Public-key destination capacity.
 * @return SUCCESS, BUFFER_TOO_SMALL, INVALID_ARGUMENT, or RNG failure.
 * @note Caller owns the private key and must securely wipe it after use.
 */
clm_int clm_crypto_x25519_key_pair(
    void* secret_key, size_t secret_capacity,
    void* public_key, size_t public_capacity);

/** Computes X25519 shared secret and rejects the all-zero result.
 * @param secret_key Borrowed 32-byte private key.
 * @param peer_public_key Borrowed 32-byte peer public key.
 * @param output Caller-owned 32-byte shared-secret buffer.
 * @param capacity Output capacity in bytes.
 * @return SUCCESS, INVALID_ARGUMENT, BUFFER_TOO_SMALL, or AUTHENTICATION
 *         for an all-zero / invalid peer result; output unchanged on failure.
 * @warning Raw X25519 output is not an AEAD key by itself. Derive a session
 *          key with a transcript-bound KDF and authenticate peer identities.
 */
clm_int clm_crypto_x25519_shared(
    const void* secret_key, const void* peer_public_key,
    void* output, size_t capacity);

/*
 * IMPORTANT: Standard Ed25519 is NOT provided by Monocypher core.
 * This wrapper deliberately omits EdDSA signatures until a separate explicit
 * algorithm ID and interoperability contract are defined. This avoids
 * accidentally treating Monocypher's BLAKE2b EdDSA as standard Ed25519.
 */


/** Encrypts a C string and returns hex ciphertext in a caller-owned C buffer.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and byte length.
 * @param nonce, nonce_length Unique nonce and byte length.
 * @param aad, aad_length Optional additional authentication data and length.
 * @param text NUL-terminated UTF-8 plaintext; NUL itself is not encrypted.
 * @param out_hex Writable hexadecimal ciphertext including terminating NUL.
 * @param hex_capacity Available hexadecimal output buffer bytes.
 * @param tag, tag_capacity Caller-owned 16-byte authentication tag and capacity.
 * @return SUCCESS, INVALID_TEXT, BUFFER_TOO_SMALL, NO_MEMORY, or another error.
 * @note Text must be UTF-8; use clm_crypto_encrypt_bytes_as for arbitrary bytes.
 *       Output buffers remain untouched if validation or backend operation fails.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_capacity Input parameter described above.
 */
clm_int clm_crypto_encrypt_string_as(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* text, char* out_hex, size_t hex_capacity,
    void* tag, size_t tag_capacity);

/** Decrypts hex ciphertext to caller-owned UTF-8 C text with a terminating NUL.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and byte length.
 * @param nonce, nonce_length Original nonce and length.
 * @param aad, aad_length Original additional authentication data and length.
 * @param hex_text NUL-terminated hexadecimal ciphertext text.
 * @param tag, tag_length Original binary authentication tag and exact size.
 * @param plaintext Writable C-text destination, at least decoded size plus NUL.
 * @param plaintext_capacity Number of available output bytes.
 * @return SUCCESS, AUTHENTICATION, INVALID_TEXT, or another CLM error.
 * @note Embedded NUL and invalid UTF-8 are rejected. Destination is not
 *       modified for failure; use decrypt_bytes_as for arbitrary binary data.
 * @param key_length Input parameter described above.
 * @param nonce_length Input parameter described above.
 * @param aad_length Input parameter described above.
 * @param tag_length Input parameter described above.
 */
clm_int clm_crypto_decrypt_string_as(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* hex_text, const void* tag, size_t tag_length,
    char* plaintext, size_t plaintext_capacity);

/** Derives a key from password bytes into an existing fixed-size BPA block.
 * @param password, password_length Borrowed password bytes and size.
 * @param salt, salt_length Unpredictable salt and its size, at least 16 bytes.
 * @param memory_kib Argon2id memory cost in KiB.
 * @param passes Argon2id pass count.
 * @param destination_id Existing non-string BPA block whose size is the
 *                       desired derived-key size, at least four bytes.
 * @return SUCCESS or argument/memory/backend error; ID is not created.
 * @note BPA is ordinary memory: this does not secure derived key storage.
 * @param password_length Input parameter described above.
 * @param salt_length Input parameter described above.
 */
clm_int clm_crypto_derive_key_to(
    const void* password, size_t password_length,
    const void* salt, size_t salt_length,
    uint32_t memory_kib, uint32_t passes,
    clm_id destination_id);

