#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 *
 * CLM adapter code: Public Domain — CC0 1.0 Universal.
 * Vendored Monocypher: BSD-2-Clause OR CC0-1.0 (see ext/monocypher.license.md).
 *
 * Don't start from scratch, bro.
 * =============================================================================
 *
 * C11 single-header interface for Monocypher 4.0.3 (core only).
 *
 * To define Monocypher's external symbols, define CLM_CRYPTO_IMPLEMENTATION
 * before including this header in EXACTLY ONE translation unit:
 *
 *     #define CLM_CRYPTO_IMPLEMENTATION
 *     #include "clm/crypto.h"
 *
 * Other translation units include it without the macro. Do not also compile
 * ext/monocypher.c separately. The upstream .h/.c remain unmodified.
 *
 * Only XChaCha20-Poly1305 AEAD is supported. Outputs are separate ciphertext
 * and 16-byte tags, and caller must preserve the 24-byte nonce. No framing.
 *
 * Byte operations are binary-safe; _as accepts a NUL-terminated char string,
 * _from reads an existing CLM string/BPA ID, _to writes an existing ID.
 * Text ciphertext is lowercase hexadecimal (never arbitrary UTF-8).
 *
 * SECURITY: Never reuse an encryption nonce with the same key. Keys and
 * plaintext borrowed from BPA are not protected from memory inspection and
 * BPA is not internally thread-safe. Wipe temporary plaintext and key copies.
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
static inline size_t clm_crypto_key_size(clm_crypto_algorithm algorithm)
{
    return algorithm == CLM_CRYPTO_ALGORITHM_XCHACHA20_POLY1305
        ? CLM_CRYPTO_KEY_SIZE : 0u;
}

/** Returns the encryption nonce size in bytes, or zero if unsupported.
 * @param algorithm CLM_CRYPTO_ALGORITHM_* identifier.
 * @return Required nonce bytes; nonce is public but must be unique per key.
 */
static inline size_t clm_crypto_nonce_size(clm_crypto_algorithm algorithm)
{
    return clm_crypto_key_size(algorithm) ? CLM_CRYPTO_NONCE_SIZE : 0u;
}

/** Returns the authentication-tag size in bytes, or zero if unsupported.
 * @param algorithm CLM_CRYPTO_ALGORITHM_* identifier.
 * @return Required tag bytes.
 */
static inline size_t clm_crypto_tag_size(clm_crypto_algorithm algorithm)
{
    return clm_crypto_key_size(algorithm) ? CLM_CRYPTO_TAG_SIZE : 0u;
}

/** Writes the supported cipher's human-readable name to an existing CLM string.
 * @param algorithm CLM cipher ID.
 * @param destination_id Existing CLM string ID; no new ID is created.
 * @return SUCCESS, UNSUPPORTED_ALGORITHM, or INVALID_ARGUMENT.
 */
static inline clm_int clm_crypto_algorithm_name(
    clm_crypto_algorithm algorithm,
    clm_id destination_id)
{
    if (clm_crypto_key_size(algorithm) == 0u) {
        return CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM;
    }
    return clm_string_set(destination_id, "XChaCha20-Poly1305") < 0
        ? CLM_CRYPTO_ERROR_INVALID_ARGUMENT : CLM_CRYPTO_SUCCESS;
}

/** Overwrites caller-owned sensitive memory using Monocypher's wipe primitive.
 * @param buffer Writable memory, or NULL only when length is zero.
 * @param length Number of bytes to erase.
 * @return SUCCESS or INVALID_ARGUMENT. Copies elsewhere remain unaffected.
 */
