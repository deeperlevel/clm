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

#include "hash.h"

const struct lfh_hash_descriptor* clm_hash_private_backend_descriptor(
    const clm_hash_algorithm algorithm) {
    switch (algorithm) {
        case CLM_HASH_ALGORITHM_MD2: return &md2_desc;
        case CLM_HASH_ALGORITHM_MD4: return &md4_desc;
        case CLM_HASH_ALGORITHM_MD5: return &md5_desc;
        case CLM_HASH_ALGORITHM_SHA1: return &sha1_desc;
        case CLM_HASH_ALGORITHM_SHA224: return &sha224_desc;
        case CLM_HASH_ALGORITHM_SHA256: return &sha256_desc;
        case CLM_HASH_ALGORITHM_SHA384: return &sha384_desc;
        case CLM_HASH_ALGORITHM_SHA512: return &sha512_desc;
        case CLM_HASH_ALGORITHM_SHA512_224: return &sha512_224_desc;
        case CLM_HASH_ALGORITHM_SHA512_256: return &sha512_256_desc;
        case CLM_HASH_ALGORITHM_SHA3_224: return &sha3_224_desc;
        case CLM_HASH_ALGORITHM_SHA3_256: return &sha3_256_desc;
        case CLM_HASH_ALGORITHM_SHA3_384: return &sha3_384_desc;
        case CLM_HASH_ALGORITHM_SHA3_512: return &sha3_512_desc;
        default: return NULL;
    }
}

clm_int clm_hash_private_backend_status(const int status) {
    if (status == CRYPT_OK)
        return CLM_HASH_SUCCESS;
    return status == CRYPT_MEM
        ? CLM_HASH_ERROR_NO_MEMORY : CLM_HASH_ERROR_BACKEND;
}

size_t clm_hash_digest_size(clm_hash_algorithm algorithm) {
    switch (algorithm) {
        case CLM_HASH_ALGORITHM_MD2:
        case CLM_HASH_ALGORITHM_MD4:
        case CLM_HASH_ALGORITHM_MD5:
            return 16u;

        case CLM_HASH_ALGORITHM_SHA1:
            return 20u;

        case CLM_HASH_ALGORITHM_SHA224:
        case CLM_HASH_ALGORITHM_SHA512_224:
        case CLM_HASH_ALGORITHM_SHA3_224:
            return 28u;

        case CLM_HASH_ALGORITHM_SHA256:
        case CLM_HASH_ALGORITHM_SHA512_256:
        case CLM_HASH_ALGORITHM_SHA3_256:
            return 32u;

        case CLM_HASH_ALGORITHM_SHA384:
        case CLM_HASH_ALGORITHM_SHA3_384:
            return 48u;

        case CLM_HASH_ALGORITHM_SHA512:
        case CLM_HASH_ALGORITHM_SHA3_512:
            return 64u;

        default:
            return 0u;
    }
}

size_t clm_hash_hex_size(clm_hash_algorithm algorithm) {
    const size_t digest_bytes = clm_hash_digest_size(algorithm);

    if (digest_bytes == 0u) {
        return 0u;
    }

    return digest_bytes * 2u + 1u;
}

