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

#include "base.h"
#include "bpa.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#endif

#define GENERATION_SUCCESS 0
#define GENERATION_FAILURE 1

#define GENERATION_CASING_MIXED 0
#define GENERATION_CASING_UPPER 1
#define GENERATION_CASING_LOWER 2

static inline int clm_gen_internal_bytes(void* buffer, size_t length)
{
#if defined(_WIN32)
    return BCryptGenRandom(NULL, (PUCHAR)buffer, (ULONG)length,
                           BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
    FILE* stream = fopen("/dev/urandom", "rb");
    if (stream == NULL)
        return 0;
    const int success = fread(buffer, 1, length, stream) == length;
    fclose(stream);
    return success;
#endif
}

static inline int clm_gen_internal_bounded(uint64_t bound, uint64_t* value)
{
    const uint64_t threshold = bound == 0 ? 0 : (UINT64_C(0) - bound) % bound;
    do {
        if (!clm_gen_internal_bytes(value, sizeof(*value)))
            return 0;
    } while (*value < threshold);
    if (bound != 0)
        *value %= bound;
    return 1;
}

static inline clm_ulong clm_gen_random_u64(clm_ulong min, clm_ulong max)
{
    uint64_t value;
    if (min > max) {
        const clm_ulong temporary = min;
        min = max;
        max = temporary;
    }
    if (min == max)
        return min;
    if (!clm_gen_internal_bounded(max - min + UINT64_C(1), &value))
        abort();
    return min + value;
}

static inline clm_long clm_gen_random_i64(clm_long min, clm_long max)
{
    const uint64_t sign = UINT64_C(1) << 63;
    const uint64_t value = clm_gen_random_u64((uint64_t)min ^ sign,
                                               (uint64_t)max ^ sign);
    return value < sign ? LONG_MIN + (clm_long)value : (clm_long)(value - sign);
}

static inline clm_sbyte clm_gen_random_i8(clm_sbyte min, clm_sbyte max)
{
    return (clm_sbyte)clm_gen_random_i64(min, max);
}

static inline clm_byte clm_gen_random_u8(clm_byte min, clm_byte max)
{
    return (clm_byte)clm_gen_random_u64(min, max);
}

static inline clm_short clm_gen_random_i16(clm_short min, clm_short max)
{
    return (clm_short)clm_gen_random_i64(min, max);
}

static inline clm_ushort clm_gen_random_u16(clm_ushort min, clm_ushort max)
{
    return (clm_ushort)clm_gen_random_u64(min, max);
}

static inline clm_int clm_gen_random_i32(clm_int min, clm_int max)
{
    return (clm_int)clm_gen_random_i64(min, max);
}

static inline clm_uint clm_gen_random_u32(clm_uint min, clm_uint max)
{
    return (clm_uint)clm_gen_random_u64(min, max);
}

static inline clm_double clm_gen_random_f64(clm_double min, clm_double max)
{
    if (!isfinite(min) || !isfinite(max))
        return NAN;
    if (min > max) {
        const clm_double temporary = min;
        min = max;
        max = temporary;
    }
    if (min == max)
        return min;
    const double unit = (double)(clm_gen_random_u64(ULONG_MIN, ULONG_MAX) >> 11) /
                        9007199254740991.0;
    const double value = min < 0 && max > 0
        ? min * (1.0 - unit) + max * unit
        : min + (max - min) * unit;
    return value < min ? min : (value > max ? max : value);
}

static inline clm_float clm_gen_random_f32(clm_float min, clm_float max)
{
    return (clm_float)clm_gen_random_f64(min, max);
}

static inline int clm_gen_internal_destination(clm_id destination)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(destination);
    return block != NULL && block->type == CLM_BPA_TYPE_STRING;
}

static inline int clm_gen_internal_casing(clm_int casing)
{
    return casing >= GENERATION_CASING_MIXED && casing <= GENERATION_CASING_LOWER;
}

static inline int clm_gen_internal_character(const char* alphabet, char* output)
{
    uint64_t index;
    if (!clm_gen_internal_bounded((uint64_t)strlen(alphabet), &index))
        return 0;
    *output = alphabet[index];
    return 1;
}

static inline int clm_gen_internal_text_to(clm_int length, const char* alphabet,
                                           int mixed_hex, void* buffer,
                                           clm_uint buffer_size)
{
    if (length < 0 || buffer == NULL ||
        (uint64_t)length + 1 > buffer_size ||
        (uint64_t)length + 1 > (size_t)-1)
        return GENERATION_FAILURE;
    char* text = (char*)buffer;
    for (clm_int index = 0; index < length; ++index) {
        if (!clm_gen_internal_character(alphabet, &text[index]))
            return GENERATION_FAILURE;
        if (mixed_hex && text[index] >= 'a' && text[index] <= 'f') {
            uint64_t upper;
            if (!clm_gen_internal_bounded(2, &upper))
                return GENERATION_FAILURE;
            if (upper)
                text[index] = (char)(text[index] - 'a' + 'A');
        }
    }
    text[length] = '\0';
    return GENERATION_SUCCESS;
}

static inline int clm_gen_internal_text(clm_int length, const char* alphabet,
                                        int mixed_hex, clm_id destination)
{
    if (length < 0 || !clm_gen_internal_destination(destination) ||
        (uint64_t)length + 1 > (size_t)-1)
        return GENERATION_FAILURE;
    char* text = (char*)malloc((size_t)length + 1);
    if (text == NULL)
        return GENERATION_FAILURE;
    const int success = clm_gen_internal_text_to(length, alphabet, mixed_hex,
                                                  text, (clm_uint)length + 1) ==
                            GENERATION_SUCCESS &&
        clm_bpa_set_string(destination, text, (clm_uint)length + 1) == length;
    free(text);
    return success ? GENERATION_SUCCESS : GENERATION_FAILURE;
}

