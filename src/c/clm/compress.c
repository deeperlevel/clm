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

#include "compress.h"
#include "bpa.h"
#include "string.h"

#ifndef MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#endif
#include "ext/miniz/miniz.h"
#include "ext/miniz/miniz_common.h"
#include "ext/miniz/miniz_tdef.h"
#include "ext/miniz/miniz_tinfl.h"
#include "ext/miniz/miniz_zip.h"
#include "ext/lz4/lz4frame.h"
#include "ext/brotli/include/decode.h"
#include "ext/brotli/include/encode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHUNK 65536u
#define GZIP_HEADER_LIMIT 65536u

typedef enum { GZ_HEADER, GZ_BODY, GZ_TRAILER, GZ_DONE } gzip_stage;

struct clm_compress_codec {
    clm_compress_algorithm algorithm;
    clm_compress_mode mode;
    clm_int level;
    size_t max_output;
    size_t produced;
    clm_int error;
    bool finished;
    bool started;
    tdefl_compressor* deflater;
    tinfl_decompressor* inflater;
    unsigned char* dict;
    size_t dict_offset;
    bool inflate_done;
    gzip_stage stage;
    uint32_t crc;
    uint64_t total;
    clm_bytes_buffer staging;
    LZ4F_cctx* lz4_encoder;
    LZ4F_dctx* lz4_decoder;
    LZ4F_preferences_t lz4_preferences;
    size_t lz4_hint;
    BrotliEncoderState* brotli_encoder;
    BrotliDecoderState* brotli_decoder;
    BrotliDecoderResult brotli_result;
};

/* ---- helpers --------------------------------------------------------------- */

static clm_int map_bytes(const clm_int status)
{
    switch (status) {
    case CLM_BYTES_SUCCESS: return CLM_COMPRESS_SUCCESS;
    case CLM_BYTES_LIMIT_EXCEEDED: return CLM_COMPRESS_LIMIT_EXCEEDED;
    case CLM_BYTES_NO_MEMORY: return CLM_COMPRESS_NO_MEMORY;
    default: return CLM_COMPRESS_FAILURE;
    }
}

static clm_int prepare_out(clm_bytes_buffer* out, const size_t min_free, unsigned char** tail, size_t* available)
{
    return map_bytes(clm_bytes_buffer_prepare(out, min_free, tail, available));
}

static clm_int commit_out(clm_bytes_buffer* out, const size_t count)
{
    return count == 0 ? CLM_COMPRESS_SUCCESS : map_bytes(clm_bytes_buffer_commit(out, count));
}

static bool valid_algorithm(const clm_compress_algorithm algorithm)
{
    return algorithm >= CLM_COMPRESS_RAW_DEFLATE && algorithm <= CLM_COMPRESS_BROTLI;
}

/* Room for decoded bytes, never more than one byte past the cap so excess is noticed. */
static clm_int decode_space(clm_compress_codec* c, clm_bytes_buffer* out, unsigned char** tail, size_t* space)
{
    size_t want = CHUNK;
    const size_t allowance = c->max_output - c->produced;
    if (allowance < want) want = allowance + 1;
    size_t available = 0;
    const clm_int status = prepare_out(out, want < 4096 ? want : 4096, tail, &available);
    if (status != CLM_COMPRESS_SUCCESS) return status;
    *space = available < want ? available : want;
    return *space == 0 ? CLM_COMPRESS_FAILURE : CLM_COMPRESS_SUCCESS;
}

/* Checks the cap and counts decoded bytes; the bytes themselves are written elsewhere. */
static clm_int decode_account(clm_compress_codec* c, const unsigned char* data, const size_t count)
{
    if (count > c->max_output - c->produced) return CLM_COMPRESS_LIMIT_EXCEEDED;
    if (count == 0) return CLM_COMPRESS_SUCCESS;
    if (c->algorithm == CLM_COMPRESS_GZIP) {
        c->crc = (uint32_t)mz_crc32(c->crc, data, count);
        c->total += count;
    }
    c->produced += count;
    return CLM_COMPRESS_SUCCESS;
}

/* Accounts for bytes a decoder already wrote at the output tail, then commits them. */
static clm_int decode_commit(clm_compress_codec* c, clm_bytes_buffer* out, const unsigned char* data, const size_t count)
{
    const clm_int status = decode_account(c, data, count);
    return status != CLM_COMPRESS_SUCCESS ? status : commit_out(out, count);
}

/* ---- deflate family (miniz) ------------------------------------------------- */

static clm_int deflate_encode(
    clm_compress_codec* c, const unsigned char* data, size_t length, const bool finish, clm_bytes_buffer* out)
{
    static const unsigned char empty[1] = {0};
    static const unsigned char gzip_header[10] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 0xff};
    if (!c->started) {
        c->started = true;
        if (c->algorithm == CLM_COMPRESS_GZIP) {
            const clm_int status = map_bytes(clm_bytes_buffer_append(out, gzip_header, sizeof(gzip_header)));
            if (status != CLM_COMPRESS_SUCCESS) return status;
        }
    }
    if (c->algorithm == CLM_COMPRESS_GZIP && length != 0) {
        c->crc = (uint32_t)mz_crc32(c->crc, data, length);
        c->total += length;
    }
    int stalls = 0;
    for (;;) {
        unsigned char* tail = NULL;
        size_t available = 0;
        clm_int status = prepare_out(out, 1, &tail, &available);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        size_t in_size = length;
        size_t out_size = available;
        const tdefl_status result = tdefl_compress(
            c->deflater, length != 0 ? data : empty, &in_size, tail, &out_size, finish ? TDEFL_FINISH : TDEFL_NO_FLUSH);
        if (result < 0) return CLM_COMPRESS_FAILURE;
        status = commit_out(out, out_size);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        data += in_size;
        length -= in_size;
        if (finish ? result == TDEFL_STATUS_DONE : length == 0) break;
        if (in_size == 0 && out_size == 0) {
            if (++stalls > 2) return CLM_COMPRESS_FAILURE;
        } else {
            stalls = 0;
        }
    }
    if (finish && c->algorithm == CLM_COMPRESS_GZIP) {
        unsigned char trailer[8];
        const uint32_t size32 = (uint32_t)c->total;
        for (int i = 0; i < 4; ++i) {
            trailer[i] = (unsigned char)(c->crc >> (8 * i));
            trailer[4 + i] = (unsigned char)(size32 >> (8 * i));
        }
        return map_bytes(clm_bytes_buffer_append(out, trailer, sizeof(trailer)));
    }
    return CLM_COMPRESS_SUCCESS;
}

