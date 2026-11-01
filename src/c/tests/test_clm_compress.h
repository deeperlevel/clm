#pragma once

#include "test.h"
#include "../clm/compress.h"
#include "../clm/gen.h"

static const clm_compress_algorithm clm_compress_test_algorithms[] = {
    CLM_COMPRESS_RAW_DEFLATE, CLM_COMPRESS_ZLIB, CLM_COMPRESS_GZIP, CLM_COMPRESS_LZ4, CLM_COMPRESS_BROTLI};

static int clm_compress_test_round_trip(
    const clm_compress_algorithm algorithm, const clm_int level, const clm_bytes_view input, size_t* packed_size)
{
    clm_compress_options options = {0};
    options.level = level;
    clm_bytes_buffer packed = {0};
    clm_bytes_buffer unpacked = {0};
    const clm_int encode_status = clm_compress_encode(algorithm, input, &options, &packed);
    const clm_int decode_status = clm_compress_decode(algorithm, clm_bytes_buffer_view(&packed), NULL, &unpacked);
    int ok = encode_status == CLM_COMPRESS_SUCCESS && decode_status == CLM_COMPRESS_SUCCESS &&
             clm_bytes_view_equals(clm_bytes_buffer_view(&unpacked), input);
    if (!ok)
        clm_test_write_format("      %s level %d: encode %d (%zu bytes), decode %d (%zu of %zu bytes)\n",
                              clm_compress_algorithm_name(algorithm), (int)level, (int)encode_status,
                              clm_bytes_buffer_length(&packed), (int)decode_status, clm_bytes_buffer_length(&unpacked),
                              input.length);
    if (packed_size != NULL) *packed_size = clm_bytes_buffer_length(&packed);
    clm_bytes_buffer_dispose(&packed);
    clm_bytes_buffer_dispose(&unpacked);
    return ok;
}

static clm_int clm_compress_test_decode_status(
    const clm_compress_algorithm algorithm, const unsigned char* data, const size_t length)
{
    clm_bytes_buffer out = {0};
    const clm_int status = clm_compress_decode(algorithm, (clm_bytes_view){data, length}, NULL, &out);
    const int untouched = clm_bytes_buffer_length(&out) == 0;
    clm_bytes_buffer_dispose(&out);
    return untouched || status == CLM_COMPRESS_SUCCESS ? status : CLM_COMPRESS_FAILURE;
}