clm_int clm_hash_algorithm_name(clm_hash_algorithm algorithm, clm_id dest) {
    const char* name = "";
    switch (algorithm) {
        case CLM_HASH_ALGORITHM_MD2: 
            name = "MD2";
            break;
        case CLM_HASH_ALGORITHM_MD4: 
            name = "MD4";
            break;
        case CLM_HASH_ALGORITHM_MD5: 
            name = "MD5";
            break;
        case CLM_HASH_ALGORITHM_SHA1: 
            name = "SHA1";
            break;
        case CLM_HASH_ALGORITHM_SHA224: 
            name = "SHA2-224";
            break;
        case CLM_HASH_ALGORITHM_SHA256: 
            name = "SHA2-256";
            break;
        case CLM_HASH_ALGORITHM_SHA384: 
            name = "SHA2-384";
            break;
        case CLM_HASH_ALGORITHM_SHA512: 
            name = "SHA2-512";
            break;
        case CLM_HASH_ALGORITHM_SHA512_224: 
            name = "SHA2-512/224";
            break;
        case CLM_HASH_ALGORITHM_SHA512_256: 
            name = "SHA2-512/256";
            break;
        case CLM_HASH_ALGORITHM_SHA3_224:
            name = "SHA3-224";
            break;
        case CLM_HASH_ALGORITHM_SHA3_256:
            name = "SHA3-256";
            break;
        case CLM_HASH_ALGORITHM_SHA3_384:
            name = "SHA3-384";
            break;
        case CLM_HASH_ALGORITHM_SHA3_512:
            name = "SHA3-512";
            break;
        default: 
            return CLM_HASH_ERROR_UNSUPPORTED_ALGORITHM;
    }

    return clm_string_set(dest, name) < 0
        ? CLM_HASH_ERROR_INVALID_ARGUMENT : CLM_HASH_SUCCESS;
}

clm_int clm_hash_hex_encode(
    const void* bytes,
    size_t length,
    char* out_hex,
    size_t capacity) {
    static const char digits[] = "0123456789abcdef";
    const unsigned char* input = (const unsigned char*)bytes;

    if (out_hex == NULL || (length != 0u && bytes == NULL)) {
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    }

    if (length > (SIZE_MAX - 1u) / 2u) {
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    }

    size_t required = length * 2u + 1u;
    if (capacity < required) {
        return CLM_HASH_ERROR_BUFFER_TOO_SMALL;
    }

    for (size_t index = 0; index < length; ++index) {
        out_hex[index * 2u] = digits[input[index] >> 4u];
        out_hex[index * 2u + 1u] = digits[input[index] & 0x0fu];
    }

    out_hex[length * 2u] = '\0';
    return CLM_HASH_SUCCESS;
}

clm_int clm_hash_bytes(
    const clm_hash_algorithm algorithm,
    const void* bytes,
    const size_t length,
    void* out_digest,
    const size_t capacity) {
    const size_t required = clm_hash_digest_size(algorithm);
    const struct lfh_hash_descriptor* descriptor =
        clm_hash_private_backend_descriptor(algorithm);
    hash_state state;
    unsigned char digest[CLM_HASH_MAX_DIGEST_SIZE];
    static const unsigned char empty_input = 0;
    const unsigned char* input = bytes != NULL
        ? (const unsigned char*)bytes : &empty_input;
    size_t offset = 0;

    if ((length != 0u && bytes == NULL) || out_digest == NULL)
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    if (required == 0u)
        return CLM_HASH_ERROR_UNSUPPORTED_ALGORITHM;
    if (capacity < required)
        return CLM_HASH_ERROR_BUFFER_TOO_SMALL;
    if (descriptor == NULL || descriptor->hashsize != required ||
        descriptor->init == NULL || descriptor->process == NULL ||
        descriptor->done == NULL)
        return CLM_HASH_ERROR_BACKEND;
    clm_int status = clm_hash_private_backend_status(descriptor->init(&state));
    if (status != CLM_HASH_SUCCESS)
        return status;

    do {
        const size_t remaining = length - offset;
        const unsigned long chunk = remaining > (size_t)ULONG_MAX
            ? ULONG_MAX : (unsigned long)remaining;
        status = clm_hash_private_backend_status(
            descriptor->process(&state, input + offset, chunk));
        if (status != CLM_HASH_SUCCESS)
            return status;
        offset += chunk;
    } while (offset < length);

    status = clm_hash_private_backend_status(descriptor->done(&state, digest));
    if (status != CLM_HASH_SUCCESS)
        return status;
    memcpy(out_digest, digest, required);
    return CLM_HASH_SUCCESS;
}

clm_int clm_hash_bytes_as(
    const clm_hash_algorithm algorithm,
    const char* text,
    void* out_digest,
    const size_t capacity) {
    if (text == NULL)
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    return clm_hash_bytes(algorithm, text, strlen(text), out_digest, capacity);
}

