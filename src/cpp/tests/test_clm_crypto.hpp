#pragma once

#include "test.hpp"
#include "../clm/crypto.hpp"

static int clm_crypto_test_string_equals(clm_id id, const char* expected)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    return block != NULL && block->type == CLM_BPA_TYPE_STRING &&
        strcmp(block->data != NULL ? (const char*)block->data : "", expected) == 0;
}

static int clm_crypto_test_filled(const void* data, size_t length, unsigned char value)
{
    const unsigned char* bytes = data;
    for (size_t i = 0; i < length; ++i) {
        if (bytes[i] != value)
            return 0;
    }
    return 1;
}

static int clm_crypto_tests(void)
{
    clm_test_result result = {0};
    const clm_crypto_algorithm algorithm = CLM_CRYPTO_ALGORITHM_XCHACHA20_POLY1305;
    /* draft-irtf-cfrg-xchacha-03, section A.3.1. */
    const char message[] =
        "Ladies and Gentlemen of the class of '99: If I could offer you "
        "only one tip for the future, sunscreen would be it.";
    const char expected_hex[] =
        "bd6d179d3e83d43b9576579493c0e939572a1700252bfaccbed2902c21396cbb"
        "731c7f1b0b4aa6440bf3a82f4eda7e39ae64c6708c54c216cb96b72e1213b452"
        "2f8c9ba40db5d945b11b69b982c1bb9e3f3fac2bc369488f76b2383565d3fff9"
        "21f9664c97637da9768812f615c68b13b52e";
    const unsigned char expected_tag[16] = {
        0xc0, 0x87, 0x59, 0x24, 0xc1, 0xc7, 0x98, 0x79,
        0x47, 0xde, 0xaf, 0xd8, 0x78, 0x0a, 0xcf, 0x49
    };
    unsigned char key[32], nonce[24];
    unsigned char aad[] = {
        0x50, 0x51, 0x52, 0x53, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7
    };
    const size_t length = sizeof(message) - 1u;
    unsigned char ciphertext[sizeof(message)], plaintext[sizeof(message)];
    unsigned char tag[16], other_tag[16];
    char hex[sizeof(message) * 2u + 1u];
    char text[sizeof(message)];
    const clm_id source_id = UINT64_C(95101);
    const clm_id hex_id = UINT64_C(95102);
    const clm_id text_id = UINT64_C(95103);
    const clm_id block_id = UINT64_C(95104);
    const clm_id encrypted_id = UINT64_C(95105);
    const clm_id decrypted_id = UINT64_C(95106);
    const clm_id key_id = UINT64_C(95107);
    const clm_id nonce_id = UINT64_C(95108);
    const clm_id missing_id = UINT64_C(95199);
    clm_test_begin("CLM - Crypto");
    for (size_t i = 0; i < sizeof(key); ++i) key[i] = (unsigned char)(0x80u + i);
    for (size_t i = 0; i < sizeof(nonce); ++i) nonce[i] = (unsigned char)(0x40u + i);

    CLM_ASSERT(clm_crypto_key_size(algorithm) == 32u &&
        clm_crypto_nonce_size(algorithm) == 24u &&
        clm_crypto_tag_size(algorithm) == 16u &&
        clm_crypto_key_size(99) == 0u && clm_crypto_nonce_size(99) == 0u &&
        clm_crypto_tag_size(99) == 0u,
        "report exact supported cipher sizes and reject unknown identifiers");
    CLM_ASSERT(clm_string_new(source_id, message) &&
        clm_string_new(hex_id, "unchanged") && clm_string_new(text_id, "unchanged"),
        "create existing string destinations for crypto adapters");
    CLM_ASSERT(clm_crypto_algorithm_name(algorithm, text_id) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_test_string_equals(text_id, "XChaCha20-Poly1305") &&
        clm_crypto_algorithm_name(99, text_id) ==
            CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM &&
        clm_crypto_algorithm_name(algorithm, missing_id) ==
            CLM_CRYPTO_ERROR_INVALID_ARGUMENT,
        "write cipher name only to existing string IDs");

    CLM_ASSERT(clm_crypto_encrypt_bytes(algorithm, key, sizeof(key), nonce,
        sizeof(nonce), aad, sizeof(aad), message, length, ciphertext,
        sizeof(ciphertext), tag, sizeof(tag)) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_hex_encode(ciphertext, length, hex, sizeof(hex)) ==
            CLM_CRYPTO_SUCCESS &&
        strcmp(hex, expected_hex) == 0 && memcmp(tag, expected_tag, 16u) == 0,
        "match independent XChaCha20-Poly1305 ciphertext and tag vector");
    CLM_ASSERT(clm_crypto_decrypt_bytes(algorithm, key, sizeof(key), nonce,
        sizeof(nonce), aad, sizeof(aad), ciphertext, length, tag, sizeof(tag),
        plaintext, sizeof(plaintext)) == CLM_CRYPTO_SUCCESS &&
        memcmp(plaintext, message, length) == 0,
        "authenticate and decrypt the known-answer ciphertext");

    int tampering_rejected = 1;
    for (size_t i = 0; i < 5u; ++i) {
        unsigned char* changed = i == 0 ? ciphertext : i == 1 ? tag :
            i == 2 ? aad : i == 3 ? nonce : key;
        changed[0] ^= 1u;
        memset(plaintext, 0xa5, sizeof(plaintext));
        tampering_rejected &= clm_crypto_decrypt_bytes(algorithm, key, sizeof(key),
            nonce, sizeof(nonce), aad, sizeof(aad), ciphertext, length, tag,
            sizeof(tag), plaintext, sizeof(plaintext)) ==
                CLM_CRYPTO_ERROR_AUTHENTICATION &&
            clm_crypto_test_filled(plaintext, sizeof(plaintext), 0xa5);
        changed[0] ^= 1u;
    }
    CLM_ASSERT(tampering_rejected,
        "reject changed ciphertext, tag, AAD, nonce and key without releasing plaintext");

    memset(plaintext, 0xa5, sizeof(plaintext));
    memset(other_tag, 0xa5, sizeof(other_tag));
    CLM_ASSERT(clm_crypto_encrypt_bytes(99, key, 32u, nonce, 24u, NULL, 0u,
        message, length, plaintext, sizeof(plaintext), other_tag, 16u) ==
            CLM_CRYPTO_ERROR_UNSUPPORTED_ALGORITHM &&
        clm_crypto_encrypt_bytes(algorithm, key, 31u, nonce, 24u, NULL, 0u,
        message, length, plaintext, sizeof(plaintext), other_tag, 16u) ==
            CLM_CRYPTO_ERROR_INVALID_ARGUMENT &&
        clm_crypto_encrypt_bytes(algorithm, key, 32u, nonce, 24u, NULL, 0u,
        message, length, plaintext, length - 1u, other_tag, 16u) ==
            CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL &&
        clm_crypto_encrypt_bytes(algorithm, key, 32u, nonce, 24u, NULL, 0u,
        message, length, plaintext, sizeof(plaintext), other_tag, 15u) ==
            CLM_CRYPTO_ERROR_BUFFER_TOO_SMALL &&
        clm_crypto_test_filled(plaintext, sizeof(plaintext), 0xa5) &&
        clm_crypto_test_filled(other_tag, sizeof(other_tag), 0xa5),
        "reject unsupported algorithms, wrong sizes and undersized outputs atomically");
    CLM_ASSERT(clm_crypto_encrypt_bytes(algorithm, key, 32u, nonce, 24u,
        NULL, 0u, ciphertext, length, ciphertext, sizeof(ciphertext), tag, 16u) ==
            CLM_CRYPTO_ERROR_INVALID_ARGUMENT &&
        clm_crypto_decrypt_bytes(algorithm, key, 32u, nonce, 24u, aad, sizeof(aad),
        ciphertext, length, tag, 16u, ciphertext, sizeof(ciphertext)) ==
            CLM_CRYPTO_ERROR_INVALID_ARGUMENT,
        "reject overlapping byte encryption and decryption buffers");
    CLM_ASSERT(clm_crypto_encrypt_bytes(algorithm, key, 32u, nonce, 24u,
        NULL, 0u, NULL, 0u, NULL, 0u, other_tag, 16u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_decrypt_bytes(algorithm, key, 32u, nonce, 24u, NULL, 0u,
        NULL, 0u, other_tag, 16u, NULL, 0u) == CLM_CRYPTO_SUCCESS,
        "authenticate empty messages with absent zero-length buffers");

    CLM_ASSERT(clm_crypto_encrypt_bytes_as(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), message, ciphertext, sizeof(ciphertext), tag, 16u) ==
            CLM_CRYPTO_SUCCESS &&
        clm_crypto_encrypt_bytes_from(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), source_id, plaintext, sizeof(plaintext), other_tag, 16u) ==
            CLM_CRYPTO_SUCCESS &&
        memcmp(ciphertext, plaintext, length) == 0 &&
        memcmp(tag, expected_tag, 16u) == 0 && memcmp(tag, other_tag, 16u) == 0,
        "encrypt C-string and CLM-string bytes without including a terminator");
    CLM_ASSERT(clm_crypto_encrypt_bytes_to(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), message, length, hex_id, tag, 16u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_test_string_equals(hex_id, expected_hex) &&
        clm_crypto_decrypt_bytes_from(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), hex_id, tag, 16u, plaintext, sizeof(plaintext)) ==
            CLM_CRYPTO_SUCCESS && memcmp(plaintext, message, length) == 0,
        "store ciphertext hex in a CLM string and decrypt it to binary bytes");
    CLM_ASSERT(clm_crypto_encrypt_string_to(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), message, hex_id, tag, 16u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_decrypt_string_to(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), expected_hex, tag, 16u, text_id) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_test_string_equals(text_id, message) &&
        clm_crypto_encrypt_string_from(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), source_id, hex_id, tag, 16u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_decrypt_string_from(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), hex_id, tag, 16u, text_id) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_test_string_equals(text_id, message),
        "round-trip UTF-8 strings through all CLM hex string adapters");
    CLM_ASSERT(clm_crypto_encrypt_string_as(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), message, hex, sizeof(hex), tag, 16u) == CLM_CRYPTO_SUCCESS &&
        strcmp(hex, expected_hex) == 0 &&
        clm_crypto_decrypt_string_as(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), hex, tag, 16u, text, sizeof(text)) == CLM_CRYPTO_SUCCESS &&
        strcmp(text, message) == 0,
        "round-trip C text through caller-owned NUL-terminated hex buffers");
    CLM_ASSERT(clm_crypto_encrypt_string_from(algorithm, key, 32u, nonce, 24u,
        NULL, 0u, source_id, source_id, tag, 16u) ==
            CLM_CRYPTO_ERROR_INVALID_ARGUMENT &&
        clm_crypto_decrypt_string_from(algorithm, key, 32u, nonce, 24u,
        NULL, 0u, hex_id, tag, 16u, hex_id) == CLM_CRYPTO_ERROR_INVALID_ARGUMENT &&
        clm_crypto_encrypt_string_to(algorithm, key, 32u, nonce, 24u, NULL, 0u,
        "\xc0\xaf", hex_id, tag, 16u) == CLM_CRYPTO_ERROR_INVALID_TEXT &&
        clm_crypto_test_string_equals(hex_id, expected_hex),
        "reject same-ID string operations and malformed UTF-8 without replacement");

    tag[0] ^= 1u;
    memset(text, 0xa5, sizeof(text));
    CLM_ASSERT(clm_crypto_decrypt_string_from(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), hex_id, tag, 16u, text_id) ==
            CLM_CRYPTO_ERROR_AUTHENTICATION &&
        clm_crypto_test_string_equals(text_id, message) &&
        clm_crypto_decrypt_string_as(algorithm, key, 32u, nonce, 24u,
        aad, sizeof(aad), expected_hex, tag, 16u, text, sizeof(text)) ==
            CLM_CRYPTO_ERROR_AUTHENTICATION &&
        clm_crypto_test_filled(text, sizeof(text), 0xa5),
        "preserve ID and C text destinations when the authentication tag changes");
    tag[0] ^= 1u;
    const char unicode[] = "caf\xc3\xa9 \xf0\x9f\x90\xb1";
    CLM_ASSERT(clm_crypto_encrypt_string_as(algorithm, key, 32u, nonce, 24u,
        NULL, 0u, unicode, hex, sizeof(hex), tag, 16u) == CLM_CRYPTO_SUCCESS &&
        strlen(hex) == 2u * (sizeof(unicode) - 1u) &&
        clm_crypto_decrypt_string_as(algorithm, key, 32u, nonce, 24u, NULL, 0u,
        hex, tag, 16u, text, sizeof(text)) == CLM_CRYPTO_SUCCESS &&
        strcmp(text, unicode) == 0,
        "use UTF-8 byte lengths rather than character counts for Unicode plaintext");

    const unsigned char binary[] = {'A', 0, 0xff};
    CLM_ASSERT(clm_crypto_encrypt_bytes_to(algorithm, key, 32u, nonce, 24u,
        NULL, 0u, binary, sizeof(binary), hex_id, tag, 16u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_decrypt_bytes_from(algorithm, key, 32u, nonce, 24u, NULL, 0u,
        hex_id, tag, 16u, plaintext, sizeof(plaintext)) == CLM_CRYPTO_SUCCESS &&
        memcmp(plaintext, binary, sizeof(binary)) == 0 &&
        clm_crypto_decrypt_string_from(algorithm, key, 32u, nonce, 24u, NULL, 0u,
        hex_id, tag, 16u, text_id) == CLM_CRYPTO_ERROR_INVALID_TEXT &&
        clm_crypto_test_string_equals(text_id, message),
        "preserve arbitrary binary plaintext but reject it as UTF-8 string output");
    size_t decoded = 77u;
    memset(plaintext, 0xa5, sizeof(plaintext));
    CLM_ASSERT(clm_crypto_hex_size(SIZE_MAX) == 0u &&
        clm_crypto_hex_size(0u) == 1u &&
        clm_crypto_hex_decode("00aF", plaintext, sizeof(plaintext), &decoded) ==
            CLM_CRYPTO_SUCCESS && decoded == 2u &&
        plaintext[0] == 0u && plaintext[1] == 0xaf &&
        clm_crypto_hex_decode("0x", plaintext, sizeof(plaintext), &decoded) ==
            CLM_CRYPTO_ERROR_INVALID_TEXT &&
        clm_crypto_hex_decode("0", plaintext, sizeof(plaintext), &decoded) ==
            CLM_CRYPTO_ERROR_INVALID_TEXT && decoded == 2u &&
        plaintext[0] == 0u && plaintext[1] == 0xaf,
        "check hex overflow, mixed case, strict syntax and unchanged failure outputs");

    CLM_ASSERT(clm_bpa_create_sized(block_id, CLM_BPA_TYPE_STRUCT, 3u) == CLM_BPA_SUCCESS &&
        clm_bpa_create_sized(encrypted_id, CLM_BPA_TYPE_STRUCT, 3u) == CLM_BPA_SUCCESS &&
        clm_bpa_create_sized(decrypted_id, CLM_BPA_TYPE_STRUCT, 3u) == CLM_BPA_SUCCESS &&
        clm_bpa_create_sized(key_id, CLM_BPA_TYPE_STRUCT, 32u) == CLM_BPA_SUCCESS &&
        clm_bpa_create_sized(nonce_id, CLM_BPA_TYPE_STRUCT, 24u) == CLM_BPA_SUCCESS &&
        clm_bpa_set(block_id, binary, sizeof(binary)) == CLM_BPA_SET_SUCCESS,
        "create binary BPA fixtures with exact key, nonce and payload sizes");
    CLM_ASSERT(clm_crypto_encrypt_from(algorithm, key, 32u, nonce, 24u, NULL, 0u,
        block_id, encrypted_id, tag, 16u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_decrypt_from(algorithm, key, 32u, nonce, 24u, NULL, 0u,
        encrypted_id, tag, 16u, decrypted_id) == CLM_CRYPTO_SUCCESS &&
        clm_bpa_get(decrypted_id, plaintext, sizeof(plaintext)) == CLM_BPA_GET_SUCCESS &&
        memcmp(plaintext, binary, sizeof(binary)) == 0,
        "round-trip fixed-size binary BPA blocks");
    tag[0] ^= 1u;
    CLM_ASSERT(clm_crypto_decrypt_from(algorithm, key, 32u, nonce, 24u,
        NULL, 0u, encrypted_id, tag, 16u, decrypted_id) ==
            CLM_CRYPTO_ERROR_AUTHENTICATION &&
        clm_bpa_get(decrypted_id, plaintext, sizeof(plaintext)) == CLM_BPA_GET_SUCCESS &&
        memcmp(plaintext, binary, sizeof(binary)) == 0 &&
        clm_crypto_encrypt_from(algorithm, key, 32u, nonce, 24u,
        NULL, 0u, block_id, block_id, tag, 16u) ==
            CLM_CRYPTO_ERROR_INVALID_ARGUMENT,
        "preserve binary BPA destination on auth failure and reject in-place IDs");
    tag[0] ^= 1u;

    unsigned char salt[16] = {0};
    unsigned char derived[32], comparison[32];
    CLM_ASSERT(clm_crypto_derive_key_bytes(message, length, salt, 16u, 32u, 2u,
        derived, 32u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_derive_key_as(message, salt, 16u, 32u, 2u, comparison, 32u) ==
            CLM_CRYPTO_SUCCESS && memcmp(derived, comparison, 32u) == 0 &&
        clm_crypto_derive_key_from(source_id, salt, 16u, 32u, 2u, comparison, 32u) ==
            CLM_CRYPTO_SUCCESS && memcmp(derived, comparison, 32u) == 0 &&
        clm_crypto_derive_key_to(message, length, salt, 16u, 32u, 2u, key_id) ==
            CLM_CRYPTO_SUCCESS &&
        clm_bpa_get(key_id, comparison, 32u) == CLM_BPA_GET_SUCCESS &&
        memcmp(derived, comparison, 32u) == 0,
        "derive consistent Argon2id keys through byte, C-string and ID adapters");
    memset(comparison, 0xa5, sizeof(comparison));
    CLM_ASSERT(clm_crypto_derive_key_bytes(NULL, 1u, salt, 16u, 32u, 2u,
        comparison, 32u) == CLM_CRYPTO_ERROR_INVALID_ARGUMENT &&
        clm_crypto_derive_key_bytes(message, length, salt, 15u, 32u, 2u,
        comparison, 32u) == CLM_CRYPTO_ERROR_INVALID_ARGUMENT &&
        clm_crypto_derive_key_bytes(message, length, salt, 16u, 7u, 2u,
        comparison, 32u) == CLM_CRYPTO_ERROR_INVALID_ARGUMENT &&
        clm_crypto_derive_key_bytes(message, length, salt, 16u, 32u, 0u,
        comparison, 32u) == CLM_CRYPTO_ERROR_INVALID_ARGUMENT &&
        clm_crypto_test_filled(comparison, 32u, 0xa5),
        "reject invalid Argon2id inputs before allocating or overwriting output");

    /* RFC 7748, section 6.1 (Alice's secret, Bob's public key and shared secret). */
    unsigned char alice_secret[32], bob_public[32], expected_shared[32], shared[32];
    CLM_ASSERT(clm_crypto_hex_decode(
        "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a",
        alice_secret, 32u, NULL) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_hex_decode(
        "de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f",
        bob_public, 32u, NULL) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_hex_decode(
        "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742",
        expected_shared, 32u, NULL) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_x25519_shared(alice_secret, bob_public, shared, 32u) ==
            CLM_CRYPTO_SUCCESS && memcmp(shared, expected_shared, 32u) == 0,
        "match RFC 7748 X25519 shared-secret vector");
    memset(bob_public, 0, 32u);
    memset(shared, 0xa5, 32u);
    CLM_ASSERT(clm_crypto_x25519_shared(alice_secret, bob_public, shared, 32u) ==
        CLM_CRYPTO_ERROR_AUTHENTICATION && clm_crypto_test_filled(shared, 32u, 0xa5),
        "reject all-zero X25519 peer result without publishing it");

    unsigned char secret_a[32], public_a[32], secret_b[32], public_b[32];
    CLM_ASSERT(clm_crypto_x25519_key_pair(secret_a, 32u, public_a, 32u) ==
        CLM_CRYPTO_SUCCESS &&
        clm_crypto_x25519_key_pair(secret_b, 32u, public_b, 32u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_x25519_shared(secret_a, public_b, derived, 32u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_x25519_shared(secret_b, public_a, comparison, 32u) ==
            CLM_CRYPTO_SUCCESS && memcmp(derived, comparison, 32u) == 0,
        "generate independent X25519 pairs and agree on the same raw secret");
    CLM_ASSERT(clm_crypto_random_bytes(NULL, 0u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_random_bytes(NULL, 1u) == CLM_CRYPTO_ERROR_INVALID_ARGUMENT &&
        clm_crypto_random_bytes(comparison, sizeof(comparison)) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_key_generate(algorithm, comparison, sizeof(comparison)) ==
            CLM_CRYPTO_SUCCESS &&
        clm_crypto_nonce_generate(algorithm, comparison, sizeof(comparison)) ==
            CLM_CRYPTO_SUCCESS &&
        clm_crypto_iv_generate(algorithm, comparison, sizeof(comparison)) ==
            CLM_CRYPTO_SUCCESS &&
        clm_crypto_random_to(block_id) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_key_generate_to(algorithm, key_id) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_nonce_generate_to(algorithm, nonce_id) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_key_generate_to(algorithm, nonce_id) ==
            CLM_CRYPTO_ERROR_INVALID_ARGUMENT,
        "use OS randomness for raw buffers and correctly sized BPA destinations");

    CLM_ASSERT(clm_crypto_secure_zero(comparison, sizeof(comparison)) ==
        CLM_CRYPTO_SUCCESS && clm_crypto_test_filled(comparison, sizeof(comparison), 0u) &&
        clm_crypto_secure_zero(NULL, 0u) == CLM_CRYPTO_SUCCESS &&
        clm_crypto_secure_zero(NULL, 1u) == CLM_CRYPTO_ERROR_INVALID_ARGUMENT,
        "wipe caller-owned bytes and validate zero-length erasure");

    crypto_wipe(key, sizeof(key));
    crypto_wipe(secret_a, sizeof(secret_a));
    crypto_wipe(secret_b, sizeof(secret_b));
    crypto_wipe(alice_secret, sizeof(alice_secret));
    crypto_wipe(shared, sizeof(shared));
    crypto_wipe(derived, sizeof(derived));
    crypto_wipe(plaintext, sizeof(plaintext));
    const clm_id blocks[] = {block_id, encrypted_id, decrypted_id, key_id, nonce_id};
    int cleaned = 1;
    for (size_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); ++i) {
        clm_bpa_block_node* block = clm_bpa_find_block(blocks[i]);
        if (block != NULL) crypto_wipe(block->data, block->block_size);
        cleaned &= clm_bpa_delete(blocks[i]) == CLM_BPA_SUCCESS;
    }
    cleaned &= clm_string_delete(source_id);
    cleaned &= clm_string_delete(hex_id);
    cleaned &= clm_string_delete(text_id);
    CLM_ASSERT(cleaned, "release all crypto string and BPA fixtures");
    clm_test_summary(result.passed, result.failed);
    return result.failed != 0;
}


static int test_clm_crypto(void)
{
    return clm_crypto_tests();
}