static inline clm_int clm_crypto_secure_zero(void* buffer, size_t length)
{
    if (length != 0u && buffer == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (length != 0u) {
        crypto_wipe(buffer, length);
    }
    return CLM_CRYPTO_SUCCESS;
}

/** Fills a caller-owned buffer from the OS cryptographically secure RNG.
 * @param output Writable destination; NULL permitted only for zero length.
 * @param length Exact number of random bytes requested.
 * @return SUCCESS, INVALID_ARGUMENT, NO_MEMORY, or BACKEND on RNG failure.
 * @note This function stages output so RNG errors leave the destination intact.
 *       Windows uses BCryptGenRandom (link bcrypt); Unix uses /dev/urandom.
 *       Other platforms fail closed. This does not use rand().
 */
static inline clm_int clm_crypto_random_bytes(void* output, size_t length)
{
    unsigned char* staged;
    size_t offset = 0u;
    clm_int status = CLM_CRYPTO_ERROR_BACKEND;

    if (length != 0u && output == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (length == 0u) {
        return CLM_CRYPTO_SUCCESS;
    }
    staged = (unsigned char*)malloc(length);
    if (staged == NULL) {
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }

#if defined(_WIN32)
    while (offset < length) {
        const size_t remaining = length - offset;
        const ULONG max_chunk = (ULONG)-1;
        const ULONG chunk = remaining > (size_t)max_chunk
            ? max_chunk : (ULONG)remaining;
        const NTSTATUS result = BCryptGenRandom(
            NULL, staged + offset, chunk, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (result < 0) {
            break;
        }
        offset += (size_t)chunk;
    }
#elif defined(__unix__) || defined(__APPLE__) || defined(__linux__)
    FILE* source = fopen("/dev/urandom", "rb");
    if (source != NULL) {
        while (offset < length) {
            const size_t received = fread(staged + offset, 1u, length - offset, source);
            if (received == 0u) {
                break;
            }
            offset += received;
        }
        (void)fclose(source);
    }
#else
    /* A platform without a reviewed CSPRNG must supply a port. */
#endif

    if (offset == length) {
        memcpy(output, staged, length);
        status = CLM_CRYPTO_SUCCESS;
    }
    crypto_wipe(staged, length);
    free(staged);
    return status;
}

/** Generates a new 32-byte symmetric encryption key from the OS CSPRNG.
 * @param algorithm Supported cipher ID.
 * @param output Caller-owned secret-key buffer.
 * @param capacity Available buffer bytes, at least clm_crypto_key_size().
 * @return SUCCESS or a CLM_CRYPTO_ERROR_* code; unchanged on error.
 * @note Callers must wipe keys after use; ordinary BPA isn't secure memory.
 */
static inline clm_int clm_crypto_key_generate(
    clm_crypto_algorithm algorithm, void* output, size_t capacity)
{
    const size_t required = clm_crypto_key_size(algorithm);
    if (required == 0u) {
        return CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM;
    }
    if (output == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (capacity < required) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    return clm_crypto_random_bytes(output, required);
}

/** Generates a 24-byte XChaCha20-Poly1305 nonce from the OS CSPRNG.
 * @param algorithm Supported cipher ID.
 * @param output Writable nonce destination.
 * @param capacity Available bytes, at least clm_crypto_nonce_size().
 * @return SUCCESS or a CLM_CRYPTO_ERROR_* code; unchanged on error.
 * @warning Callers MUST avoid nonce reuse with the same key. Randomness makes
 *          collisions negligible for 24-byte nonces, not mathematically impossible.
 */
static inline clm_int clm_crypto_nonce_generate(
    clm_crypto_algorithm algorithm, void* output, size_t capacity)
{
    const size_t required = clm_crypto_nonce_size(algorithm);
    if (required == 0u) {
        return CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM;
    }
    if (output == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (capacity < required) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    return clm_crypto_random_bytes(output, required);
}

/** Alias for nonce generation; the IV in this cipher is a 24-byte nonce.
 * @param algorithm Supported cipher ID.
 * @param output Writable IV destination.
 * @param capacity Available IV bytes.
 * @return Same status as clm_crypto_nonce_generate().
 */
static inline clm_int clm_crypto_iv_generate(
    clm_crypto_algorithm algorithm, void* output, size_t capacity)
{
    return clm_crypto_nonce_generate(algorithm, output, capacity);
}

/** Tests whether two nonempty caller-owned address ranges overlap.
 * @param a First range start, non-NULL when a_size is nonzero.
 * @param a_size First range length.
 * @param b Second range start, non-NULL when b_size is nonzero.
 * @param b_size Second range length.
 * @return Nonzero on overlap (for CLM input validation only).
 */
static inline int clm_crypto_private_overlap(
    const void* a, size_t a_size, const void* b, size_t b_size)
{
    uintptr_t x, y;
    if (a_size == 0u || b_size == 0u || a == NULL || b == NULL) {
        return 0;
    }
    x = (uintptr_t)a;
    y = (uintptr_t)b;
    if (x <= y) {
        return y - x < a_size;
    }
    return x - y < b_size;
}

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
static inline clm_int clm_crypto_encrypt_bytes(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const void* plaintext, size_t plaintext_length,
    void* ciphertext, size_t ciphertext_capacity,
    void* tag, size_t tag_capacity)
{
    static const uint8_t empty_byte = 0u;
    uint8_t empty_output = 0u;
    uint8_t staged_tag[CLM_CRYPTO_TAG_SIZE];
    uint8_t* out;
    const uint8_t* in;
    const uint8_t* ad;

    if (clm_crypto_key_size(algorithm) == 0u) {
        return CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM;
    }
    if (key == NULL || key_length != CLM_CRYPTO_KEY_SIZE ||
        nonce == NULL || nonce_length != CLM_CRYPTO_NONCE_SIZE ||
        tag == NULL || (aad_length != 0u && aad == NULL) ||
        (plaintext_length != 0u && (plaintext == NULL || ciphertext == NULL))) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (ciphertext_capacity < plaintext_length || tag_capacity < CLM_CRYPTO_TAG_SIZE) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    if (clm_crypto_private_overlap(plaintext, plaintext_length, ciphertext, plaintext_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, ciphertext, plaintext_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, plaintext, plaintext_length) ||
        clm_crypto_private_overlap(ciphertext, plaintext_length, key, key_length) ||
        clm_crypto_private_overlap(ciphertext, plaintext_length, nonce, nonce_length) ||
        clm_crypto_private_overlap(ciphertext, plaintext_length, aad, aad_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, key, key_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, nonce, nonce_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, aad, aad_length)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }

    out = ciphertext != NULL ? (uint8_t*)ciphertext : &empty_output;
    in = plaintext != NULL ? (const uint8_t*)plaintext : &empty_byte;
    ad = aad != NULL ? (const uint8_t*)aad : &empty_byte;
    crypto_aead_lock(out, staged_tag, (const uint8_t*)key,
                     (const uint8_t*)nonce, ad, aad_length, in, plaintext_length);
    memcpy(tag, staged_tag, sizeof(staged_tag));
    crypto_wipe(staged_tag, sizeof(staged_tag));
    return CLM_CRYPTO_SUCCESS;
}

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
static inline clm_int clm_crypto_decrypt_bytes(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const void* ciphertext, size_t ciphertext_length,
    const void* tag, size_t tag_length,
    void* plaintext, size_t plaintext_capacity)
{
    static const uint8_t empty_byte = 0u;
    uint8_t empty_output = 0u;
    uint8_t* staged = &empty_output;
    clm_int status = CLM_CRYPTO_ERROR_AUTHENTICATION;

    if (clm_crypto_key_size(algorithm) == 0u) {
        return CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM;
    }
    if (key == NULL || key_length != CLM_CRYPTO_KEY_SIZE ||
        nonce == NULL || nonce_length != CLM_CRYPTO_NONCE_SIZE ||
        tag == NULL || tag_length != CLM_CRYPTO_TAG_SIZE ||
        (aad_length != 0u && aad == NULL) ||
        (ciphertext_length != 0u && (ciphertext == NULL || plaintext == NULL))) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (plaintext_capacity < ciphertext_length) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    if (clm_crypto_private_overlap(plaintext, ciphertext_length, ciphertext, ciphertext_length) ||
        clm_crypto_private_overlap(plaintext, ciphertext_length, key, key_length) ||
        clm_crypto_private_overlap(plaintext, ciphertext_length, nonce, nonce_length) ||
        clm_crypto_private_overlap(plaintext, ciphertext_length, aad, aad_length) ||
        clm_crypto_private_overlap(plaintext, ciphertext_length, tag, tag_length)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (ciphertext_length != 0u) {
        staged = (uint8_t*)malloc(ciphertext_length);
        if (staged == NULL) {
            return CLM_CRYPTO_ERROR_NO_MEMORY;
        }
    }
    const uint8_t* input = ciphertext != NULL ? (const uint8_t*)ciphertext : &empty_byte;
    const uint8_t* ad = aad != NULL ? (const uint8_t*)aad : &empty_byte;
    if (crypto_aead_unlock(staged, (const uint8_t*)tag,
                           (const uint8_t*)key, (const uint8_t*)nonce,
                           ad, aad_length, input, ciphertext_length) == 0) {
        if (ciphertext_length != 0u) {
            memcpy(plaintext, staged, ciphertext_length);
        }
        status = CLM_CRYPTO_SUCCESS;
    }
    if (ciphertext_length != 0u) {
        crypto_wipe(staged, ciphertext_length);
        free(staged);
    }
    return status;
}

/** Encrypts the bytes of a NUL-terminated C string (excluding the terminator).
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret encryption key and its exact size.
 * @param nonce, nonce_length Unique nonce and its exact size.
 * @param aad, aad_length Optional authenticated associated data and byte size.
 * @param text Non-NULL NUL-terminated C string; no UTF-8 conversion occurs.
 * @param ciphertext, ciphertext_capacity Binary ciphertext output and capacity.
 * @param tag, tag_capacity Binary authentication-tag output and capacity.
 * @return Same errors as clm_crypto_encrypt_bytes().
 */
static inline clm_int clm_crypto_encrypt_bytes_as(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* text,
    void* ciphertext, size_t ciphertext_capacity,
    void* tag, size_t tag_capacity)
{
    if (text == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    return clm_crypto_encrypt_bytes(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        text, strlen(text), ciphertext, ciphertext_capacity, tag, tag_capacity);
}

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
 */
static inline clm_int clm_crypto_encrypt_bytes_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id,
    void* ciphertext, size_t ciphertext_capacity,
    void* tag, size_t tag_capacity)
{
    clm_bpa_block_node* block = NULL;
    const char* text = NULL;
    size_t byte_length = 0u;
    size_t characters = 0u;
    if (clm_string_get_block(source_id, &block, &text,
                             &byte_length, &characters) != CLM_BPA_SUCCESS) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    return clm_crypto_encrypt_bytes(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        text, byte_length, ciphertext, ciphertext_capacity, tag, tag_capacity);
}

/** Returns required lowercase-hex buffer size including a NUL terminator.
 * @param byte_length Number of binary bytes to encode.
 * @return 2 * byte_length + 1, or zero when that would overflow size_t.
 */
static inline size_t clm_crypto_hex_size(size_t byte_length)
{
    return byte_length > (SIZE_MAX - 1u) / 2u
        ? 0u : byte_length * 2u + 1u;
}

/** Encodes arbitrary bytes to lowercase hexadecimal text.
 * @param bytes Borrowed bytes; NULL accepted only when length is zero.
 * @param length Number of input bytes.
 * @param out_hex Caller-owned text buffer including space for NUL.
 * @param capacity Available output bytes.
 * @return SUCCESS, INVALID_ARGUMENT, or BUFFER_TOO_SMALL; no output on error.
 * @note Input and output buffers must not overlap.
 */
static inline clm_int clm_crypto_hex_encode(
    const void* bytes, size_t length, char* out_hex, size_t capacity)
{
    static const char digits[] = "0123456789abcdef";
    const unsigned char* input = (const unsigned char*)bytes;
    const size_t required = clm_crypto_hex_size(length);

    if (required == 0u || out_hex == NULL || (length != 0u && bytes == NULL) ||
        clm_crypto_private_overlap(bytes, length, out_hex, required)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (capacity < required) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    for (size_t index = 0u; index < length; ++index) {
        out_hex[index * 2u] = digits[input[index] >> 4u];
        out_hex[index * 2u + 1u] = digits[input[index] & 15u];
    }
    out_hex[length * 2u] = '\0';
    return CLM_CRYPTO_SUCCESS;
}

/** Decodes lowercase or uppercase hex, rejecting invalid or odd-length text.
 * @param text NUL-terminated hex text with no whitespace or prefix.
 * @param output Caller-owned byte buffer, or NULL if text is empty.
 * @param capacity Available destination bytes.
 * @param out_length Receives decoded byte count on success; may be NULL.
 * @return SUCCESS, INVALID_TEXT, INVALID_ARGUMENT, or BUFFER_TOO_SMALL.
 * @note Output is unchanged on validation failure; input/output must not overlap.
 */
static inline clm_int clm_crypto_hex_decode(
    const char* text, void* output, size_t capacity, size_t* out_length)
{
    size_t length;
    size_t bytes;
    unsigned char* destination = (unsigned char*)output;

    if (text == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    length = strlen(text);
    if ((length & 1u) != 0u) {
        return CLM_CRYPTO_ERROR_INVALID_TEXT;
    }
    bytes = length / 2u;
    if ((bytes != 0u && output == NULL) ||
        clm_crypto_private_overlap(text, length + 1u, output, bytes)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (capacity < bytes) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    for (size_t i = 0u; i < length; ++i) {
        const unsigned char c = (unsigned char)text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            return CLM_CRYPTO_ERROR_INVALID_TEXT;
        }
    }
    for (size_t i = 0u; i < bytes; ++i) {
        const unsigned char a = (unsigned char)text[i * 2u];
        const unsigned char b = (unsigned char)text[i * 2u + 1u];
        unsigned int hi;
        unsigned int lo;
        if (a <= '9') {
            hi = (unsigned int)(a - (unsigned char)'0');
        } else if (a <= 'F') {
            hi = (unsigned int)(a - (unsigned char)'A') + 10u;
        } else {
            hi = (unsigned int)(a - (unsigned char)'a') + 10u;
        }
        if (b <= '9') {
            lo = (unsigned int)(b - (unsigned char)'0');
        } else if (b <= 'F') {
            lo = (unsigned int)(b - (unsigned char)'A') + 10u;
        } else {
            lo = (unsigned int)(b - (unsigned char)'a') + 10u;
        }
        destination[i] = (unsigned char)((hi << 4u) | lo);
    }
    if (out_length != NULL) {
        *out_length = bytes;
    }
    return CLM_CRYPTO_SUCCESS;
}

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
 */
static inline clm_int clm_crypto_encrypt_bytes_to(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const void* plaintext, size_t plaintext_length,
    clm_id destination_id,
    void* tag, size_t tag_capacity)
{
    char* hex = NULL;
    uint8_t* ciphertext = NULL;
    uint8_t staged_tag[CLM_CRYPTO_TAG_SIZE];
    const size_t hex_capacity = clm_crypto_hex_size(plaintext_length);
    const clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    clm_int status;

    if (destination == NULL || destination->type != CLM_BPA_TYPE_STRING ||
        tag == NULL || tag_capacity < CLM_CRYPTO_TAG_SIZE ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, plaintext, plaintext_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, key, key_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, nonce, nonce_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, aad, aad_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE,
                                   destination->data, destination->length + 1u)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (hex_capacity == 0u || hex_capacity > (size_t)INT_MAX ||
        plaintext_length > (size_t)UINT_MAX) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    hex = (char*)malloc(hex_capacity);
    ciphertext = (uint8_t*)malloc(plaintext_length != 0u ? plaintext_length : 1u);
    if (hex == NULL || ciphertext == NULL) {
        status = CLM_CRYPTO_ERROR_NO_MEMORY;
        goto cleanup;
    }
    status = clm_crypto_encrypt_bytes(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        plaintext, plaintext_length, ciphertext, plaintext_length,
        staged_tag, sizeof(staged_tag));
    if (status != CLM_CRYPTO_SUCCESS) {
        goto cleanup;
    }
    status = clm_crypto_hex_encode(ciphertext, plaintext_length, hex, hex_capacity);
    if (status != CLM_CRYPTO_SUCCESS) {
        goto cleanup;
    }
    if (clm_string_set(destination_id, hex) < 0) {
        status = CLM_CRYPTO_ERROR_BACKEND;
        goto cleanup;
    }
    memcpy(tag, staged_tag, CLM_CRYPTO_TAG_SIZE);
cleanup:
    crypto_wipe(staged_tag, sizeof(staged_tag));
    if (ciphertext != NULL) {
        crypto_wipe(ciphertext, plaintext_length);
        free(ciphertext);
    }
    if (hex != NULL) {
        crypto_wipe(hex, hex_capacity);
        free(hex);
    }
    return status;
}

/** Encrypts a NUL-terminated UTF-8 text string to an existing CLM hex string.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and its size.
 * @param nonce, nonce_length Unique nonce and its size.
 * @param aad, aad_length Optional associated data and byte count.
 * @param text Non-NULL NUL-terminated UTF-8 input; no normalization.
 * @param destination_id Existing CLM string receiving ciphertext hex.
 * @param tag, tag_capacity Binary authentication tag destination and size.
 * @return SUCCESS or argument/UTF-8/capacity/backend error.
 */
static inline clm_int clm_crypto_encrypt_string_to(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* text, clm_id destination_id,
    void* tag, size_t tag_capacity)
{
    size_t characters = 0u;
    size_t length;
    if (text == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    length = strlen(text);
    if (clm_string_measure(text, length, CLM_STRING_ENCODING_UTF8,
                           &characters) != CLM_BPA_SUCCESS) {
        return CLM_CRYPTO_ERROR_INVALID_TEXT;
    }
    return clm_crypto_encrypt_bytes_to(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        text, length, destination_id, tag, tag_capacity);
}

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
 */
static inline clm_int clm_crypto_encrypt_string_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, clm_id destination_id,
    void* tag, size_t tag_capacity)
{
    clm_bpa_block_node* block = NULL;
    const char* text = NULL;
    size_t length = 0u;
    size_t characters = 0u;
    if (source_id == destination_id ||
        clm_string_get_block(source_id, &block, &text,
                             &length, &characters) != CLM_BPA_SUCCESS) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    return clm_crypto_encrypt_bytes_to(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        text, length, destination_id, tag, tag_capacity);
}

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
 */
static inline clm_int clm_crypto_decrypt_bytes_as(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* hex_text, const void* tag, size_t tag_length,
    void* plaintext, size_t plaintext_capacity)
{
    uint8_t* ciphertext;
    size_t bytes = 0u;
    clm_int status;
    size_t hex_length;

    if (hex_text == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    hex_length = strlen(hex_text);
    if ((hex_length & 1u) != 0u) {
        return CLM_CRYPTO_ERROR_INVALID_TEXT;
    }
    bytes = hex_length / 2u;
    if (clm_crypto_private_overlap(plaintext, bytes, hex_text, hex_length + 1u) ||
        clm_crypto_private_overlap(plaintext, bytes, tag, tag_length) ||
        clm_crypto_private_overlap(plaintext, bytes, key, key_length) ||
        clm_crypto_private_overlap(plaintext, bytes, nonce, nonce_length) ||
        clm_crypto_private_overlap(plaintext, bytes, aad, aad_length)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    ciphertext = (uint8_t*)malloc(bytes != 0u ? bytes : 1u);
    if (ciphertext == NULL) {
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }
    status = clm_crypto_hex_decode(hex_text, ciphertext, bytes, NULL);
    if (status == CLM_CRYPTO_SUCCESS) {
        status = clm_crypto_decrypt_bytes(
            algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
            ciphertext, bytes, tag, tag_length, plaintext, plaintext_capacity);
    }
    crypto_wipe(ciphertext, bytes);
    free(ciphertext);
    return status;
}

/** Decrypts ciphertext hex held by a CLM string ID into caller-owned bytes.
 * @param algorithm Supported cipher ID.
 * @param key, key_length Secret key and size.
 * @param nonce, nonce_length Original nonce and size.
 * @param aad, aad_length Original associated data and size.
 * @param source_id Existing CLM hex string ID.
 * @param tag, tag_length Original binary tag and size.
 * @param plaintext, plaintext_capacity Binary output buffer and capacity.
 * @return SUCCESS or a CLM_CRYPTO_ERROR_* result; output unchanged on failure.
 */
static inline clm_int clm_crypto_decrypt_bytes_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, const void* tag, size_t tag_length,
    void* plaintext, size_t plaintext_capacity)
{
    clm_bpa_block_node* block = NULL;
    const char* text = NULL;
    size_t length = 0u;
    size_t characters = 0u;
    if (clm_string_get_block(source_id, &block, &text,
                             &length, &characters) != CLM_BPA_SUCCESS) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    return clm_crypto_decrypt_bytes_as(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        text, tag, tag_length, plaintext, plaintext_capacity);
}

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
 */
static inline clm_int clm_crypto_decrypt_string_to(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* hex_text, const void* tag, size_t tag_length,
    clm_id destination_id)
{
    char* staged = NULL;
    size_t characters = 0u;
    size_t bytes;
    clm_int status;
    const clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);

    if (hex_text == NULL || destination == NULL ||
        destination->type != CLM_BPA_TYPE_STRING) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    const size_t hex_length = strlen(hex_text);
    if ((hex_length & 1u) != 0u || hex_length / 2u == SIZE_MAX) {
        return CLM_CRYPTO_ERROR_INVALID_TEXT;
    }
    bytes = hex_length / 2u;
    if (bytes >= (size_t)INT_MAX) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    staged = (char*)malloc(bytes + 1u);
    if (staged == NULL) {
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }
    status = clm_crypto_decrypt_bytes_as(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        hex_text, tag, tag_length, staged, bytes);
    if (status != CLM_CRYPTO_SUCCESS) {
        goto cleanup;
    }
    if (memchr(staged, '\0', bytes) != NULL ||
        clm_string_measure(staged, bytes, CLM_STRING_ENCODING_UTF8,
                           &characters) != CLM_BPA_SUCCESS) {
        status = CLM_CRYPTO_ERROR_INVALID_TEXT;
        goto cleanup;
    }
    staged[bytes] = '\0';
    if (clm_string_set(destination_id, staged) < 0) {
        status = CLM_CRYPTO_ERROR_BACKEND;
        goto cleanup;
    }
cleanup:
    crypto_wipe(staged, bytes + 1u);
    free(staged);
    return status;
}

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
 */
static inline clm_int clm_crypto_decrypt_string_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, const void* tag, size_t tag_length,
    clm_id destination_id)
{
    clm_bpa_block_node* block = NULL;
    const char* text = NULL;
    size_t length = 0u;
    size_t characters = 0u;
    if (source_id == destination_id ||
        clm_string_get_block(source_id, &block, &text,
                             &length, &characters) != CLM_BPA_SUCCESS) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    return clm_crypto_decrypt_string_to(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        text, tag, tag_length, destination_id);
}

/** Fills an existing BPA fixed-size block with OS cryptographic random bytes.
 * @param destination_id Existing non-string BPA block to fill exactly.
 * @return SUCCESS, INVALID_ARGUMENT, NO_MEMORY, or BACKEND.
 * @note Not a secured memory type: normal BPA storage may retain key material.
 */
static inline clm_int clm_crypto_random_to(clm_id destination_id)
{
    clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    uint8_t* staged;
    clm_int status;
    if (destination == NULL || destination->type == CLM_BPA_TYPE_STRING ||
        destination->data == NULL || destination->block_size == 0u) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    staged = (uint8_t*)malloc(destination->block_size);
    if (staged == NULL) {
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }
    status = clm_crypto_random_bytes(staged, destination->block_size);
    if (status == CLM_CRYPTO_SUCCESS &&
        clm_bpa_set(destination_id, staged, destination->block_size) < 0) {
        status = CLM_CRYPTO_ERROR_BACKEND;
    }
    crypto_wipe(staged, destination->block_size);
    free(staged);
    return status;
}

/** Generates a symmetric key into an existing 32-byte BPA fixed-size block.
 * @param algorithm Supported cipher ID.
 * @param destination_id Pre-existing non-string BPA ID of exact key size.
 * @return SUCCESS or argument/algorithm/RNG failure.
 */
static inline clm_int clm_crypto_key_generate_to(
    clm_crypto_algorithm algorithm, clm_id destination_id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(destination_id);
    if (clm_crypto_key_size(algorithm) == 0u) {
        return CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM;
    }
    if (block == NULL || block->type == CLM_BPA_TYPE_STRING ||
        block->block_size != CLM_CRYPTO_KEY_SIZE) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    return clm_crypto_random_to(destination_id);
}

/** Generates a nonce into an existing 24-byte BPA fixed-size block.
 * @param algorithm Supported cipher ID.
 * @param destination_id Pre-existing non-string BPA ID of exact nonce size.
 * @return SUCCESS or argument/algorithm/RNG failure.
 * @warning Still requires nonce uniqueness under the same encryption key.
 */
static inline clm_int clm_crypto_nonce_generate_to(
    clm_crypto_algorithm algorithm, clm_id destination_id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(destination_id);
    if (clm_crypto_nonce_size(algorithm) == 0u) {
        return CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM;
    }
    if (block == NULL || block->type == CLM_BPA_TYPE_STRING ||
        block->block_size != CLM_CRYPTO_NONCE_SIZE) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    return clm_crypto_random_to(destination_id);
}

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
 */
static inline clm_int clm_crypto_encrypt_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, clm_id destination_id,
    void* tag, size_t tag_capacity)
{
    const clm_bpa_block_node* source = clm_bpa_find_block(source_id);
    const clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    uint8_t* ciphertext;
    uint8_t staged_tag[CLM_CRYPTO_TAG_SIZE];
    clm_int status;

    if (source_id == destination_id || source == NULL || destination == NULL ||
        tag == NULL || tag_capacity < CLM_CRYPTO_TAG_SIZE ||
        source->type == CLM_BPA_TYPE_STRING ||
        destination->type == CLM_BPA_TYPE_STRING ||
        source->data == NULL || destination->data == NULL ||
        source->block_size != destination->block_size ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, key, key_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, nonce, nonce_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE, aad, aad_length) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE,
                                   source->data, source->block_size) ||
        clm_crypto_private_overlap(tag, CLM_CRYPTO_TAG_SIZE,
                                   destination->data, destination->block_size)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    ciphertext = (uint8_t*)malloc(source->block_size != 0u ? source->block_size : 1u);
    if (ciphertext == NULL) {
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }
    status = clm_crypto_encrypt_bytes(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        source->data, source->block_size,
        ciphertext, source->block_size, staged_tag, sizeof(staged_tag));
    if (status == CLM_CRYPTO_SUCCESS &&
        clm_bpa_set(destination_id, ciphertext, destination->block_size) < 0) {
        status = CLM_CRYPTO_ERROR_BACKEND;
    }
    if (status == CLM_CRYPTO_SUCCESS) {
        memcpy(tag, staged_tag, CLM_CRYPTO_TAG_SIZE);
    }
    crypto_wipe(staged_tag, sizeof(staged_tag));
    crypto_wipe(ciphertext, source->block_size);
    free(ciphertext);
    return status;
}

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
 */
static inline clm_int clm_crypto_decrypt_from(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    clm_id source_id, const void* tag, size_t tag_length,
    clm_id destination_id)
{
    const clm_bpa_block_node* source = clm_bpa_find_block(source_id);
    const clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    uint8_t* plaintext;
    clm_int status;

    if (source_id == destination_id || source == NULL || destination == NULL ||
        source->type == CLM_BPA_TYPE_STRING ||
        destination->type == CLM_BPA_TYPE_STRING ||
        source->data == NULL || destination->data == NULL ||
        source->block_size != destination->block_size) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    plaintext = (uint8_t*)malloc(source->block_size != 0u ? source->block_size : 1u);
    if (plaintext == NULL) {
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }
    status = clm_crypto_decrypt_bytes(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        source->data, source->block_size, tag, tag_length,
        plaintext, source->block_size);
    if (status == CLM_CRYPTO_SUCCESS &&
        clm_bpa_set(destination_id, plaintext, destination->block_size) < 0) {
        status = CLM_CRYPTO_ERROR_BACKEND;
    }
    crypto_wipe(plaintext, source->block_size);
    free(plaintext);
    return status;
}

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
static inline clm_int clm_crypto_derive_key_bytes(
    const void* password, size_t password_length,
    const void* salt, size_t salt_length,
    uint32_t memory_kib, uint32_t passes,
    void* output, size_t output_length)
{
    crypto_argon2_config config;
    crypto_argon2_inputs inputs;
    uint8_t* work_area;
    uint8_t* derived;
    size_t work_size;
    static const uint8_t empty_byte = 0u;
    clm_int status = CLM_CRYPTO_SUCCESS;

    if ((password_length != 0u && password == NULL) || salt == NULL ||
        salt_length < 16u || salt_length > UINT32_MAX ||
        password_length > UINT32_MAX || output == NULL ||
        output_length < 4u || output_length > UINT32_MAX ||
        memory_kib < 8u || passes < 1u ||
        ((uint64_t)memory_kib * UINT64_C(1024)) > (uint64_t)SIZE_MAX) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    work_size = (size_t)memory_kib * 1024u;
    work_area = (uint8_t*)malloc(work_size);
    if (work_area == NULL) {
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }
    derived = (uint8_t*)malloc(output_length);
    if (derived == NULL) {
        crypto_wipe(work_area, work_size);
        free(work_area);
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }
    config.algorithm = CRYPTO_ARGON2_ID;
    config.nb_blocks = memory_kib;
    config.nb_passes = passes;
    config.nb_lanes = 1u;
    inputs.pass = password != NULL ? (const uint8_t*)password : &empty_byte;
    inputs.salt = (const uint8_t*)salt;
    inputs.pass_size = (uint32_t)password_length;
    inputs.salt_size = (uint32_t)salt_length;

    crypto_argon2(derived, (uint32_t)output_length, work_area,
                  config, inputs, crypto_argon2_no_extras);
    memcpy(output, derived, output_length);
    crypto_wipe(derived, output_length);
    crypto_wipe(work_area, work_size);
    free(derived);
    free(work_area);
    return status;
}

/** Derives Argon2id key bytes from a NUL-terminated password C string.
 * @param password Borrowed NUL-terminated password; embedded NUL is excluded.
 * @param salt, salt_length Random salt and size, at least 16 bytes.
 * @param memory_kib Memory cost in KiB, at least eight.
 * @param passes Positive work-factor pass count.
 * @param output, output_length Caller-owned derived-key buffer and exact length.
 * @return Same statuses as clm_crypto_derive_key_bytes().
 */
static inline clm_int clm_crypto_derive_key_as(
    const char* password, const void* salt, size_t salt_length,
    uint32_t memory_kib, uint32_t passes,
    void* output, size_t output_length)
{
    if (password == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    return clm_crypto_derive_key_bytes(
        password, strlen(password), salt, salt_length,
        memory_kib, passes, output, output_length);
}

/** Derives Argon2id key bytes from an existing CLM UTF-8 password string ID.
 * @param password_id Existing CLM UTF-8 string ID; read as raw UTF-8 bytes.
 * @param salt, salt_length Random salt and size.
 * @param memory_kib Work-memory cost in KiB.
 * @param passes CPU work factor.
 * @param output, output_length Derived-key destination and length.
 * @return Same statuses as clm_crypto_derive_key_bytes().
 */
static inline clm_int clm_crypto_derive_key_from(
    clm_id password_id, const void* salt, size_t salt_length,
    uint32_t memory_kib, uint32_t passes,
    void* output, size_t output_length)
{
    clm_bpa_block_node* block = NULL;
    const char* text = NULL;
    size_t length = 0u;
    size_t characters = 0u;
    if (clm_string_get_block(password_id, &block, &text,
                             &length, &characters) != CLM_BPA_SUCCESS) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    return clm_crypto_derive_key_bytes(
        text, length, salt, salt_length, memory_kib, passes,
        output, output_length);
}

/** Generates X25519 private/public key pair using OS randomness and Monocypher.
 * @param secret_key Writable 32-byte private-key output.
 * @param secret_capacity Secret-key destination capacity in bytes.
 * @param public_key Writable 32-byte public-key output.
 * @param public_capacity Public-key destination capacity.
 * @return SUCCESS, BUFFER_TOO_SMALL, INVALID_ARGUMENT, or RNG failure.
 * @note Caller owns the private key and must securely wipe it after use.
 */
static inline clm_int clm_crypto_x25519_key_pair(
    void* secret_key, size_t secret_capacity,
    void* public_key, size_t public_capacity)
{
    uint8_t secret[32];
    uint8_t public_bytes[32];
    clm_int status;
    if (secret_key == NULL || public_key == NULL ||
        clm_crypto_private_overlap(secret_key, 32u, public_key, 32u)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (secret_capacity < 32u || public_capacity < 32u) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    status = clm_crypto_random_bytes(secret, sizeof(secret));
    if (status != CLM_CRYPTO_SUCCESS) {
        return status;
    }
    crypto_x25519_public_key(public_bytes, secret);
    memcpy(secret_key, secret, sizeof(secret));
    memcpy(public_key, public_bytes, sizeof(public_bytes));
    crypto_wipe(secret, sizeof(secret));
    return CLM_CRYPTO_SUCCESS;
}

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
static inline clm_int clm_crypto_x25519_shared(
    const void* secret_key, const void* peer_public_key,
    void* output, size_t capacity)
{
    uint8_t result[32];
    uint8_t aggregate = 0u;
    if (secret_key == NULL || peer_public_key == NULL || output == NULL ||
        clm_crypto_private_overlap(output, 32u, secret_key, 32u) ||
        clm_crypto_private_overlap(output, 32u, peer_public_key, 32u)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (capacity < 32u) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    crypto_x25519(result, (const uint8_t*)secret_key,
                  (const uint8_t*)peer_public_key);
    for (size_t i = 0u; i < sizeof(result); ++i) {
        aggregate |= result[i];
    }
    if (aggregate == 0u) {
        crypto_wipe(result, sizeof(result));
        return CLM_CRYPTO_ERROR_AUTHENTICATION;
    }
    memcpy(output, result, sizeof(result));
    crypto_wipe(result, sizeof(result));
    return CLM_CRYPTO_SUCCESS;
}

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
 */
static inline clm_int clm_crypto_encrypt_string_as(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* text, char* out_hex, size_t hex_capacity,
    void* tag, size_t tag_capacity)
{
    uint8_t* ciphertext = NULL;
    char* hex = NULL;
    uint8_t staged_tag[CLM_CRYPTO_TAG_SIZE];
    size_t plaintext_length;
    size_t character_count;
    size_t required;
    clm_int status;

    if (text == NULL || out_hex == NULL || tag == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    plaintext_length = strlen(text);
    required = clm_crypto_hex_size(plaintext_length);
    if (required == 0u) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (hex_capacity < required || tag_capacity < sizeof(staged_tag)) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    if (clm_crypto_private_overlap(out_hex, required, text, plaintext_length + 1u) ||
        clm_crypto_private_overlap(out_hex, required, key, key_length) ||
        clm_crypto_private_overlap(out_hex, required, nonce, nonce_length) ||
        clm_crypto_private_overlap(out_hex, required, aad, aad_length) ||
        clm_crypto_private_overlap(tag, sizeof(staged_tag), text, plaintext_length + 1u) ||
        clm_crypto_private_overlap(tag, sizeof(staged_tag), out_hex, required) ||
        clm_crypto_private_overlap(tag, sizeof(staged_tag), key, key_length) ||
        clm_crypto_private_overlap(tag, sizeof(staged_tag), nonce, nonce_length) ||
        clm_crypto_private_overlap(tag, sizeof(staged_tag), aad, aad_length)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    if (clm_string_measure(text, plaintext_length, CLM_STRING_ENCODING_UTF8,
                           &character_count) != CLM_BPA_SUCCESS) {
        return CLM_CRYPTO_ERROR_INVALID_TEXT;
    }
    ciphertext = (uint8_t*)malloc(plaintext_length != 0u ? plaintext_length : 1u);
    hex = (char*)malloc(required);
    if (ciphertext == NULL || hex == NULL) {
        status = CLM_CRYPTO_ERROR_NO_MEMORY;
        goto cleanup;
    }
    status = clm_crypto_encrypt_bytes(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        text, plaintext_length, ciphertext, plaintext_length,
        staged_tag, sizeof(staged_tag));
    if (status == CLM_CRYPTO_SUCCESS) {
        status = clm_crypto_hex_encode(ciphertext, plaintext_length, hex, required);
    }
    if (status == CLM_CRYPTO_SUCCESS) {
        memcpy(out_hex, hex, required);
        memcpy(tag, staged_tag, sizeof(staged_tag));
    }
cleanup:
    crypto_wipe(staged_tag, sizeof(staged_tag));
    if (ciphertext != NULL) {
        crypto_wipe(ciphertext, plaintext_length);
        free(ciphertext);
    }
    if (hex != NULL) {
        crypto_wipe(hex, required);
        free(hex);
    }
    return status;
}

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
 */
static inline clm_int clm_crypto_decrypt_string_as(
    clm_crypto_algorithm algorithm,
    const void* key, size_t key_length,
    const void* nonce, size_t nonce_length,
    const void* aad, size_t aad_length,
    const char* hex_text, const void* tag, size_t tag_length,
    char* plaintext, size_t plaintext_capacity)
{
    char* staged;
    size_t length;
    size_t characters = 0u;
    clm_int status;

    if (hex_text == NULL || plaintext == NULL) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    length = strlen(hex_text);
    if ((length & 1u) != 0u) {
        return CLM_CRYPTO_ERROR_INVALID_TEXT;
    }
    length /= 2u;
    if (length == SIZE_MAX || plaintext_capacity < length + 1u) {
        return CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL;
    }
    if (clm_crypto_private_overlap(plaintext, length + 1u, hex_text, length * 2u + 1u) ||
        clm_crypto_private_overlap(plaintext, length + 1u, key, key_length) ||
        clm_crypto_private_overlap(plaintext, length + 1u, nonce, nonce_length) ||
        clm_crypto_private_overlap(plaintext, length + 1u, aad, aad_length) ||
        clm_crypto_private_overlap(plaintext, length + 1u, tag, tag_length)) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    staged = (char*)malloc(length + 1u);
    if (staged == NULL) {
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }
    status = clm_crypto_decrypt_bytes_as(
        algorithm, key, key_length, nonce, nonce_length, aad, aad_length,
        hex_text, tag, tag_length, staged, length);
    if (status == CLM_CRYPTO_SUCCESS) {
        if (memchr(staged, '\0', length) != NULL ||
            clm_string_measure(staged, length, CLM_STRING_ENCODING_UTF8,
                               &characters) != CLM_BPA_SUCCESS) {
            status = CLM_CRYPTO_ERROR_INVALID_TEXT;
        } else {
            staged[length] = '\0';
            memcpy(plaintext, staged, length + 1u);
        }
    }
    crypto_wipe(staged, length + 1u);
    free(staged);
    return status;
}

/** Derives a key from password bytes into an existing fixed-size BPA block.
 * @param password, password_length Borrowed password bytes and size.
 * @param salt, salt_length Unpredictable salt and its size, at least 16 bytes.
 * @param memory_kib Argon2id memory cost in KiB.
 * @param passes Argon2id pass count.
 * @param destination_id Existing non-string BPA block whose size is the
 *                       desired derived-key size, at least four bytes.
 * @return SUCCESS or argument/memory/backend error; ID is not created.
 * @note BPA is ordinary memory: this does not secure derived key storage.
 */
static inline clm_int clm_crypto_derive_key_to(
    const void* password, size_t password_length,
    const void* salt, size_t salt_length,
    uint32_t memory_kib, uint32_t passes,
    clm_id destination_id)
{
    const clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    uint8_t* derived;
    clm_int status;
    if (destination == NULL || destination->type == CLM_BPA_TYPE_STRING ||
        destination->data == NULL || destination->block_size < 4u) {
        return CLM_CRYPTO_ERROR_INVALID_ARGUMENT;
    }
    derived = (uint8_t*)malloc(destination->block_size);
    if (derived == NULL) {
        return CLM_CRYPTO_ERROR_NO_MEMORY;
    }
    status = clm_crypto_derive_key_bytes(
        password, password_length, salt, salt_length, memory_kib, passes,
        derived, destination->block_size);
    if (status == CLM_CRYPTO_SUCCESS &&
        clm_bpa_set(destination_id, derived, destination->block_size) < 0) {
        status = CLM_CRYPTO_ERROR_BACKEND;
    }
    crypto_wipe(derived, destination->block_size);
    free(derived);
    return status;
}

#if defined(CLM_CRYPTO_IMPLEMENTATION)
/* Compile exactly once. Core Monocypher is C99 and its upstream files remain
 * verbatim in clm/ext; do not compile the .c separately as well. */
#include "ext/monocypher.c"
#endif