clm_int clm_hash_bytes_from(
    const clm_hash_algorithm algorithm,
    const clm_id source_id,
    void* out_digest,
    const size_t capacity) {
    clm_bpa_block_node* block;
    const char* text;
    size_t byte_length, character_count;
    if (clm_string_get_block(
            source_id, &block, &text, &byte_length, &character_count) != CLM_BPA_SUCCESS)
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    return clm_hash_bytes(algorithm, text, byte_length, out_digest, capacity);
}

clm_int clm_hash_write_string(
    const clm_hash_algorithm algorithm,
    const void* bytes,
    const size_t length,
    const clm_id destination_id) {
    clm_bpa_block_node* destination;
    const char* destination_text;
    size_t destination_length, destination_characters;
    unsigned char digest[CLM_HASH_MAX_DIGEST_SIZE];
    char hex[CLM_HASH_MAX_DIGEST_SIZE * 2u + 1u];
    const size_t digest_size = clm_hash_digest_size(algorithm);

    if (clm_string_get_block(
            destination_id, &destination, &destination_text,
            &destination_length, &destination_characters) != CLM_BPA_SUCCESS)
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    if (digest_size == 0u)
        return CLM_HASH_ERROR_UNSUPPORTED_ALGORITHM;
    const clm_int status = clm_hash_bytes(
        algorithm, bytes, length, digest, sizeof(digest));
    if (status != CLM_HASH_SUCCESS)
        return status;
    const clm_int encoded = clm_hash_hex_encode(
        digest, digest_size, hex, sizeof(hex));
    if (encoded != CLM_HASH_SUCCESS)
        return encoded;
    return clm_string_set(destination_id, hex) < 0
        ? CLM_HASH_ERROR_INVALID_ARGUMENT : CLM_HASH_SUCCESS;
}

clm_int clm_hash_bytes_to(
    const clm_hash_algorithm algorithm,
    const void* bytes,
    const size_t length,
    const clm_id destination_id) {
    return clm_hash_write_string(algorithm, bytes, length, destination_id);
}

clm_int clm_hash_string_to(
    const clm_hash_algorithm algorithm,
    const char* text,
    const clm_id destination_id) {
    const size_t byte_length = text != NULL ? strlen(text) : 0u;
    size_t character_count;
    if (text == NULL)
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    if (clm_string_measure(
            text, byte_length, CLM_STRING_ENCODING_UTF8,
            &character_count) != CLM_BPA_SUCCESS)
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    return clm_hash_write_string(
        algorithm, text, byte_length, destination_id);
}

clm_int clm_hash_string_from(
    const clm_hash_algorithm algorithm,
    const clm_id source_id,
    const clm_id destination_id) {
    clm_bpa_block_node* source;
    const char* text;
    size_t byte_length, character_count;
    if (clm_string_get_block(
            source_id, &source, &text, &byte_length, &character_count) != CLM_BPA_SUCCESS)
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    return clm_hash_write_string(
        algorithm, text, byte_length, destination_id);
}

clm_int clm_hash_string_as(
    const clm_hash_algorithm algorithm,
    const char* text,
    char* out_hex,
    const size_t capacity) {
    size_t character_count;
    unsigned char digest[CLM_HASH_MAX_DIGEST_SIZE];
    const size_t digest_size = clm_hash_digest_size(algorithm);

    if (text == NULL || out_hex == NULL)
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    if (digest_size == 0u)
        return CLM_HASH_ERROR_UNSUPPORTED_ALGORITHM;
    if (capacity < digest_size * 2u + 1u)
        return CLM_HASH_ERROR_BUFFER_TOO_SMALL;
    size_t byte_length = strlen(text);
    if (clm_string_measure(
            text, byte_length, CLM_STRING_ENCODING_UTF8,
            &character_count) != CLM_BPA_SUCCESS)
        return CLM_HASH_ERROR_INVALID_ARGUMENT;
    const clm_int status = clm_hash_bytes(
        algorithm, text, byte_length, digest, sizeof(digest));
    if (status != CLM_HASH_SUCCESS)
        return status;
    return clm_hash_hex_encode(
        digest, digest_size, out_hex, capacity);
}