/* Parses a gzip header; SUCCESS reports its size, SUCCESS+1 means more bytes are needed. */
static clm_int gzip_header_size(const unsigned char* b, const size_t n, size_t* size)
{
    if (n >= 1 && b[0] != 0x1f) return CLM_COMPRESS_CORRUPT_DATA;
    if (n >= 2 && b[1] != 0x8b) return CLM_COMPRESS_CORRUPT_DATA;
    if (n >= 3 && b[2] != 8) return CLM_COMPRESS_CORRUPT_DATA;
    if (n >= 4 && (b[3] & 0xE0u) != 0) return CLM_COMPRESS_CORRUPT_DATA;
    if (n < 10) return CLM_COMPRESS_SUCCESS + 1;
    const unsigned flags = b[3];
    size_t pos = 10;
    if (flags & 4u) {
        if (n < pos + 2) return CLM_COMPRESS_SUCCESS + 1;
        pos += 2 + ((size_t)b[pos] | ((size_t)b[pos + 1] << 8));
        if (n < pos) return CLM_COMPRESS_SUCCESS + 1;
    }
    for (unsigned bit = 8; bit <= 16; bit <<= 1) {
        if (!(flags & bit)) continue;
        while (pos < n && b[pos] != 0) ++pos;
        if (pos >= n) return CLM_COMPRESS_SUCCESS + 1;
        ++pos;
    }
    if (flags & 2u) {
        pos += 2;
        if (n < pos) return CLM_COMPRESS_SUCCESS + 1;
    }
    *size = pos;
    return n >= pos ? CLM_COMPRESS_SUCCESS : CLM_COMPRESS_SUCCESS + 1;
}

static clm_int inflate_body(
    clm_compress_codec* c, const unsigned char** in, size_t* length, clm_bytes_buffer* out, const bool more)
{
    const mz_uint32 flags = (c->algorithm == CLM_COMPRESS_ZLIB ? TINFL_FLAG_PARSE_ZLIB_HEADER : 0u) |
                            (more ? TINFL_FLAG_HAS_MORE_INPUT : 0u);
    static const unsigned char none[1] = {0};
    for (;;) {
        size_t in_bytes = *length;
        size_t out_bytes = TINFL_LZ_DICT_SIZE - c->dict_offset;
        const tinfl_status status = tinfl_decompress(
            c->inflater, *length != 0 ? *in : none, &in_bytes, c->dict, c->dict + c->dict_offset, &out_bytes, flags);
        *in += in_bytes;
        *length -= in_bytes;
        if (out_bytes != 0) {
            const clm_int emitted = decode_account(c, c->dict + c->dict_offset, out_bytes);
            if (emitted != CLM_COMPRESS_SUCCESS) return emitted;
            if (clm_bytes_buffer_append(out, c->dict + c->dict_offset, out_bytes) != CLM_BYTES_SUCCESS)
                return CLM_COMPRESS_NO_MEMORY;
            c->dict_offset = (c->dict_offset + out_bytes) & (TINFL_LZ_DICT_SIZE - 1);
        }
        if (status == TINFL_STATUS_DONE) {
            c->inflate_done = true;
            return CLM_COMPRESS_SUCCESS;
        }
        if (status == TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS) return CLM_COMPRESS_TRUNCATED;
        if (status < 0) return CLM_COMPRESS_CORRUPT_DATA;
        if (status == TINFL_STATUS_NEEDS_MORE_INPUT) {
            if (!more) return CLM_COMPRESS_TRUNCATED;
            if (*length == 0) return CLM_COMPRESS_SUCCESS;
        }
    }
}

static clm_int inflate_write(clm_compress_codec* c, const unsigned char* in, size_t length, clm_bytes_buffer* out)
{
    while (length > 0) {
        if (c->algorithm == CLM_COMPRESS_GZIP) {
            if (c->stage == GZ_HEADER) {
                if (clm_bytes_buffer_length(&c->staging) >= GZIP_HEADER_LIMIT) return CLM_COMPRESS_CORRUPT_DATA;
                if (clm_bytes_buffer_append_byte(&c->staging, *in) != CLM_BYTES_SUCCESS) return CLM_COMPRESS_NO_MEMORY;
                ++in;
                --length;
                const clm_bytes_view head = clm_bytes_buffer_view(&c->staging);
                size_t size = 0;
                const clm_int parsed = gzip_header_size(head.data, head.length, &size);
                if (parsed < 0) return parsed;
                if (parsed == CLM_COMPRESS_SUCCESS) {
                    clm_bytes_buffer_clear(&c->staging);
                    c->stage = GZ_BODY;
                }
                continue;
            }
            if (c->stage == GZ_TRAILER) {
                if (clm_bytes_buffer_append_byte(&c->staging, *in) != CLM_BYTES_SUCCESS) return CLM_COMPRESS_NO_MEMORY;
                ++in;
                --length;
                if (clm_bytes_buffer_length(&c->staging) == 8) {
                    const unsigned char* t = clm_bytes_buffer_view(&c->staging).data;
                    const uint32_t crc = (uint32_t)t[0] | ((uint32_t)t[1] << 8) | ((uint32_t)t[2] << 16) | ((uint32_t)t[3] << 24);
                    const uint32_t size = (uint32_t)t[4] | ((uint32_t)t[5] << 8) | ((uint32_t)t[6] << 16) | ((uint32_t)t[7] << 24);
                    if (crc != c->crc || size != (uint32_t)c->total) return CLM_COMPRESS_CORRUPT_DATA;
                    c->stage = GZ_DONE;
                    clm_bytes_buffer_clear(&c->staging);
                }
                continue;
            }
            if (c->stage == GZ_DONE) return CLM_COMPRESS_CORRUPT_DATA;
        } else if (c->inflate_done) {
            return CLM_COMPRESS_CORRUPT_DATA;
        }
        const clm_int status = inflate_body(c, &in, &length, out, true);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        if (c->inflate_done && c->algorithm == CLM_COMPRESS_GZIP) c->stage = GZ_TRAILER;
    }
    return CLM_COMPRESS_SUCCESS;
}