static inline clm_int clm_gen_hex_to(clm_int length, clm_int casing,
                                     void* buffer, clm_uint buffer_size)
{
    if (!clm_gen_internal_casing(casing))
        return GENERATION_FAILURE;
    return clm_gen_internal_text_to(length,
        casing == GENERATION_CASING_UPPER ? "0123456789ABCDEF" :
        "0123456789abcdef", casing == GENERATION_CASING_MIXED, buffer, buffer_size);
}

static inline clm_int clm_gen_hex(clm_int length, clm_int casing, clm_id destination)
{
    if (!clm_gen_internal_casing(casing))
        return GENERATION_FAILURE;
    return clm_gen_internal_text(length,
        casing == GENERATION_CASING_UPPER ? "0123456789ABCDEF" :
        "0123456789abcdef", casing == GENERATION_CASING_MIXED, destination);
}

static inline clm_int clm_gen_uuid_to(clm_int casing, void* buffer,
                                      clm_uint buffer_size)
{
    unsigned char bytes[16];
    char text[37];
    if (!clm_gen_internal_casing(casing) ||
        buffer == NULL || buffer_size < sizeof(text) ||
        !clm_gen_internal_bytes(bytes, sizeof(bytes)))
        return GENERATION_FAILURE;
    bytes[6] = (unsigned char)((bytes[6] & 0x0f) | 0x40);
    bytes[8] = (unsigned char)((bytes[8] & 0x3f) | 0x80);
    size_t position = 0;
    for (size_t index = 0; index < sizeof(bytes); ++index) {
        if (index == 4 || index == 6 || index == 8 || index == 10)
            text[position++] = '-';
        for (int shift = 4; shift >= 0; shift -= 4) {
            char character = "0123456789abcdef"[(bytes[index] >> shift) & 0x0f];
            if (character >= 'a' && casing != GENERATION_CASING_LOWER) {
                uint64_t upper = 1;
                if (casing == GENERATION_CASING_MIXED &&
                    !clm_gen_internal_bounded(2, &upper))
                    return GENERATION_FAILURE;
                if (upper)
                    character = (char)(character - 'a' + 'A');
            }
            text[position++] = character;
        }
    }
    text[position] = '\0';
    memcpy(buffer, text, sizeof(text));
    return GENERATION_SUCCESS;
}

static inline clm_int clm_gen_uuid(clm_int casing, clm_id destination)
{
    char text[37];
    if (!clm_gen_internal_destination(destination) ||
        clm_gen_uuid_to(casing, text, sizeof(text)) != GENERATION_SUCCESS)
        return GENERATION_FAILURE;
    return clm_bpa_set_string(destination, text, sizeof(text)) == 36
        ? GENERATION_SUCCESS : GENERATION_FAILURE;
}

static inline clm_int clm_gen_alphanum_to(clm_int length, clm_int casing,
                                          void* buffer, clm_uint buffer_size)
{
    if (!clm_gen_internal_casing(casing))
        return GENERATION_FAILURE;
    return clm_gen_internal_text_to(length,
        casing == GENERATION_CASING_UPPER ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" :
        casing == GENERATION_CASING_LOWER ? "0123456789abcdefghijklmnopqrstuvwxyz" :
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 0, buffer, buffer_size);
}

static inline clm_int clm_gen_alphanum(clm_int length, clm_int casing,
                                       clm_id destination)
{
    if (!clm_gen_internal_casing(casing))
        return GENERATION_FAILURE;
    return clm_gen_internal_text(length,
        casing == GENERATION_CASING_UPPER ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" :
        casing == GENERATION_CASING_LOWER ? "0123456789abcdefghijklmnopqrstuvwxyz" :
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", 0, destination);
}

static inline clm_int clm_gen_strong_password_to(clm_int length, void* buffer,
                                                 clm_uint buffer_size)
{
    static const char* const groups[] = {
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "abcdefghijklmnopqrstuvwxyz",
        "0123456789", "!@#$%^&*()-_=+[]{};:,.?/|~"
    };
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
                           "0123456789!@#$%^&*()-_=+[]{};:,.?/|~";
    if (length < 4 || buffer == NULL ||
        (uint64_t)length + 1 > buffer_size ||
        (uint64_t)length + 1 > (size_t)-1)
        return GENERATION_FAILURE;
    char* text = (char*)buffer;
    int success = 1;
    for (clm_int index = 0; index < length && success; ++index)
        success = clm_gen_internal_character(index < 4 ? groups[index] : alphabet,
                                              &text[index]);
    for (clm_int index = length - 1; index > 0 && success; --index) {
        uint64_t swap_index;
        success = clm_gen_internal_bounded((uint64_t)index + 1, &swap_index);
        if (success) {
            const char temporary = text[index];
            text[index] = text[swap_index];
            text[swap_index] = temporary;
        }
    }
    text[length] = '\0';
    return success ? GENERATION_SUCCESS : GENERATION_FAILURE;
}

static inline clm_int clm_gen_strong_password(clm_int length, clm_id destination)
{
    if (length < 4 || !clm_gen_internal_destination(destination) ||
        (uint64_t)length + 1 > (size_t)-1)
        return GENERATION_FAILURE;
    char* text = (char*)malloc((size_t)length + 1);
    if (text == NULL)
        return GENERATION_FAILURE;
    const int success = clm_gen_strong_password_to(length, text, (clm_uint)length + 1) ==
                            GENERATION_SUCCESS &&
        clm_bpa_set_string(destination, text, (clm_uint)length + 1) == length;
    free(text);
    return success ? GENERATION_SUCCESS : GENERATION_FAILURE;
}