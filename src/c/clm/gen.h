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

int clm_gen_internal_bytes(void* buffer, size_t length);

int clm_gen_internal_bounded(uint64_t bound, uint64_t* value);

clm_ulong clm_gen_random_u64(clm_ulong min, clm_ulong max);

clm_long clm_gen_random_i64(clm_long min, clm_long max);

clm_sbyte clm_gen_random_i8(clm_sbyte min, clm_sbyte max);

clm_byte clm_gen_random_u8(clm_byte min, clm_byte max);

clm_short clm_gen_random_i16(clm_short min, clm_short max);

clm_ushort clm_gen_random_u16(clm_ushort min, clm_ushort max);

clm_int clm_gen_random_i32(clm_int min, clm_int max);

clm_uint clm_gen_random_u32(clm_uint min, clm_uint max);

clm_double clm_gen_random_f64(clm_double min, clm_double max);

clm_float clm_gen_random_f32(clm_float min, clm_float max);

int clm_gen_internal_destination(clm_id destination);

int clm_gen_internal_casing(clm_int casing);

int clm_gen_internal_character(const char* alphabet, char* output);

int clm_gen_internal_text_to(clm_int length, const char* alphabet,
                                           int mixed_hex, void* buffer,
                                           clm_uint buffer_size);

int clm_gen_internal_text(clm_int length, const char* alphabet,
                                        int mixed_hex, clm_id destination);

clm_int clm_gen_hex_to(clm_int length, clm_int casing,
                                     void* buffer, clm_uint buffer_size);

clm_int clm_gen_hex(clm_int length, clm_int casing, clm_id destination);

clm_int clm_gen_uuid_to(clm_int casing, void* buffer,
                                      clm_uint buffer_size);

clm_int clm_gen_uuid(clm_int casing, clm_id destination);

clm_int clm_gen_alphanum_to(clm_int length, clm_int casing,
                                          void* buffer, clm_uint buffer_size);

clm_int clm_gen_alphanum(clm_int length, clm_int casing,
                                       clm_id destination);

clm_int clm_gen_strong_password_to(clm_int length, void* buffer,
                                                 clm_uint buffer_size);

clm_int clm_gen_strong_password(clm_int length, clm_id destination);