static clm_int inflate_finish(clm_compress_codec* c, clm_bytes_buffer* out)
{
    const unsigned char* none = NULL;
    size_t zero = 0;
    if (c->algorithm == CLM_COMPRESS_GZIP) {
        if (c->stage == GZ_HEADER) return CLM_COMPRESS_TRUNCATED;
        if (c->stage == GZ_BODY) {
            const clm_int status = inflate_body(c, &none, &zero, out, false);
            if (status != CLM_COMPRESS_SUCCESS) return status;
            c->stage = GZ_TRAILER;
        }
        return c->stage == GZ_DONE ? CLM_COMPRESS_SUCCESS : CLM_COMPRESS_TRUNCATED;
    }
    return c->inflate_done ? CLM_COMPRESS_SUCCESS : inflate_body(c, &none, &zero, out, false);
}

/* ---- LZ4 frame -------------------------------------------------------------- */

static clm_int lz4_encode(
    clm_compress_codec* c, const unsigned char* data, size_t length, const bool finish, clm_bytes_buffer* out)
{
    unsigned char* tail = NULL;
    size_t available = 0;
    clm_int status;
    if (!c->started) {
        status = prepare_out(out, LZ4F_HEADER_SIZE_MAX, &tail, &available);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        const size_t written = LZ4F_compressBegin(c->lz4_encoder, tail, available, &c->lz4_preferences);
        if (LZ4F_isError(written)) return CLM_COMPRESS_FAILURE;
        status = commit_out(out, written);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        c->started = true;
    }
    while (length > 0) {
        const size_t chunk = length < CHUNK ? length : CHUNK;
        status = prepare_out(out, LZ4F_compressBound(chunk, &c->lz4_preferences), &tail, &available);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        const size_t written = LZ4F_compressUpdate(c->lz4_encoder, tail, available, data, chunk, NULL);
        if (LZ4F_isError(written)) return CLM_COMPRESS_FAILURE;
        status = commit_out(out, written);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        data += chunk;
        length -= chunk;
    }
    if (finish) {
        status = prepare_out(out, LZ4F_compressBound(0, &c->lz4_preferences), &tail, &available);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        const size_t written = LZ4F_compressEnd(c->lz4_encoder, tail, available, NULL);
        if (LZ4F_isError(written)) return CLM_COMPRESS_FAILURE;
        return commit_out(out, written);
    }
    return CLM_COMPRESS_SUCCESS;
}

static clm_int lz4_decode(clm_compress_codec* c, const unsigned char* data, size_t length, clm_bytes_buffer* out)
{
    bool again = true;
    while (again) {
        unsigned char* tail = NULL;
        size_t space = 0;
        clm_int status = decode_space(c, out, &tail, &space);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        size_t dst_size = space;
        size_t src_size = length;
        const size_t hint = LZ4F_decompress(c->lz4_decoder, tail, &dst_size, data, &src_size, NULL);
        if (LZ4F_isError(hint)) return CLM_COMPRESS_CORRUPT_DATA;
        status = decode_commit(c, out, tail, dst_size);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        data += src_size;
        length -= src_size;
        if (src_size != 0 || dst_size != 0) c->lz4_hint = hint;
        c->started = true;
        if (length > 0 && src_size == 0 && dst_size == 0) return CLM_COMPRESS_CORRUPT_DATA;
        again = length > 0 || dst_size == space;
    }
    return CLM_COMPRESS_SUCCESS;
}

/* ---- Brotli ----------------------------------------------------------------- */

static clm_int brotli_encode(
    clm_compress_codec* c, const unsigned char* data, const size_t length, const bool finish, clm_bytes_buffer* out)
{
    const uint8_t* next_in = data;
    size_t avail_in = length;
    int stalls = 0;
    for (;;) {
        unsigned char* tail = NULL;
        size_t available = 0;
        clm_int status = prepare_out(out, 64, &tail, &available);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        uint8_t* next_out = tail;
        size_t avail_out = available;
        const size_t before = avail_in;
        if (!BrotliEncoderCompressStream(c->brotli_encoder, finish ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_PROCESS,
                                         &avail_in, &next_in, &avail_out, &next_out, NULL))
            return CLM_COMPRESS_FAILURE;
        const size_t produced = available - avail_out;
        status = commit_out(out, produced);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        if (finish ? BrotliEncoderIsFinished(c->brotli_encoder) : avail_in == 0) break;
        if (before == avail_in && produced == 0) {
            if (++stalls > 2) return CLM_COMPRESS_FAILURE;
        } else {
            stalls = 0;
        }
    }
    return CLM_COMPRESS_SUCCESS;
}