static int clm_compress_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Compress");
#define CLM_COMPRESS_CHECK(flag, algorithm, condition)                                                   \
    do {                                                                                                 \
        if (!(condition)) {                                                                              \
            (flag) = 0;                                                                                  \
            clm_test_write_format("      %s failed: %s\n", clm_compress_algorithm_name(algorithm), #condition); \
        }                                                                                                \
    } while (0)

    clm_bytes_buffer text = {0};
    for (int i = 0; i < 200; ++i)
        clm_bytes_buffer_append_string(&text, "The quick brown fox jumps over the lazy dog. ");
    const clm_bytes_view text_view = clm_bytes_buffer_view(&text);

    unsigned char noise[65536];
    unsigned long long state = 0x9E3779B97F4A7C15ull;
    for (size_t i = 0; i < sizeof(noise); ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        noise[i] = (unsigned char)(state >> 24);
    }
    noise[100] = 0;
    noise[101] = 0;
    const clm_bytes_view noise_view = {noise, sizeof(noise)};

    int all_text = 1;
    int all_noise = 1;
    int all_empty = 1;
    int all_levels = 1;
    int smaller = 1;
    for (size_t i = 0; i < sizeof(clm_compress_test_algorithms) / sizeof(clm_compress_test_algorithms[0]); ++i) {
        const clm_compress_algorithm algorithm = clm_compress_test_algorithms[i];
        size_t packed_size = 0;
        all_text &= clm_compress_test_round_trip(algorithm, 0, text_view, &packed_size);
        smaller &= packed_size < text_view.length / 4;
        CLM_COMPRESS_CHECK(all_noise, algorithm, clm_compress_test_round_trip(algorithm, 0, noise_view, NULL));
        CLM_COMPRESS_CHECK(all_empty, algorithm, clm_compress_test_round_trip(algorithm, 0, (clm_bytes_view){NULL, 0}, NULL));
        for (clm_int level = 1; level <= 9; level += 4)
            all_levels &= clm_compress_test_round_trip(algorithm, level, text_view, NULL);
    }
    CLM_ASSERT(all_text && smaller, "round-trip repetitive text through every format and shrink it");
    CLM_ASSERT(all_noise && all_empty, "round-trip incompressible binary data with NUL bytes and empty input");
    CLM_ASSERT(all_levels, "round-trip at the fastest, middle, and smallest levels");

    const unsigned char gzip_hello[] = {0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xcb, 0x48, 0xcd, 0xc9,
                                        0xc9, 0xe7, 0x02, 0x00, 0x20, 0x30, 0x3a, 0x36, 0x06, 0x00, 0x00, 0x00};
    const unsigned char zlib_hello[] = {0x78, 0x9c, 0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00, 0x06, 0x2c, 0x02, 0x15};
    const unsigned char raw_hello[] = {0xcb, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00};
    clm_bytes_buffer out = {0};
    CLM_ASSERT(clm_compress_decode(CLM_COMPRESS_GZIP, (clm_bytes_view){gzip_hello, sizeof(gzip_hello)}, NULL, &out) ==
                   CLM_COMPRESS_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&out), clm_bytes_view_from_string("hello\n")),
               "decode a gzip stream produced by another tool");
    clm_bytes_buffer_clear(&out);
    CLM_ASSERT(clm_compress_decode(CLM_COMPRESS_ZLIB, (clm_bytes_view){zlib_hello, sizeof(zlib_hello)}, NULL, &out) ==
                   CLM_COMPRESS_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&out), clm_bytes_view_from_string("hello")) &&
                   clm_compress_decode(CLM_COMPRESS_RAW_DEFLATE, (clm_bytes_view){raw_hello, sizeof(raw_hello)}, NULL,
                                       &out) == CLM_COMPRESS_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&out), clm_bytes_view_from_string("hellohello")),
               "decode zlib and raw deflate streams produced by another tool");
    clm_bytes_buffer_clear(&out);
    CLM_ASSERT(clm_compress_encode_string(CLM_COMPRESS_ZLIB, "hello", NULL, &out) == CLM_COMPRESS_SUCCESS &&
                   clm_bytes_buffer_length(&out) > 2 && clm_bytes_buffer_view(&out).data[0] == 0x78,
               "produce a standard zlib header");
    clm_bytes_buffer_clear(&out);
    const unsigned char hello_trailer[8] = {0x86, 0xa6, 0x10, 0x36, 0x05, 0x00, 0x00, 0x00};
    CLM_ASSERT(clm_compress_encode_string(CLM_COMPRESS_GZIP, "hello", NULL, &out) == CLM_COMPRESS_SUCCESS &&
                   clm_bytes_buffer_length(&out) > 18 && memcmp(clm_bytes_buffer_view(&out).data, gzip_hello, 3) == 0 &&
                   memcmp(clm_bytes_buffer_view(&out).data + clm_bytes_buffer_length(&out) - 8, hello_trailer, 8) == 0,
               "produce a standard gzip header and a CRC-32 and size trailer");
    clm_bytes_buffer_clear(&out);

    int truncated_ok = 1;
    int trailing_ok = 1;
    int corrupt_ok = 1;
    for (size_t i = 0; i < sizeof(clm_compress_test_algorithms) / sizeof(clm_compress_test_algorithms[0]); ++i) {
        const clm_compress_algorithm algorithm = clm_compress_test_algorithms[i];
        clm_bytes_buffer packed = {0};
        clm_compress_encode(algorithm, text_view, NULL, &packed);
        const size_t length = clm_bytes_buffer_length(&packed);
        unsigned char* bytes = (unsigned char*)calloc(length + 8, 1);
        memcpy(bytes, clm_bytes_buffer_view(&packed).data, length);
        CLM_COMPRESS_CHECK(truncated_ok, algorithm, clm_compress_test_decode_status(algorithm, bytes, length - 1) == CLM_COMPRESS_TRUNCATED);
        CLM_COMPRESS_CHECK(truncated_ok, algorithm, clm_compress_test_decode_status(algorithm, bytes, length / 2) == CLM_COMPRESS_TRUNCATED);
        CLM_COMPRESS_CHECK(truncated_ok, algorithm, clm_compress_test_decode_status(algorithm, bytes, 0) == CLM_COMPRESS_TRUNCATED);
        bytes[length] = 0x00;
        CLM_COMPRESS_CHECK(trailing_ok, algorithm, clm_compress_test_decode_status(algorithm, bytes, length + 8) == CLM_COMPRESS_CORRUPT_DATA);
        bytes[length] = 0;
        bytes[length / 2] ^= 0x5a;
        const clm_int flipped = clm_compress_test_decode_status(algorithm, bytes, length);
        if (algorithm == CLM_COMPRESS_RAW_DEFLATE || algorithm == CLM_COMPRESS_BROTLI) corrupt_ok &= 1;
        else corrupt_ok &= flipped == CLM_COMPRESS_CORRUPT_DATA;
        free(bytes);
        clm_bytes_buffer_dispose(&packed);
    }
    CLM_ASSERT(truncated_ok, "report truncated streams of every format");
    CLM_ASSERT(trailing_ok, "reject bytes after the end of a stream");
    CLM_ASSERT(corrupt_ok, "detect corruption through checksums");

    unsigned char* zeros = (unsigned char*)calloc(4u * 1024 * 1024, 1);
    int bomb_ok = 1;
    int bomb_small = 1;
    clm_compress_options capped = {0};
    capped.max_output = 100000;
    for (size_t i = 0; i < sizeof(clm_compress_test_algorithms) / sizeof(clm_compress_test_algorithms[0]); ++i) {
        const clm_compress_algorithm algorithm = clm_compress_test_algorithms[i];
        clm_bytes_buffer bomb = {0};
        clm_bytes_buffer kept = {0};
        clm_bytes_buffer_append_string(&kept, "keep");
        bomb_ok &= clm_compress_encode(algorithm, (clm_bytes_view){zeros, 4u * 1024 * 1024}, NULL, &bomb) ==
                   CLM_COMPRESS_SUCCESS;
        bomb_small &= clm_bytes_buffer_length(&bomb) < 100000;
        bomb_ok &= clm_compress_decode(algorithm, clm_bytes_buffer_view(&bomb), &capped, &kept) ==
                   CLM_COMPRESS_LIMIT_EXCEEDED;
        bomb_ok &= clm_bytes_view_equals(clm_bytes_buffer_view(&kept), clm_bytes_view_from_string("keep"));
        capped.max_output = 4u * 1024 * 1024;
        bomb_ok &= clm_compress_decode(algorithm, clm_bytes_buffer_view(&bomb), &capped, &kept) == CLM_COMPRESS_SUCCESS &&
                   clm_bytes_buffer_length(&kept) == 4 + 4u * 1024 * 1024;
        capped.max_output = 100000;
        clm_bytes_buffer_dispose(&bomb);
        clm_bytes_buffer_dispose(&kept);
    }
    free(zeros);
    CLM_ASSERT(bomb_small && bomb_ok, "cap decoded output so a small input cannot expand without bound");

    clm_bytes_buffer limited = {0};
    clm_bytes_buffer_set_limit(&limited, 16);
    int limit_ok = 1;
    for (size_t i = 0; i < sizeof(clm_compress_test_algorithms) / sizeof(clm_compress_test_algorithms[0]); ++i)
        limit_ok &= clm_compress_encode(clm_compress_test_algorithms[i], noise_view, NULL, &limited) ==
                        CLM_COMPRESS_LIMIT_EXCEEDED && clm_bytes_buffer_length(&limited) == 0;
    CLM_ASSERT(limit_ok && clm_compress_encode(99, text_view, NULL, &limited) == CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_encode(CLM_COMPRESS_GZIP, text_view, &(clm_compress_options){.level = 10}, &limited) ==
                       CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_encode(CLM_COMPRESS_GZIP, (clm_bytes_view){NULL, 1}, NULL, &limited) ==
                       CLM_COMPRESS_INVALID_ARGUMENT,
               "respect an output buffer limit and reject invalid arguments");
    clm_bytes_buffer_dispose(&limited);

    int piece_ok = 1;
    const size_t piece_sizes[] = {1, 7, 4096};
    for (size_t a = 0; a < sizeof(clm_compress_test_algorithms) / sizeof(clm_compress_test_algorithms[0]); ++a) {
        for (size_t p = 0; p < sizeof(piece_sizes) / sizeof(piece_sizes[0]); ++p) {
            const size_t piece = piece_sizes[p];
            const clm_bytes_view source = piece == 1 ? (clm_bytes_view){text_view.data, 3000} : text_view;
            clm_compress_codec* encoder = NULL;
            clm_compress_codec* decoder = NULL;
            clm_bytes_buffer packed = {0};
            clm_bytes_buffer unpacked = {0};
            piece_ok &= clm_compress_codec_open(&encoder, clm_compress_test_algorithms[a], CLM_COMPRESS_MODE_ENCODE, NULL) ==
                        CLM_COMPRESS_SUCCESS;
            for (size_t at = 0; at < source.length; at += piece) {
                const size_t n = source.length - at < piece ? source.length - at : piece;
                piece_ok &= clm_compress_codec_write(encoder, source.data + at, n, &packed) == CLM_COMPRESS_SUCCESS;
            }
            piece_ok &= clm_compress_codec_finish(encoder, &packed) == CLM_COMPRESS_SUCCESS &&
                        clm_compress_codec_write(encoder, "x", 1, &packed) == CLM_COMPRESS_INVALID_ARGUMENT;
            piece_ok &= clm_compress_codec_open(&decoder, clm_compress_test_algorithms[a], CLM_COMPRESS_MODE_DECODE, NULL) ==
                        CLM_COMPRESS_SUCCESS;
            const clm_bytes_view wire = clm_bytes_buffer_view(&packed);
            for (size_t at = 0; at < wire.length; at += piece) {
                const size_t n = wire.length - at < piece ? wire.length - at : piece;
                piece_ok &= clm_compress_codec_write(decoder, wire.data + at, n, &unpacked) == CLM_COMPRESS_SUCCESS;
            }
            piece_ok &= clm_compress_codec_finish(decoder, &unpacked) == CLM_COMPRESS_SUCCESS &&
                        clm_bytes_view_equals(clm_bytes_buffer_view(&unpacked), source);
            clm_compress_codec_close(encoder);
            clm_compress_codec_close(decoder);
            clm_bytes_buffer_dispose(&packed);
            clm_bytes_buffer_dispose(&unpacked);
        }
    }
    CLM_ASSERT(piece_ok, "encode and decode incrementally in pieces of one byte and up");

    clm_compress_codec* poisoned = NULL;
    clm_bytes_buffer scratch = {0};
    CLM_ASSERT(clm_compress_codec_open(&poisoned, CLM_COMPRESS_GZIP, CLM_COMPRESS_MODE_DECODE, NULL) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_codec_write(poisoned, "not gzip at all", 15, &scratch) == CLM_COMPRESS_CORRUPT_DATA &&
                   clm_compress_codec_write(poisoned, "more", 4, &scratch) == CLM_COMPRESS_CORRUPT_DATA &&
                   clm_compress_codec_finish(poisoned, &scratch) == CLM_COMPRESS_CORRUPT_DATA &&
                   clm_compress_codec_open(NULL, CLM_COMPRESS_GZIP, CLM_COMPRESS_MODE_DECODE, NULL) ==
                       CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_codec_open(&poisoned, CLM_COMPRESS_GZIP, 0, NULL) == CLM_COMPRESS_INVALID_ARGUMENT,
               "keep reporting the first error from a failed codec");
    clm_compress_codec_close(poisoned);
    clm_bytes_buffer_dispose(&scratch);

    const clm_id text_id = 77700;
    const clm_id result_id = 77701;
    clm_bytes_buffer packed = {0};
    CLM_ASSERT(clm_string_new(text_id, "Grüße, 世界! Grüße, 世界! Grüße, 世界!") && clm_string_new(result_id, "old") &&
                   clm_compress_encode_string_id(CLM_COMPRESS_BROTLI, text_id, NULL, &packed) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_decode_to_string_id(CLM_COMPRESS_BROTLI, clm_bytes_buffer_view(&packed), NULL, result_id) ==
                       CLM_COMPRESS_SUCCESS &&
                   strcmp((const char*)clm_bpa_find_block(result_id)->data, "Grüße, 世界! Grüße, 世界! Grüße, 世界!") == 0,
               "compress and restore a CLM string with non-ASCII text");
    clm_bytes_buffer_clear(&packed);
    const unsigned char invalid_utf8[] = {'a', 0xC3, 0x28, 'b'};
    const unsigned char with_nul[] = {'a', 0, 'b'};
    clm_compress_encode(CLM_COMPRESS_GZIP, (clm_bytes_view){invalid_utf8, sizeof(invalid_utf8)}, NULL, &packed);
    clm_bytes_buffer second = {0};
    clm_compress_encode(CLM_COMPRESS_GZIP, (clm_bytes_view){with_nul, sizeof(with_nul)}, NULL, &second);
    CLM_ASSERT(clm_compress_decode_to_string_id(CLM_COMPRESS_GZIP, clm_bytes_buffer_view(&packed), NULL, result_id) ==
                   CLM_COMPRESS_FAILURE &&
                   clm_compress_decode_to_string_id(CLM_COMPRESS_GZIP, clm_bytes_buffer_view(&second), NULL, result_id) ==
                       CLM_COMPRESS_FAILURE &&
                   strncmp((const char*)clm_bpa_find_block(result_id)->data, "Gr", 2) == 0 &&
                   clm_compress_decode_to_string_id(CLM_COMPRESS_GZIP, clm_bytes_buffer_view(&second), NULL, 77999) ==
                       CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_encode_string_id(CLM_COMPRESS_GZIP, 77999, NULL, &second) == CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_encode_string(CLM_COMPRESS_GZIP, NULL, NULL, &second) == CLM_COMPRESS_INVALID_ARGUMENT,
               "leave the destination string unchanged unless the result is valid text");
    clm_bytes_buffer_dispose(&second);
    CLM_ASSERT(clm_string_delete(text_id) && clm_string_delete(result_id), "release the compress test strings");
    clm_bytes_buffer_dispose(&packed);

    clm_compress_algorithm found = 0;
    CLM_ASSERT(clm_compress_algorithm_from_name("GZip", &found) == CLM_COMPRESS_SUCCESS && found == CLM_COMPRESS_GZIP &&
                   clm_compress_algorithm_from_name("deflate", &found) == CLM_COMPRESS_SUCCESS && found == CLM_COMPRESS_ZLIB &&
                   clm_compress_algorithm_from_name("br", &found) == CLM_COMPRESS_SUCCESS && found == CLM_COMPRESS_BROTLI &&
                   clm_compress_algorithm_from_name("lz4", &found) == CLM_COMPRESS_SUCCESS && found == CLM_COMPRESS_LZ4 &&
                   clm_compress_algorithm_from_name("zstd", &found) == CLM_COMPRESS_NOT_FOUND &&
                   strcmp(clm_compress_algorithm_name(CLM_COMPRESS_ZLIB), "deflate") == 0 &&
                   strcmp(clm_compress_algorithm_name(CLM_COMPRESS_BROTLI), "br") == 0 &&
                   clm_compress_algorithm_name(0) == NULL,
               "map HTTP content-coding names to formats and back");
    int detect_ok = 1;
    for (size_t i = 0; i < 3; ++i) {
        clm_bytes_buffer probe = {0};
        const clm_compress_algorithm algorithm = clm_compress_test_algorithms[i + 1];
        clm_compress_encode(algorithm, text_view, NULL, &probe);
        found = 0;
        detect_ok &= clm_compress_detect(clm_bytes_buffer_view(&probe), &found) == CLM_COMPRESS_SUCCESS &&
                     found == algorithm;
        clm_bytes_buffer_dispose(&probe);
    }
    CLM_ASSERT(detect_ok && clm_compress_detect(clm_bytes_view_from_string("plain text"), &found) == CLM_COMPRESS_NOT_FOUND &&
                   clm_compress_detect((clm_bytes_view){NULL, 0}, &found) == CLM_COMPRESS_NOT_FOUND,
               "recognize gzip, zlib, and LZ4 data by their signatures");

    int stream_ok = 1;
    for (size_t i = 0; i < sizeof(clm_compress_test_algorithms) / sizeof(clm_compress_test_algorithms[0]); ++i) {
        const clm_compress_algorithm algorithm = clm_compress_test_algorithms[i];
        clm_stream_source source;
        clm_stream_sink inner;
        clm_stream_sink encoder;
        clm_bytes_buffer wire = {0};
        clm_ulong copied = 0;
        clm_stream_options tiny = {0};
        tiny.buffer_size = 7;
        clm_stream_source_memory(&source, text_view);
        clm_stream_sink_buffer(&inner, &wire);
        CLM_COMPRESS_CHECK(stream_ok, algorithm, clm_compress_stream_sink(&encoder, &inner, algorithm, CLM_COMPRESS_MODE_ENCODE, NULL) == CLM_COMPRESS_SUCCESS);
        const clm_int encoded_status = clm_stream_copy(&source, &encoder, &tiny, &copied);
        CLM_COMPRESS_CHECK(stream_ok, algorithm, encoded_status == CLM_STREAM_SUCCESS && copied == text_view.length);
        clm_stream_sink_dispose(&encoder);
        clm_bytes_buffer plain = {0};
        CLM_COMPRESS_CHECK(stream_ok, algorithm, clm_compress_decode(algorithm, clm_bytes_buffer_view(&wire), NULL, &plain) == CLM_COMPRESS_SUCCESS && clm_bytes_view_equals(clm_bytes_buffer_view(&plain), text_view));
        clm_bytes_buffer_clear(&plain);

        clm_stream_source packed_source;
        clm_stream_source decoder;
        clm_stream_sink plain_sink;
        clm_stream_source_memory(&packed_source, clm_bytes_buffer_view(&wire));
        clm_stream_sink_buffer(&plain_sink, &plain);
        CLM_COMPRESS_CHECK(stream_ok, algorithm, clm_compress_stream_source(&decoder, &packed_source, algorithm, CLM_COMPRESS_MODE_DECODE, NULL) == CLM_COMPRESS_SUCCESS);
        const clm_int source_status = clm_stream_copy(&decoder, &plain_sink, &tiny, &copied);
        CLM_COMPRESS_CHECK(stream_ok, algorithm, source_status == CLM_STREAM_SUCCESS && clm_bytes_view_equals(clm_bytes_buffer_view(&plain), text_view));
        if (source_status != CLM_STREAM_SUCCESS) clm_test_write_format("      source status %d\n", (int)source_status);
        clm_stream_source_dispose(&decoder);
        clm_bytes_buffer_clear(&plain);

        clm_stream_sink decoding_sink;
        clm_stream_source_memory(&packed_source, clm_bytes_buffer_view(&wire));
        clm_stream_sink_buffer(&plain_sink, &plain);
        CLM_COMPRESS_CHECK(stream_ok, algorithm, clm_compress_stream_sink(&decoding_sink, &plain_sink, algorithm, CLM_COMPRESS_MODE_DECODE, NULL) == CLM_COMPRESS_SUCCESS);
        const clm_int sink_status = clm_stream_copy(&packed_source, &decoding_sink, &tiny, &copied);
        CLM_COMPRESS_CHECK(stream_ok, algorithm, sink_status == CLM_STREAM_SUCCESS && clm_bytes_view_equals(clm_bytes_buffer_view(&plain), text_view));
        if (sink_status != CLM_STREAM_SUCCESS) clm_test_write_format("      sink status %d\n", (int)sink_status);
        clm_stream_sink_dispose(&decoding_sink);
        clm_bytes_buffer_clear(&plain);

        clm_bytes_view cut = clm_bytes_buffer_view(&wire);
        cut.length -= 1;
        clm_stream_source_memory(&packed_source, cut);
        clm_stream_sink_buffer(&plain_sink, &plain);
        CLM_COMPRESS_CHECK(stream_ok, algorithm, clm_compress_stream_source(&decoder, &packed_source, algorithm, CLM_COMPRESS_MODE_DECODE, NULL) == CLM_COMPRESS_SUCCESS);
        const clm_int cut_status = clm_stream_copy(&decoder, &plain_sink, NULL, &copied);
        CLM_COMPRESS_CHECK(stream_ok, algorithm, cut_status == CLM_STREAM_TRUNCATED);
        if (cut_status != CLM_STREAM_TRUNCATED) clm_test_write_format("      cut status %d\n", (int)cut_status);
        clm_stream_source_dispose(&decoder);
        clm_bytes_buffer_dispose(&plain);
        clm_bytes_buffer_dispose(&wire);
    }
    CLM_ASSERT(stream_ok, "encode and decode through stream sources and sinks and report truncated input");

    unsigned char* zeros_again = (unsigned char*)calloc(4u * 1024 * 1024, 1);
    clm_bytes_buffer bomb = {0};
    clm_compress_encode(CLM_COMPRESS_GZIP, (clm_bytes_view){zeros_again, 4u * 1024 * 1024}, NULL, &bomb);
    clm_stream_source bomb_source;
    clm_stream_source bomb_decoder;
    clm_stream_sink discard;
    clm_compress_options small_cap = {0};
    small_cap.max_output = 50000;
    clm_stream_source_memory(&bomb_source, clm_bytes_buffer_view(&bomb));
    clm_stream_sink_discard(&discard);
    CLM_ASSERT(clm_compress_stream_source(&bomb_decoder, &bomb_source, CLM_COMPRESS_GZIP, CLM_COMPRESS_MODE_DECODE,
                                          &small_cap) == CLM_COMPRESS_SUCCESS &&
                   clm_stream_copy(&bomb_decoder, &discard, NULL, NULL) == CLM_STREAM_LIMIT_EXCEEDED,
               "stop a decompression bomb inside a stream");
    clm_stream_source_dispose(&bomb_decoder);
    clm_bytes_buffer_dispose(&bomb);
    free(zeros_again);

    CLM_ASSERT(clm_compress_zip_name_is_safe("a/b.txt") && clm_compress_zip_name_is_safe("dir/") &&
                   clm_compress_zip_name_is_safe("a..b/c") && clm_compress_zip_name_is_safe("./x") &&
                   !clm_compress_zip_name_is_safe("") && !clm_compress_zip_name_is_safe("/etc/passwd") &&
                   !clm_compress_zip_name_is_safe("\\windows") && !clm_compress_zip_name_is_safe("C:\\x") &&
                   !clm_compress_zip_name_is_safe("../x") && !clm_compress_zip_name_is_safe("a/../x") &&
                   !clm_compress_zip_name_is_safe("a\\..\\x") && !clm_compress_zip_name_is_safe("a/..") &&
                   !clm_compress_zip_name_is_safe("a\nb") && !clm_compress_zip_name_is_safe(NULL),
               "judge which ZIP entry names are safe to extract");

    clm_compress_zip_writer writer;
    clm_bytes_buffer archive = {0};
    CLM_ASSERT(clm_compress_zip_writer_init(&writer) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_writer_add(&writer, "notes/readme.txt", text_view, 0) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_writer_add(&writer, "notes/", (clm_bytes_view){NULL, 0}, 0) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_writer_add(&writer, "data.bin", noise_view, 9) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_writer_add(&writer, "empty.txt", (clm_bytes_view){NULL, 0}, 1) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_writer_add(&writer, "../evil", text_view, 0) == CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_zip_writer_add(&writer, "dir/", text_view, 0) == CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_zip_writer_add(&writer, "x", text_view, 10) == CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_zip_writer_finish(&writer, &archive) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_writer_add(&writer, "late", text_view, 0) == CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_zip_writer_finish(&writer, &archive) == CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_bytes_buffer_view(&archive).data[0] == 'P' && clm_bytes_buffer_view(&archive).data[1] == 'K',
               "build a ZIP archive in memory");
    clm_compress_zip_writer_dispose(&writer);

    clm_compress_zip_reader reader;
    clm_compress_zip_entry entry;
    size_t index = 0;
    clm_bytes_buffer extracted = {0};
    CLM_ASSERT(clm_compress_zip_reader_open(&reader, clm_bytes_buffer_view(&archive)) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_reader_count(&reader) == 4 &&
                   clm_compress_zip_reader_find(&reader, "data.bin", &index) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_reader_entry(&reader, index, &entry) == CLM_COMPRESS_SUCCESS &&
                   strcmp(entry.name, "data.bin") == 0 && entry.size == sizeof(noise) && !entry.is_directory &&
                   clm_compress_zip_reader_extract(&reader, index, 0, &extracted) == CLM_COMPRESS_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&extracted), noise_view),
               "list, find, and extract entries of a ZIP archive");
    clm_bytes_buffer_clear(&extracted);
    CLM_ASSERT(clm_compress_zip_reader_find(&reader, "notes/readme.txt", &index) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_reader_entry(&reader, index, &entry) == CLM_COMPRESS_SUCCESS &&
                   entry.compressed_size < entry.size / 4 &&
                   clm_compress_zip_reader_extract(&reader, index, 0, &extracted) == CLM_COMPRESS_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&extracted), text_view) &&
                   clm_compress_zip_reader_extract(&reader, index, 100, &extracted) == CLM_COMPRESS_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_length(&extracted) == text_view.length,
               "compress entries and refuse to extract past the size cap");
    clm_bytes_buffer_clear(&extracted);
    CLM_ASSERT(clm_compress_zip_reader_find(&reader, "notes/", &index) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_reader_entry(&reader, index, &entry) == CLM_COMPRESS_SUCCESS && entry.is_directory &&
                   clm_compress_zip_reader_extract(&reader, index, 0, &extracted) == CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_zip_reader_find(&reader, "empty.txt", &index) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_reader_extract(&reader, index, 0, &extracted) == CLM_COMPRESS_SUCCESS &&
                   clm_bytes_buffer_length(&extracted) == 0 &&
                   clm_compress_zip_reader_find(&reader, "missing", &index) == CLM_COMPRESS_NOT_FOUND &&
                   clm_compress_zip_reader_entry(&reader, 99, &entry) == CLM_COMPRESS_INVALID_ARGUMENT &&
                   clm_compress_zip_reader_extract(&reader, 99, 0, &extracted) == CLM_COMPRESS_INVALID_ARGUMENT,
               "handle directories, empty files, and bad lookups");
    clm_compress_zip_reader_dispose(&reader);

    const size_t archive_length = clm_bytes_buffer_length(&archive);
    unsigned char* damaged = (unsigned char*)malloc(archive_length);
    memcpy(damaged, clm_bytes_buffer_view(&archive).data, archive_length);
    CLM_ASSERT(clm_compress_zip_reader_open(&reader, (clm_bytes_view){damaged, archive_length / 2}) == CLM_COMPRESS_CORRUPT_DATA &&
                   clm_compress_zip_reader_open(&reader, clm_bytes_view_from_string("not a zip archive")) ==
                       CLM_COMPRESS_CORRUPT_DATA &&
                   clm_compress_zip_reader_open(&reader, (clm_bytes_view){NULL, 0}) == CLM_COMPRESS_INVALID_ARGUMENT,
               "reject truncated and invalid archives");
    const size_t data_offset = 30 + strlen("notes/readme.txt") + 4;
    damaged[data_offset] ^= 0xff;
    damaged[data_offset + 1] ^= 0xff;
    clm_bytes_buffer_clear(&extracted);
    CLM_ASSERT(clm_compress_zip_reader_open(&reader, (clm_bytes_view){damaged, archive_length}) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_reader_find(&reader, "notes/readme.txt", &index) == CLM_COMPRESS_SUCCESS &&
                   clm_compress_zip_reader_extract(&reader, index, 0, &extracted) == CLM_COMPRESS_CORRUPT_DATA &&
                   clm_bytes_buffer_length(&extracted) == 0,
               "detect a damaged entry when it is extracted");
    clm_compress_zip_reader_dispose(&reader);
    free(damaged);
    clm_bytes_buffer_dispose(&extracted);
    clm_bytes_buffer_dispose(&archive);

    clm_bytes_buffer_dispose(&out);
    clm_bytes_buffer_dispose(&text);
    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
