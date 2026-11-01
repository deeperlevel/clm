#pragma once

#include "test.hpp"
#include "../clm/gen.hpp"

static int clm_gen_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Generation");

    int integers_valid = 1;
    int floats_valid = 1;
    for (int iteration = 0; iteration < 256; ++iteration) {
        const clm_sbyte signed_byte = clm_gen_random_i8(-12, 12);
        const clm_byte byte = clm_gen_random_u8(5, 200);
        const clm_short signed_short = clm_gen_random_i16(-1000, -100);
        const clm_ushort unsigned_short = clm_gen_random_u16(40000, 60000);
        const clm_int integer = clm_gen_random_i32(-100000, 100000);
        const clm_uint unsigned_integer = clm_gen_random_u32(3000000000U, UINT_MAX);
        const clm_long long_integer = clm_gen_random_i64(LONG_MIN, LONG_MIN + 100);
        const clm_ulong unsigned_long = clm_gen_random_u64(ULONG_MAX - 100, ULONG_MAX);
        integers_valid &= signed_byte >= -12 && signed_byte <= 12 &&
            byte >= 5 && byte <= 200 && signed_short >= -1000 && signed_short <= -100 &&
            unsigned_short >= 40000 && unsigned_short <= 60000 &&
            integer >= -100000 && integer <= 100000 && unsigned_integer >= 3000000000U &&
            long_integer >= LONG_MIN && long_integer <= LONG_MIN + 100 &&
            unsigned_long >= ULONG_MAX - 100;
        const clm_float single = clm_gen_random_f32(FLOAT_MIN, FLOAT_MAX);
        const clm_double wide = clm_gen_random_f64(DOUBLE_MIN, DOUBLE_MAX);
        const clm_double positive = clm_gen_random_f64(10.0, 20.0);
        const clm_double negative = clm_gen_random_f64(-20.0, -10.0);
        floats_valid &= isfinite(single) && single >= FLOAT_MIN && single <= FLOAT_MAX &&
            isfinite(wide) && wide >= DOUBLE_MIN && wide <= DOUBLE_MAX &&
            positive >= 10.0 && positive <= 20.0 && negative >= -20.0 && negative <= -10.0;
    }
    CLM_ASSERT(integers_valid, "keep all eight integer types within inclusive bounds");
    CLM_ASSERT(floats_valid, "generate finite floating-point values without range overflow");
    CLM_ASSERT(clm_gen_random_i64(LONG_MIN, LONG_MIN) == LONG_MIN &&
               clm_gen_random_i64(LONG_MAX, LONG_MAX) == LONG_MAX &&
               clm_gen_random_u64(ULONG_MAX, ULONG_MAX) == ULONG_MAX &&
               clm_gen_random_i8(SBYTE_MIN, SBYTE_MIN) == SBYTE_MIN &&
               clm_gen_random_u8(BYTE_MAX, BYTE_MAX) == BYTE_MAX &&
               clm_gen_random_i16(SHORT_MIN, SHORT_MIN) == SHORT_MIN &&
               clm_gen_random_u16(USHORT_MAX, USHORT_MAX) == USHORT_MAX &&
               clm_gen_random_i32(INT_MIN, INT_MIN) == INT_MIN &&
               clm_gen_random_u32(UINT_MAX, UINT_MAX) == UINT_MAX &&
               clm_gen_random_f32(3.5f, 3.5f) == 3.5f &&
               clm_gen_random_f64(-2.0, -2.0) == -2.0,
               "return the bound for singleton ranges including integer limits");
    const clm_long reversed = clm_gen_random_i64(10, -10);
    const clm_ulong reversed_unsigned = clm_gen_random_u64(20, 10);
    const clm_double reversed_float = clm_gen_random_f64(2.0, -2.0);
    CLM_ASSERT(reversed >= -10 && reversed <= 10 &&
               reversed_unsigned >= 10 && reversed_unsigned <= 20 &&
               reversed_float >= -2.0 && reversed_float <= 2.0,
               "normalize reversed numeric bounds");
    (void)clm_gen_random_i64(LONG_MIN, LONG_MAX);
    (void)clm_gen_random_u64(ULONG_MIN, ULONG_MAX);
    CLM_ASSERT(isnan(clm_gen_random_f64(NAN, 1.0)) &&
               isnan(clm_gen_random_f64(0.0, INFINITY)),
               "reject non-finite floating-point bounds with NaN");

    const clm_id destination = 7001;
    const clm_id wrong_type = 7002;
    char text[129] = {0};
    CLM_ASSERT(clm_bpa_create(destination, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS &&
               clm_bpa_create(wrong_type, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS,
               "create generation test destinations");
    for (clm_int casing = GENERATION_CASING_MIXED;
         casing <= GENERATION_CASING_LOWER; ++casing) {
        const char* hex_alphabet = casing == GENERATION_CASING_UPPER
            ? "0123456789ABCDEF" : casing == GENERATION_CASING_LOWER
            ? "0123456789abcdef" : "0123456789abcdefABCDEF";
        CLM_ASSERT(clm_gen_hex(128, casing, destination) == GENERATION_SUCCESS &&
                   clm_bpa_get_string(destination, text, sizeof(text)) == 128 &&
                   strlen(text) == 128 && strspn(text, hex_alphabet) == 128,
                   "generate hexadecimal text with the requested length and casing");
        const char* alphabet = casing == GENERATION_CASING_UPPER
            ? "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" : casing == GENERATION_CASING_LOWER
            ? "0123456789abcdefghijklmnopqrstuvwxyz"
            : "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        CLM_ASSERT(clm_gen_alphanum(128, casing, destination) == GENERATION_SUCCESS &&
                   clm_bpa_get_string(destination, text, sizeof(text)) == 128 &&
                   strlen(text) == 128 && strspn(text, alphabet) == 128,
                   "generate alphanumeric text with the requested length and casing");
        CLM_ASSERT(clm_gen_uuid(casing, destination) == GENERATION_SUCCESS &&
                   clm_bpa_get_string(destination, text, sizeof(text)) == 36 &&
                   strlen(text) == 36 && text[8] == '-' && text[13] == '-' &&
                   text[18] == '-' && text[23] == '-' && text[14] == '4' &&
                   strchr("89aAbB", text[19]) != NULL &&
                   strspn(text, "0123456789abcdefABCDEF-") == 36,
                   "generate a canonical version-4 UUID with the RFC variant");
        int uuid_casing_valid = 1;
        for (size_t index = 0; index < 36; ++index)
            uuid_casing_valid &= text[index] == '-' || strchr(hex_alphabet, text[index]) != NULL;
        CLM_ASSERT(uuid_casing_valid, "apply the requested UUID letter casing");
        text[128] = '!';
        CLM_ASSERT(clm_gen_hex_to(127, casing, text, 128) == GENERATION_SUCCESS &&
                   text[127] == '\0' && text[128] == '!' &&
                   strspn(text, hex_alphabet) == 127,
                   "generate hexadecimal text into an exact-fit buffer without overruns");
        CLM_ASSERT(clm_gen_alphanum_to(127, casing, text, 128) == GENERATION_SUCCESS &&
                   text[127] == '\0' && text[128] == '!' &&
                   strspn(text, alphabet) == 127,
                   "generate alphanumeric text into an exact-fit buffer without overruns");
        text[37] = '!';
        const int uuid_generated = clm_gen_uuid_to(casing, text, 37) == GENERATION_SUCCESS;
        uuid_casing_valid = 1;
        for (size_t index = 0; index < 36; ++index)
            uuid_casing_valid &= text[index] == '-' || strchr(hex_alphabet, text[index]) != NULL;
        CLM_ASSERT(uuid_generated && text[36] == '\0' && text[37] == '!' &&
                   uuid_casing_valid && text[8] == '-' && text[13] == '-' &&
                   text[18] == '-' && text[23] == '-' && text[14] == '4' &&
                   strchr("89aAbB", text[19]) != NULL,
                   "write a version-4 UUID with the requested casing into 37 bytes");
    }
    int passwords_valid = 1;
    for (int iteration = 0; iteration < 32; ++iteration) {
        const clm_int length = iteration == 0 ? 4 : 32;
        const int generated = clm_gen_strong_password(length, destination) == GENERATION_SUCCESS &&
            clm_bpa_get_string(destination, text, sizeof(text)) == length;
        int upper = 0, lower = 0, digit = 0, symbol = 0;
        for (clm_int index = 0; generated && index < length; ++index) {
            upper |= text[index] >= 'A' && text[index] <= 'Z';
            lower |= text[index] >= 'a' && text[index] <= 'z';
            digit |= text[index] >= '0' && text[index] <= '9';
            symbol |= strchr("!@#$%^&*()-_=+[]{};:,.?/|~", text[index]) != NULL;
        }
        passwords_valid &= generated && upper && lower && digit && symbol;
    }
    CLM_ASSERT(passwords_valid, "include all four character classes in every strong password");
    int buffer_passwords_valid = 1;
    for (int iteration = 0; iteration < 32; ++iteration) {
        const clm_int length = iteration == 0 ? 4 : 32;
        text[length + 1] = '!';
        const int generated = clm_gen_strong_password_to(length, text,
            (clm_uint)length + 1) == GENERATION_SUCCESS;
        int upper = 0, lower = 0, digit = 0, symbol = 0;
        for (clm_int index = 0; generated && index < length; ++index) {
            upper |= text[index] >= 'A' && text[index] <= 'Z';
            lower |= text[index] >= 'a' && text[index] <= 'z';
            digit |= text[index] >= '0' && text[index] <= '9';
            symbol |= strchr("!@#$%^&*()-_=+[]{};:,.?/|~", text[index]) != NULL;
        }
        buffer_passwords_valid &= generated && upper && lower && digit && symbol &&
            text[length] == '\0' && text[length + 1] == '!';
    }
    CLM_ASSERT(buffer_passwords_valid,
               "write strong passwords into exact-fit buffers with all four character classes");
    char empty[2] = {'!', '!'};
    CLM_ASSERT(clm_gen_hex_to(0, 0, empty, 1) == GENERATION_SUCCESS &&
               empty[0] == '\0' && empty[1] == '!' &&
               clm_gen_alphanum_to(0, 0, empty, 1) == GENERATION_SUCCESS &&
               empty[0] == '\0' && empty[1] == '!',
               "write empty strings into one-byte buffers");
    CLM_ASSERT(clm_gen_hex_to(8, 0, NULL, 9) == GENERATION_FAILURE &&
               clm_gen_alphanum_to(8, 0, NULL, 9) == GENERATION_FAILURE &&
               clm_gen_uuid_to(0, NULL, 37) == GENERATION_FAILURE &&
               clm_gen_strong_password_to(8, NULL, 9) == GENERATION_FAILURE,
               "reject null buffers in all four buffer generators");
    memset(text, '!', sizeof(text));
    CLM_ASSERT(clm_gen_hex_to(8, 0, text, 8) == GENERATION_FAILURE &&
               clm_gen_alphanum_to(8, 0, text, 8) == GENERATION_FAILURE &&
               clm_gen_uuid_to(0, text, 36) == GENERATION_FAILURE &&
               clm_gen_strong_password_to(8, text, 8) == GENERATION_FAILURE &&
               clm_gen_hex_to(0, 0, text, 0) == GENERATION_FAILURE &&
               clm_gen_alphanum_to(0, 0, text, 0) == GENERATION_FAILURE &&
               clm_gen_uuid_to(0, text, 0) == GENERATION_FAILURE &&
               clm_gen_strong_password_to(4, text, 0) == GENERATION_FAILURE &&
               text[0] == '!' && text[128] == '!',
               "reject insufficient capacities including zero without writing output");
    CLM_ASSERT(clm_gen_hex_to(-1, 0, text, sizeof(text)) == GENERATION_FAILURE &&
               clm_gen_hex_to(8, -1, text, sizeof(text)) == GENERATION_FAILURE &&
               clm_gen_alphanum_to(-1, 0, text, sizeof(text)) == GENERATION_FAILURE &&
               clm_gen_alphanum_to(8, 3, text, sizeof(text)) == GENERATION_FAILURE &&
               clm_gen_uuid_to(3, text, sizeof(text)) == GENERATION_FAILURE &&
               clm_gen_strong_password_to(3, text, sizeof(text)) == GENERATION_FAILURE &&
               clm_gen_strong_password_to(-1, text, sizeof(text)) == GENERATION_FAILURE &&
               text[0] == '!' && text[128] == '!',
               "reject invalid buffer-generator lengths and casing without writing output");
    CLM_ASSERT(clm_gen_hex(0, GENERATION_CASING_LOWER, destination) == GENERATION_SUCCESS &&
               clm_bpa_get_string(destination, text, sizeof(text)) == 0 && text[0] == '\0' &&
               clm_gen_alphanum(0, GENERATION_CASING_MIXED, destination) == GENERATION_SUCCESS,
               "allow empty hexadecimal and alphanumeric strings");
    CLM_ASSERT(clm_bpa_set_string(destination, "unchanged", 10) == 9 &&
               clm_gen_hex(-1, GENERATION_CASING_LOWER, destination) == GENERATION_FAILURE &&
               clm_gen_hex(8, -1, destination) == GENERATION_FAILURE &&
               clm_gen_alphanum(-1, GENERATION_CASING_LOWER, destination) == GENERATION_FAILURE &&
               clm_gen_alphanum(8, 3, destination) == GENERATION_FAILURE &&
               clm_gen_uuid(3, destination) == GENERATION_FAILURE &&
               clm_gen_strong_password(3, destination) == GENERATION_FAILURE &&
               clm_gen_strong_password(-1, destination) == GENERATION_FAILURE &&
               clm_bpa_get_string(destination, text, sizeof(text)) == 9 &&
               strcmp(text, "unchanged") == 0,
               "reject invalid lengths and casing without changing the destination");
    CLM_ASSERT(clm_gen_uuid(0, wrong_type) == GENERATION_FAILURE &&
               clm_gen_hex(8, 0, wrong_type) == GENERATION_FAILURE &&
               clm_gen_alphanum(8, 0, wrong_type) == GENERATION_FAILURE &&
               clm_gen_strong_password(8, wrong_type) == GENERATION_FAILURE &&
               clm_gen_uuid(0, 7003) == GENERATION_FAILURE &&
               clm_gen_hex(8, 0, 7003) == GENERATION_FAILURE &&
               clm_gen_alphanum(8, 0, 7003) == GENERATION_FAILURE &&
               clm_gen_strong_password(8, 7003) == GENERATION_FAILURE,
               "require an existing string destination for every text generator");
    CLM_ASSERT(clm_bpa_delete(destination) == CLM_BPA_SUCCESS &&
               clm_bpa_delete(wrong_type) == CLM_BPA_SUCCESS,
               "release generation test destinations");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}

static int test_clm_gen(void)
{
    return clm_gen_tests();
}