static clm_int brotli_decode(clm_compress_codec* c, const unsigned char* data, const size_t length, clm_bytes_buffer* out)
{
    if (c->brotli_result == BROTLI_DECODER_RESULT_SUCCESS) return CLM_COMPRESS_CORRUPT_DATA;
    const uint8_t* next_in = data;
    size_t avail_in = length;
    for (;;) {
        unsigned char* tail = NULL;
        size_t space = 0;
        clm_int status = decode_space(c, out, &tail, &space);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        uint8_t* next_out = tail;
        size_t avail_out = space;
        const BrotliDecoderResult result = BrotliDecoderDecompressStream(
            c->brotli_decoder, &avail_in, &next_in, &avail_out, &next_out, NULL);
        status = decode_commit(c, out, tail, space - avail_out);
        if (status != CLM_COMPRESS_SUCCESS) return status;
        c->brotli_result = result;
        if (result == BROTLI_DECODER_RESULT_ERROR) return CLM_COMPRESS_CORRUPT_DATA;
        if (result == BROTLI_DECODER_RESULT_SUCCESS) return avail_in > 0 ? CLM_COMPRESS_CORRUPT_DATA : CLM_COMPRESS_SUCCESS;
        if (result == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) return CLM_COMPRESS_SUCCESS;
    }
}

/* ---- codec ------------------------------------------------------------------ */

void clm_compress_codec_close(clm_compress_codec* c)
{
    if (c == NULL) return;
    if (c->deflater != NULL) tdefl_compressor_free(c->deflater);
    if (c->inflater != NULL) tinfl_decompressor_free(c->inflater);
    free(c->dict);
    if (c->lz4_encoder != NULL) LZ4F_freeCompressionContext(c->lz4_encoder);
    if (c->lz4_decoder != NULL) LZ4F_freeDecompressionContext(c->lz4_decoder);
    if (c->brotli_encoder != NULL) BrotliEncoderDestroyInstance(c->brotli_encoder);
    if (c->brotli_decoder != NULL) BrotliDecoderDestroyInstance(c->brotli_decoder);
    clm_bytes_buffer_dispose(&c->staging);
    free(c);
}

clm_int clm_compress_codec_open(
    clm_compress_codec** out, const clm_compress_algorithm algorithm, const clm_compress_mode mode,
    const clm_compress_options* options)
{
    if (out == NULL) return CLM_COMPRESS_INVALID_ARGUMENT;
    *out = NULL;
    const clm_int level = options != NULL ? options->level : 0;
    if (!valid_algorithm(algorithm) || (mode != CLM_COMPRESS_MODE_ENCODE && mode != CLM_COMPRESS_MODE_DECODE) ||
        level < 0 || level > 9)
        return CLM_COMPRESS_INVALID_ARGUMENT;
    clm_compress_codec* c = (clm_compress_codec*)calloc(1, sizeof(*c));
    if (c == NULL) return CLM_COMPRESS_NO_MEMORY;
    c->algorithm = algorithm;
    c->mode = mode;
    c->level = level;
    c->max_output = options != NULL && options->max_output != 0 ? options->max_output : CLM_COMPRESS_DEFAULT_MAX_OUTPUT;
    c->brotli_result = BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT;
    bool ok = true;
    if (algorithm <= CLM_COMPRESS_GZIP) {
        if (mode == CLM_COMPRESS_MODE_ENCODE) {
            const int zip_level = level == 0 ? 6 : (int)level;
            c->deflater = tdefl_compressor_alloc();
            ok = c->deflater != NULL &&
                 tdefl_init(c->deflater, NULL, NULL,
                            (int)tdefl_create_comp_flags_from_zip_params(zip_level, algorithm == CLM_COMPRESS_ZLIB ? 15 : -15,
                                                                         MZ_DEFAULT_STRATEGY)) == TDEFL_STATUS_OKAY;
        } else {
            c->inflater = tinfl_decompressor_alloc();
            c->dict = (unsigned char*)malloc(TINFL_LZ_DICT_SIZE);
            ok = c->inflater != NULL && c->dict != NULL;
            if (ok) tinfl_init(c->inflater);
            c->stage = algorithm == CLM_COMPRESS_GZIP ? GZ_HEADER : GZ_BODY;
        }
    } else if (algorithm == CLM_COMPRESS_LZ4) {
        static const int lz4_levels[10] = {0, 0, 3, 4, 6, 8, 9, 10, 11, 12};
        if (mode == CLM_COMPRESS_MODE_ENCODE) {
            c->lz4_preferences.compressionLevel = lz4_levels[level];
            c->lz4_preferences.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
            ok = !LZ4F_isError(LZ4F_createCompressionContext(&c->lz4_encoder, LZ4F_VERSION));
        } else {
            ok = !LZ4F_isError(LZ4F_createDecompressionContext(&c->lz4_decoder, LZ4F_VERSION));
        }
    } else {
        static const int brotli_qualities[10] = {5, 1, 2, 3, 4, 5, 6, 8, 9, 11};
        if (mode == CLM_COMPRESS_MODE_ENCODE) {
            c->brotli_encoder = BrotliEncoderCreateInstance(NULL, NULL, NULL);
            ok = c->brotli_encoder != NULL &&
                 BrotliEncoderSetParameter(c->brotli_encoder, BROTLI_PARAM_QUALITY, (uint32_t)brotli_qualities[level]);
        } else {
            c->brotli_decoder = BrotliDecoderCreateInstance(NULL, NULL, NULL);
            ok = c->brotli_decoder != NULL;
        }
    }
    if (!ok) {
        clm_compress_codec_close(c);
        return CLM_COMPRESS_NO_MEMORY;
    }
    *out = c;
    return CLM_COMPRESS_SUCCESS;
}

