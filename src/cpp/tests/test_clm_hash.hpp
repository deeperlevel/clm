#pragma once

#include "test.hpp"
#include "../clm/hash.hpp"

static int clm_hash_test_string_equals(const clm_id id, const char* expected)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    return block != NULL && block->type == CLM_BPA_TYPE_STRING &&
        strcmp(block->data != NULL ? (const char*)block->data : "", expected) == 0;
}

static int clm_hash_tests(void)
{
    clm_test_result result = {0};
    static const char sha256_abc[] =
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad";
    static const char sha3_256_abc[] =
        "3a985da74fe225b2045c172d6bd390bd"
        "855f086e3e9d525b46bfe24511431532";
    static const unsigned char expected_sha256[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    static const clm_hash_algorithm algorithms[] = {
        CLM_HASH_ALGORITHM_MD2, CLM_HASH_ALGORITHM_MD4,
        CLM_HASH_ALGORITHM_MD5, CLM_HASH_ALGORITHM_SHA1,
        CLM_HASH_ALGORITHM_SHA224, CLM_HASH_ALGORITHM_SHA256,
        CLM_HASH_ALGORITHM_SHA384, CLM_HASH_ALGORITHM_SHA512,
        CLM_HASH_ALGORITHM_SHA512_224, CLM_HASH_ALGORITHM_SHA512_256,
        CLM_HASH_ALGORITHM_SHA3_224, CLM_HASH_ALGORITHM_SHA3_256,
        CLM_HASH_ALGORITHM_SHA3_384, CLM_HASH_ALGORITHM_SHA3_512
    };
    const clm_id input_id = UINT64_C(95001);
    const clm_id output_id = UINT64_C(95002);
    const clm_id name_id = UINT64_C(95003);
    unsigned char digest[CLM_HASH_MAX_DIGEST_SIZE] = {0};
    unsigned char unchanged[CLM_HASH_MAX_DIGEST_SIZE];
    char hex[CLM_HASH_MAX_DIGEST_SIZE * 2u + 1u];
    const unsigned char binary_input[] = {'a', 0, 'b'};

    clm_test_begin("CLM - Hash");

    CLM_ASSERT(clm_hash_digest_size(CLM_HASH_ALGORITHM_SHA256) == 32u &&
        clm_hash_hex_size(CLM_HASH_ALGORITHM_SHA256) == 65u &&
        clm_hash_digest_size(CLM_HASH_ALGORITHM_SHA3_512) == 64u &&
        clm_hash_digest_size(-1) == 0u &&
        clm_hash_hex_size(-1) == 0u,
        "report digest and lowercase hex buffer sizes for supported algorithms");

    CLM_ASSERT(clm_string_new(input_id, "abc") &&
        clm_string_new(output_id, "unchanged") &&
        clm_string_new(name_id, "name"),
        "create CLM string fixtures for hash adapters");

    CLM_ASSERT(clm_hash_bytes(CLM_HASH_ALGORITHM_SHA256, "abc", 3u,
            digest, sizeof(digest)) == CLM_HASH_SUCCESS &&
        memcmp(digest, expected_sha256, sizeof(expected_sha256)) == 0,
        "hash explicit bytes with SHA-256 known-answer vector");

    CLM_ASSERT(clm_hash_bytes_as(CLM_HASH_ALGORITHM_SHA256, "abc",
            digest, sizeof(digest)) == CLM_HASH_SUCCESS &&
        memcmp(digest, expected_sha256, sizeof(expected_sha256)) == 0 &&
        clm_hash_bytes_from(CLM_HASH_ALGORITHM_SHA256, input_id,
            digest, sizeof(digest)) == CLM_HASH_SUCCESS &&
        memcmp(digest, expected_sha256, sizeof(expected_sha256)) == 0,
        "hash C-string and CLM-string inputs consistently");

    int algorithms_available = 1;
    for (size_t index = 0;
         index < sizeof(algorithms) / sizeof(algorithms[0]); ++index) {
        if (clm_hash_bytes(algorithms[index], "abc", 3u,
                digest, sizeof(digest)) != CLM_HASH_SUCCESS ||
            clm_hash_digest_size(algorithms[index]) == 0u) {
            algorithms_available = 0;
            break;
        }
    }
    CLM_ASSERT(algorithms_available,
        "compute a digest for every algorithm exposed by the C API");

    CLM_ASSERT(clm_hash_string_as(CLM_HASH_ALGORITHM_SHA256, "abc",
            hex, sizeof(hex)) == CLM_HASH_SUCCESS &&
        strcmp(hex, sha256_abc) == 0 &&
        clm_hash_string_as(CLM_HASH_ALGORITHM_SHA3_256, "abc",
            hex, sizeof(hex)) == CLM_HASH_SUCCESS &&
        strcmp(hex, sha3_256_abc) == 0,
        "hash UTF-8 text and format SHA-2/SHA-3 lowercase hex vectors");

    CLM_ASSERT(clm_hash_bytes_to(CLM_HASH_ALGORITHM_SHA256, "abc", 3u,
            output_id) == CLM_HASH_SUCCESS &&
        clm_hash_test_string_equals(output_id, sha256_abc) &&
        clm_hash_string_to(CLM_HASH_ALGORITHM_SHA3_256, "abc", output_id) ==
            CLM_HASH_SUCCESS &&
        clm_hash_test_string_equals(output_id, sha3_256_abc) &&
        clm_hash_string_from(CLM_HASH_ALGORITHM_SHA256, input_id, output_id) ==
            CLM_HASH_SUCCESS &&
        clm_hash_test_string_equals(output_id, sha256_abc),
        "write byte and string digests to CLM string IDs");

    CLM_ASSERT(clm_hash_algorithm_name(CLM_HASH_ALGORITHM_SHA3_256, name_id) ==
            CLM_HASH_SUCCESS &&
        clm_hash_test_string_equals(name_id, "SHA3-256") &&
        clm_hash_algorithm_name(999, name_id) ==
            CLM_HASH_ERROR_UNSUPPORTED_ALGORITHM &&
        clm_hash_test_string_equals(name_id, "SHA3-256"),
        "store known algorithm names and preserve output for unknown IDs");

    memset(unchanged, 0xA5, sizeof(unchanged));
    CLM_ASSERT(clm_hash_bytes(CLM_HASH_ALGORITHM_SHA256, binary_input,
            sizeof(binary_input), digest, sizeof(digest)) == CLM_HASH_SUCCESS &&
        clm_hash_bytes(CLM_HASH_ALGORITHM_SHA256, "a", 1u,
            unchanged, sizeof(unchanged)) == CLM_HASH_SUCCESS &&
        memcmp(digest, unchanged, 32u) != 0,
        "include embedded NUL bytes in explicit-length hash input");

    memset(unchanged, 0xA5, sizeof(unchanged));
    CLM_ASSERT(clm_hash_bytes(CLM_HASH_ALGORITHM_SHA256, "abc", 3u,
            unchanged, 1u) == CLM_HASH_ERROR_BUFFER_TOO_SMALL &&
        unchanged[0] == 0xA5 &&
        clm_hash_bytes(999, "abc", 3u, unchanged, sizeof(unchanged)) ==
            CLM_HASH_ERROR_UNSUPPORTED_ALGORITHM &&
        unchanged[0] == 0xA5 &&
        clm_hash_bytes(CLM_HASH_ALGORITHM_SHA256, NULL, 1u,
            unchanged, sizeof(unchanged)) == CLM_HASH_ERROR_INVALID_ARGUMENT &&
        unchanged[0] == 0xA5,
        "reject invalid inputs and insufficient outputs without modifying buffers");

    memset(hex, 'x', sizeof(hex));
    CLM_ASSERT(clm_hash_hex_encode("\x00\xaf", 2u, hex, sizeof(hex)) ==
            CLM_HASH_SUCCESS &&
        strcmp(hex, "00af") == 0 &&
        clm_hash_hex_encode("abc", 3u, hex, 6u) ==
            CLM_HASH_ERROR_BUFFER_TOO_SMALL &&
        hex[0] == '0',
        "encode arbitrary bytes as lowercase hex and preserve undersized output");

    CLM_ASSERT(clm_hash_string_to(CLM_HASH_ALGORITHM_SHA256, "\xC0\xAF",
            output_id) == CLM_HASH_ERROR_INVALID_ARGUMENT &&
        clm_hash_test_string_equals(output_id, sha256_abc),
        "reject malformed UTF-8 without replacing destination string");

    (void)clm_string_delete(input_id);
    (void)clm_string_delete(output_id);
    (void)clm_string_delete(name_id);

    clm_test_summary(result.passed, result.failed);
    return result.failed != 0;
}


static int test_clm_hash(void)
{
    return clm_hash_tests();
}