static clm_int codec_process(
    clm_compress_codec* c, const unsigned char* data, const size_t length, const bool finish, clm_bytes_buffer* out)
{
    if (c->mode == CLM_COMPRESS_MODE_ENCODE) {
        switch (c->algorithm) {
        case CLM_COMPRESS_LZ4: return lz4_encode(c, data, length, finish, out);
        case CLM_COMPRESS_BROTLI: return brotli_encode(c, data, length, finish, out);
        default: return deflate_encode(c, data, length, finish, out);
        }
    }
    switch (c->algorithm) {
    case CLM_COMPRESS_LZ4:
        if (finish) return c->started && c->lz4_hint == 0 ? CLM_COMPRESS_SUCCESS : CLM_COMPRESS_TRUNCATED;
        return lz4_decode(c, data, length, out);
    case CLM_COMPRESS_BROTLI:
        if (finish) return c->brotli_result == BROTLI_DECODER_RESULT_SUCCESS ? CLM_COMPRESS_SUCCESS : CLM_COMPRESS_TRUNCATED;
        return brotli_decode(c, data, length, out);
    default:
        return finish ? inflate_finish(c, out) : inflate_write(c, data, length, out);
    }
}

clm_int clm_compress_codec_write(clm_compress_codec* c, const void* data, const size_t length, clm_bytes_buffer* out)
{
    if (c == NULL || out == NULL || (data == NULL && length != 0)) return CLM_COMPRESS_INVALID_ARGUMENT;
    if (c->error != CLM_COMPRESS_SUCCESS) return c->error;
    if (c->finished) return CLM_COMPRESS_INVALID_ARGUMENT;
    if (length == 0) return CLM_COMPRESS_SUCCESS;
    const clm_int status = codec_process(c, (const unsigned char*)data, length, false, out);
    if (status != CLM_COMPRESS_SUCCESS) c->error = status;
    return status;
}

clm_int clm_compress_codec_finish(clm_compress_codec* c, clm_bytes_buffer* out)
{
    if (c == NULL || out == NULL) return CLM_COMPRESS_INVALID_ARGUMENT;
    if (c->error != CLM_COMPRESS_SUCCESS) return c->error;
    if (c->finished) return CLM_COMPRESS_INVALID_ARGUMENT;
    const clm_int status = codec_process(c, NULL, 0, true, out);
    if (status != CLM_COMPRESS_SUCCESS) c->error = status;
    else c->finished = true;
    return status;
}

/* ---- bytes and strings ------------------------------------------------------- */

static clm_int run_codec(
    const clm_compress_algorithm algorithm, const clm_compress_mode mode, const clm_bytes_view data,
    const clm_compress_options* options, clm_bytes_buffer* out)
{
    if (out == NULL || (data.length != 0 && data.data == NULL)) return CLM_COMPRESS_INVALID_ARGUMENT;
    const size_t start = clm_bytes_buffer_length(out);
    clm_compress_codec* codec = NULL;
    clm_int status = clm_compress_codec_open(&codec, algorithm, mode, options);
    if (status != CLM_COMPRESS_SUCCESS) return status;
    status = clm_compress_codec_write(codec, data.data, data.length, out);
    if (status == CLM_COMPRESS_SUCCESS) status = clm_compress_codec_finish(codec, out);
    clm_compress_codec_close(codec);
    if (status != CLM_COMPRESS_SUCCESS) clm_bytes_buffer_resize(out, start, 0);
    return status;
}

clm_int clm_compress_encode(
    const clm_compress_algorithm algorithm, const clm_bytes_view data, const clm_compress_options* options,
    clm_bytes_buffer* out)
{
    return run_codec(algorithm, CLM_COMPRESS_MODE_ENCODE, data, options, out);
}

clm_int clm_compress_decode(
    const clm_compress_algorithm algorithm, const clm_bytes_view data, const clm_compress_options* options,
    clm_bytes_buffer* out)
{
    return run_codec(algorithm, CLM_COMPRESS_MODE_DECODE, data, options, out);
}

clm_int clm_compress_encode_string(
    const clm_compress_algorithm algorithm, const char* text, const clm_compress_options* options, clm_bytes_buffer* out)
{
    if (text == NULL) return CLM_COMPRESS_INVALID_ARGUMENT;
    return clm_compress_encode(algorithm, clm_bytes_view_from_string(text), options, out);
}

clm_int clm_compress_encode_string_id(
    const clm_compress_algorithm algorithm, const clm_id string_id, const clm_compress_options* options,
    clm_bytes_buffer* out)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(string_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || (block->length != 0 && block->data == NULL))
        return CLM_COMPRESS_INVALID_ARGUMENT;
    const clm_bytes_view text = {(const unsigned char*)block->data, block->length};
    return clm_compress_encode(algorithm, text, options, out);
}

clm_int clm_compress_decode_to_string_id(
    const clm_compress_algorithm algorithm, const clm_bytes_view data, const clm_compress_options* options,
    const clm_id destination_id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(destination_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING) return CLM_COMPRESS_INVALID_ARGUMENT;
    clm_bytes_buffer text = {0};
    clm_int status = clm_compress_decode(algorithm, data, options, &text);
    if (status == CLM_COMPRESS_SUCCESS) {
        const clm_bytes_view view = clm_bytes_buffer_view(&text);
        if ((view.length != 0 && memchr(view.data, '\0', view.length) != NULL) ||
            clm_bytes_buffer_append_byte(&text, 0) != CLM_BYTES_SUCCESS ||
            clm_string_set(destination_id, (const char*)clm_bytes_buffer_view(&text).data) == CLM_BPA_FAILURE)
            status = CLM_COMPRESS_FAILURE;
    }
    clm_bytes_buffer_dispose(&text);
    return status;
}

static bool equals_ignore_case(const char* a, const char* b)
{
    for (; *a != '\0' && *b != '\0'; ++a, ++b) {
        const char x = (*a >= 'A' && *a <= 'Z') ? (char)(*a + 32) : *a;
        const char y = (*b >= 'A' && *b <= 'Z') ? (char)(*b + 32) : *b;
        if (x != y) return false;
    }
    return *a == *b;
}

clm_int clm_compress_algorithm_from_name(const char* name, clm_compress_algorithm* out)
{
    static const struct { const char* name; clm_compress_algorithm algorithm; } names[] = {
        {"gzip", CLM_COMPRESS_GZIP}, {"x-gzip", CLM_COMPRESS_GZIP}, {"deflate", CLM_COMPRESS_ZLIB},
        {"zlib", CLM_COMPRESS_ZLIB}, {"raw-deflate", CLM_COMPRESS_RAW_DEFLATE}, {"lz4", CLM_COMPRESS_LZ4},
        {"br", CLM_COMPRESS_BROTLI}, {"brotli", CLM_COMPRESS_BROTLI}};
    if (name == NULL || out == NULL) return CLM_COMPRESS_INVALID_ARGUMENT;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (equals_ignore_case(name, names[i].name)) {
            *out = names[i].algorithm;
            return CLM_COMPRESS_SUCCESS;
        }
    }
    return CLM_COMPRESS_NOT_FOUND;
}

const char* clm_compress_algorithm_name(const clm_compress_algorithm algorithm)
{
    switch (algorithm) {
    case CLM_COMPRESS_RAW_DEFLATE: return "raw-deflate";
    case CLM_COMPRESS_ZLIB: return "deflate";
    case CLM_COMPRESS_GZIP: return "gzip";
    case CLM_COMPRESS_LZ4: return "lz4";
    case CLM_COMPRESS_BROTLI: return "br";
    default: return NULL;
    }
}

clm_int clm_compress_detect(const clm_bytes_view data, clm_compress_algorithm* out)
{
    if (out == NULL || (data.length != 0 && data.data == NULL)) return CLM_COMPRESS_INVALID_ARGUMENT;
    const unsigned char* b = data.data;
    if (data.length >= 2 && b[0] == 0x1f && b[1] == 0x8b) {
        *out = CLM_COMPRESS_GZIP;
        return CLM_COMPRESS_SUCCESS;
    }
    if (data.length >= 4 && b[0] == 0x04 && b[1] == 0x22 && b[2] == 0x4d && b[3] == 0x18) {
        *out = CLM_COMPRESS_LZ4;
        return CLM_COMPRESS_SUCCESS;
    }
    if (data.length >= 2 && (b[0] & 0x0f) == 8 && (b[0] >> 4) <= 7 && (((unsigned)b[0] << 8) | b[1]) % 31 == 0) {
        *out = CLM_COMPRESS_ZLIB;
        return CLM_COMPRESS_SUCCESS;
    }
    return CLM_COMPRESS_NOT_FOUND;
}

/* ---- stream adapters --------------------------------------------------------- */

static clm_int stream_status(const clm_int status)
{
    switch (status) {
    case CLM_COMPRESS_SUCCESS: return CLM_STREAM_SUCCESS;
    case CLM_COMPRESS_LIMIT_EXCEEDED: return CLM_STREAM_LIMIT_EXCEEDED;
    case CLM_COMPRESS_TRUNCATED: return CLM_STREAM_TRUNCATED;
    case CLM_COMPRESS_NO_MEMORY: return CLM_STREAM_NO_MEMORY;
    default: return CLM_STREAM_FAILURE;
    }
}

typedef struct {
    clm_compress_codec* codec;
    clm_stream_sink* inner;
    clm_bytes_buffer pending;
    bool finished;
    bool inner_finished;
} sink_state;

static clm_int sink_flush(sink_state* s)
{
    while (clm_bytes_buffer_length(&s->pending) != 0) {
        const clm_bytes_view view = clm_bytes_buffer_view(&s->pending);
        size_t written = 0;
        const clm_int status = s->inner->write(s->inner, view.data, view.length, &written);
        if (status != CLM_STREAM_SUCCESS) return status;
        if (written == 0 || written > view.length) return CLM_STREAM_FAILURE;
        clm_bytes_buffer_consume(&s->pending, written);
    }
    return CLM_STREAM_SUCCESS;
}

static clm_int sink_write(clm_stream_sink* self, const void* data, size_t length, size_t* out_written)
{
    sink_state* s = (sink_state*)self->user;
    if (data == NULL || length == 0 || out_written == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    clm_int status = sink_flush(s);
    if (status != CLM_STREAM_SUCCESS) return status;
    status = stream_status(clm_compress_codec_write(s->codec, data, length, &s->pending));
    if (status != CLM_STREAM_SUCCESS) return status;
    *out_written = length;
    status = sink_flush(s);
    return status == CLM_STREAM_WOULD_BLOCK ? CLM_STREAM_SUCCESS : status;
}

static clm_int sink_finish(clm_stream_sink* self)
{
    sink_state* s = (sink_state*)self->user;
    if (!s->finished) {
        const clm_int status = stream_status(clm_compress_codec_finish(s->codec, &s->pending));
        if (status != CLM_STREAM_SUCCESS) return status;
        s->finished = true;
    }
    clm_int status = sink_flush(s);
    if (status != CLM_STREAM_SUCCESS) return status;
    if (!s->inner_finished) {
        status = s->inner->finish != NULL ? s->inner->finish(s->inner) : CLM_STREAM_SUCCESS;
        if (status != CLM_STREAM_SUCCESS) return status;
        s->inner_finished = true;
    }
    return CLM_STREAM_SUCCESS;
}

static void sink_dispose(clm_stream_sink* self)
{
    sink_state* s = (sink_state*)self->user;
    if (s == NULL) return;
    clm_compress_codec_close(s->codec);
    clm_bytes_buffer_dispose(&s->pending);
    free(s);
    self->user = NULL;
}

clm_int clm_compress_stream_sink(
    clm_stream_sink* sink, clm_stream_sink* inner, const clm_compress_algorithm algorithm, const clm_compress_mode mode,
    const clm_compress_options* options)
{
    if (sink == NULL || inner == NULL || inner->write == NULL) return CLM_COMPRESS_INVALID_ARGUMENT;
    sink_state* s = (sink_state*)calloc(1, sizeof(*s));
    if (s == NULL) return CLM_COMPRESS_NO_MEMORY;
    const clm_int status = clm_compress_codec_open(&s->codec, algorithm, mode, options);
    if (status != CLM_COMPRESS_SUCCESS) {
        free(s);
        return status;
    }
    s->inner = inner;
    clm_stream_sink_init(sink, sink_write, sink_finish, sink_dispose, s);
    return CLM_COMPRESS_SUCCESS;
}

typedef struct {
    clm_compress_codec* codec;
    clm_stream_source* inner;
    clm_bytes_buffer pending;
    bool inner_done;
    bool finished;
    clm_int error;
    unsigned char input[16384];
} source_state;

static clm_int source_read(clm_stream_source* self, void* dest, size_t capacity, size_t* out_read)
{
    source_state* s = (source_state*)self->user;
    if (dest == NULL || capacity == 0 || out_read == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    if (s->error != CLM_STREAM_SUCCESS) return s->error;
    for (;;) {
        const size_t ready = clm_bytes_buffer_length(&s->pending);
        if (ready != 0) {
            const size_t count = capacity < ready ? capacity : ready;
            clm_bytes_buffer_read(&s->pending, dest, count);
            *out_read = count;
            return CLM_STREAM_SUCCESS;
        }
        if (s->finished) return CLM_STREAM_END;
        clm_int status;
        if (s->inner_done) {
            status = stream_status(clm_compress_codec_finish(s->codec, &s->pending));
            s->finished = true;
        } else {
            size_t got = 0;
            status = s->inner->read(s->inner, s->input, sizeof(s->input), &got);
            if (status == CLM_STREAM_SUCCESS) {
                status = stream_status(clm_compress_codec_write(s->codec, s->input, got, &s->pending));
            } else if (status == CLM_STREAM_END) {
                s->inner_done = true;
                status = CLM_STREAM_SUCCESS;
            } else {
                return status;
            }
        }
        if (status != CLM_STREAM_SUCCESS) {
            s->error = status;
            return status;
        }
    }
}

static void source_dispose(clm_stream_source* self)
{
    source_state* s = (source_state*)self->user;
    if (s == NULL) return;
    clm_compress_codec_close(s->codec);
    clm_bytes_buffer_dispose(&s->pending);
    free(s);
    self->user = NULL;
}

clm_int clm_compress_stream_source(
    clm_stream_source* source, clm_stream_source* inner, const clm_compress_algorithm algorithm,
    const clm_compress_mode mode, const clm_compress_options* options)
{
    if (source == NULL || inner == NULL || inner->read == NULL) return CLM_COMPRESS_INVALID_ARGUMENT;
    source_state* s = (source_state*)calloc(1, sizeof(*s));
    if (s == NULL) return CLM_COMPRESS_NO_MEMORY;
    const clm_int status = clm_compress_codec_open(&s->codec, algorithm, mode, options);
    if (status != CLM_COMPRESS_SUCCESS) {
        free(s);
        return status;
    }
    s->inner = inner;
    clm_stream_source_init(source, source_read, s);
    source->dispose = source_dispose;
    return CLM_COMPRESS_SUCCESS;
}

/* ---- ZIP archives -------------------------------------------------------------- */

struct clm_compress_zip_writer_impl {
    mz_zip_archive zip;
    bool finished;
};

struct clm_compress_zip_reader_impl {
    mz_zip_archive zip;
};

bool clm_compress_zip_name_is_safe(const char* name)
{
    if (name == NULL || name[0] == '\0' || strlen(name) >= MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE) return false;
    if (name[0] == '/' || name[0] == '\\') return false;
    if (((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z')) && name[1] == ':') return false;
    size_t segment = 0;
    for (size_t i = 0;; ++i) {
        const unsigned char c = (unsigned char)name[i];
        if (c != '\0' && (c < 0x20 || c == 0x7f)) return false;
        if (c == '/' || c == '\\' || c == '\0') {
            if (i - segment == 2 && name[segment] == '.' && name[segment + 1] == '.') return false;
            segment = i + 1;
            if (c == '\0') break;
        }
    }
    return true;
}

clm_int clm_compress_zip_writer_init(clm_compress_zip_writer* writer)
{
    if (writer == NULL) return CLM_COMPRESS_INVALID_ARGUMENT;
    writer->impl = (struct clm_compress_zip_writer_impl*)calloc(1, sizeof(*writer->impl));
    if (writer->impl == NULL) return CLM_COMPRESS_NO_MEMORY;
    mz_zip_zero_struct(&writer->impl->zip);
    if (!mz_zip_writer_init_heap(&writer->impl->zip, 0, 64 * 1024)) {
        free(writer->impl);
        writer->impl = NULL;
        return CLM_COMPRESS_NO_MEMORY;
    }
    return CLM_COMPRESS_SUCCESS;
}

clm_int clm_compress_zip_writer_add(
    clm_compress_zip_writer* writer, const char* name, const clm_bytes_view data, const clm_int level)
{
    if (writer == NULL || writer->impl == NULL || writer->impl->finished || level < 0 || level > 9 ||
        !clm_compress_zip_name_is_safe(name) || (data.length != 0 && data.data == NULL))
        return CLM_COMPRESS_INVALID_ARGUMENT;
    const size_t name_length = strlen(name);
    if (name[name_length - 1] == '/' && data.length != 0) return CLM_COMPRESS_INVALID_ARGUMENT;
    static const unsigned char empty[1] = {0};
    return mz_zip_writer_add_mem(&writer->impl->zip, name, data.length != 0 ? data.data : empty, data.length,
                                 (mz_uint)(level == 0 ? 6 : level))
        ? CLM_COMPRESS_SUCCESS : CLM_COMPRESS_FAILURE;
}

clm_int clm_compress_zip_writer_finish(clm_compress_zip_writer* writer, clm_bytes_buffer* out)
{
    if (writer == NULL || writer->impl == NULL || writer->impl->finished || out == NULL)
        return CLM_COMPRESS_INVALID_ARGUMENT;
    void* archive = NULL;
    size_t size = 0;
    if (!mz_zip_writer_finalize_heap_archive(&writer->impl->zip, &archive, &size)) return CLM_COMPRESS_FAILURE;
    writer->impl->finished = true;
    const clm_int status = map_bytes(clm_bytes_buffer_append(out, archive, size));
    mz_free(archive);
    return status;
}

void clm_compress_zip_writer_dispose(clm_compress_zip_writer* writer)
{
    if (writer == NULL || writer->impl == NULL) return;
    mz_zip_writer_end(&writer->impl->zip);
    free(writer->impl);
    writer->impl = NULL;
}

clm_int clm_compress_zip_reader_open(clm_compress_zip_reader* reader, const clm_bytes_view archive)
{
    if (reader == NULL || archive.data == NULL) return CLM_COMPRESS_INVALID_ARGUMENT;
    reader->impl = (struct clm_compress_zip_reader_impl*)calloc(1, sizeof(*reader->impl));
    if (reader->impl == NULL) return CLM_COMPRESS_NO_MEMORY;
    mz_zip_zero_struct(&reader->impl->zip);
    if (!mz_zip_reader_init_mem(&reader->impl->zip, archive.data, archive.length, 0)) {
        const clm_int status = mz_zip_get_last_error(&reader->impl->zip) == MZ_ZIP_ALLOC_FAILED
            ? CLM_COMPRESS_NO_MEMORY : CLM_COMPRESS_CORRUPT_DATA;
        free(reader->impl);
        reader->impl = NULL;
        return status;
    }
    return CLM_COMPRESS_SUCCESS;
}

size_t clm_compress_zip_reader_count(const clm_compress_zip_reader* reader)
{
    if (reader == NULL || reader->impl == NULL) return 0;
    return (size_t)mz_zip_reader_get_num_files(&reader->impl->zip);
}

clm_int clm_compress_zip_reader_entry(clm_compress_zip_reader* reader, const size_t index, clm_compress_zip_entry* out)
{
    mz_zip_archive_file_stat stat;
    if (reader == NULL || reader->impl == NULL || out == NULL || index >= clm_compress_zip_reader_count(reader))
        return CLM_COMPRESS_INVALID_ARGUMENT;
    if (!mz_zip_reader_file_stat(&reader->impl->zip, (mz_uint)index, &stat)) return CLM_COMPRESS_CORRUPT_DATA;
    memset(out, 0, sizeof(*out));
    snprintf(out->name, sizeof(out->name), "%s", stat.m_filename);
    out->size = stat.m_uncomp_size;
    out->compressed_size = stat.m_comp_size;
    out->is_directory = stat.m_is_directory != 0;
    return CLM_COMPRESS_SUCCESS;
}

clm_int clm_compress_zip_reader_find(clm_compress_zip_reader* reader, const char* name, size_t* index)
{
    if (reader == NULL || reader->impl == NULL || name == NULL || index == NULL) return CLM_COMPRESS_INVALID_ARGUMENT;
    const int found = mz_zip_reader_locate_file(&reader->impl->zip, name, NULL, 0);
    if (found < 0) return CLM_COMPRESS_NOT_FOUND;
    *index = (size_t)found;
    return CLM_COMPRESS_SUCCESS;
}

clm_int clm_compress_zip_reader_extract(
    clm_compress_zip_reader* reader, const size_t index, const size_t max_output, clm_bytes_buffer* out)
{
    mz_zip_archive_file_stat stat;
    if (reader == NULL || reader->impl == NULL || out == NULL || index >= clm_compress_zip_reader_count(reader))
        return CLM_COMPRESS_INVALID_ARGUMENT;
    mz_zip_archive* zip = &reader->impl->zip;
    if (!mz_zip_reader_file_stat(zip, (mz_uint)index, &stat)) return CLM_COMPRESS_CORRUPT_DATA;
    if (stat.m_is_directory) return CLM_COMPRESS_INVALID_ARGUMENT;
    if (!stat.m_is_supported || mz_zip_reader_is_file_encrypted(zip, (mz_uint)index)) return CLM_COMPRESS_UNSUPPORTED;
    const size_t cap = max_output != 0 ? max_output : CLM_COMPRESS_DEFAULT_MAX_OUTPUT;
    if (stat.m_uncomp_size > cap || stat.m_uncomp_size > SIZE_MAX) return CLM_COMPRESS_LIMIT_EXCEEDED;
    const size_t size = (size_t)stat.m_uncomp_size;
    if (size == 0) return CLM_COMPRESS_SUCCESS;
    unsigned char* tail = NULL;
    size_t available = 0;
    clm_int status = prepare_out(out, size, &tail, &available);
    if (status != CLM_COMPRESS_SUCCESS) return status;
    if (!mz_zip_reader_extract_to_mem(zip, (mz_uint)index, tail, size, 0)) return CLM_COMPRESS_CORRUPT_DATA;
    return commit_out(out, size);
}

void clm_compress_zip_reader_dispose(clm_compress_zip_reader* reader)
{
    if (reader == NULL || reader->impl == NULL) return;
    mz_zip_reader_end(&reader->impl->zip);
    free(reader->impl);
    reader->impl = NULL;
}
 