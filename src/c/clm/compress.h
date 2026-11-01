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

/* Compression for bytes, strings, streams and ZIP archives.
 *
 * Formats (vendored libraries under ext/):
 *   RAW_DEFLATE, ZLIB, GZIP   miniz     HTTP "deflate" is ZLIB, "gzip" is GZIP
 *   LZ4                       lz4       LZ4 frame format (with content checksum)
 *   BROTLI                    brotli    HTTP "br"
 *   ZIP archives              miniz     in memory, see clm_compress_zip_*
 * Output is the standard format of each library, so it interoperates with other
 * tools. memlz and uzlib are not used: memlz is marked beta and uzlib duplicates
 * what miniz already provides.
 *
 * Decoding treats its input as hostile: output is capped (max_output) so a tiny
 * input cannot expand without bound, corrupt, truncated and trailing data are
 * reported, and nothing is written past the cap. The incremental codec and the
 * stream adapters work in bounded steps, so they suit nonblocking transfers.
 */

#include "base.h"
#include "bytes.h"
#include "stream.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLM_COMPRESS_SUCCESS ((clm_int)0)
/** A lookup found nothing; not an error. */
#define CLM_COMPRESS_NOT_FOUND ((clm_int)1)
#define CLM_COMPRESS_FAILURE ((clm_int)-1)
#define CLM_COMPRESS_INVALID_ARGUMENT ((clm_int)-2)
#define CLM_COMPRESS_NO_MEMORY ((clm_int)-3)
/** The data is not a valid stream of the chosen format, or has a bad checksum
 * or trailing bytes. */
#define CLM_COMPRESS_CORRUPT_DATA ((clm_int)-4)
/** The data ended before the stream was complete. */
#define CLM_COMPRESS_TRUNCATED ((clm_int)-5)
/** Decoded output would be larger than max_output. */
#define CLM_COMPRESS_LIMIT_EXCEEDED ((clm_int)-6)
#define CLM_COMPRESS_UNSUPPORTED ((clm_int)-7)

/** Used when clm_compress_options.max_output is 0. */
#define CLM_COMPRESS_DEFAULT_MAX_OUTPUT ((size_t)256 * 1024 * 1024)

typedef enum {
    CLM_COMPRESS_RAW_DEFLATE = 1,
    CLM_COMPRESS_ZLIB = 2,
    CLM_COMPRESS_GZIP = 3,
    CLM_COMPRESS_LZ4 = 4,
    CLM_COMPRESS_BROTLI = 5
} clm_compress_algorithm;

typedef enum {
    CLM_COMPRESS_MODE_ENCODE = 1,
    CLM_COMPRESS_MODE_DECODE = 2
} clm_compress_mode;

/** A zeroed struct (or NULL) selects the defaults. */
typedef struct {
    /** 0 is the default; 1 is fastest and 9 is smallest. Higher levels cost
     * more time and, for LZ4 and Brotli, more memory. */
    clm_int level;
    /** Decoding only: cap on the decoded size; 0 selects the default. */
    size_t max_output;
} clm_compress_options;

/** Maps an HTTP content-coding or format name, ignoring case:
 * "gzip" "x-gzip" "deflate" (zlib) "zlib" "raw-deflate" "lz4" "br" "brotli".
 * @return SUCCESS, or NOT_FOUND for an unknown name.
 */
clm_int clm_compress_algorithm_from_name(const char* name, clm_compress_algorithm* out);

/** Canonical name: "raw-deflate", "deflate" (the zlib format, as HTTP uses it),
 * "gzip", "lz4", "br"; NULL for an invalid value. */
const char* clm_compress_algorithm_name(clm_compress_algorithm algorithm);

/** Recognizes gzip, zlib and LZ4 data by its first bytes. Raw deflate and Brotli
 * have no signature. */
clm_int clm_compress_detect(clm_bytes_view data, clm_compress_algorithm* out);

/** Compresses data, appending to out. On failure out keeps its old length.
 * @return SUCCESS, INVALID_ARGUMENT, NO_MEMORY, LIMIT_EXCEEDED (out has a limit).
 */
clm_int clm_compress_encode(
    clm_compress_algorithm algorithm, clm_bytes_view data,
    const clm_compress_options* options, clm_bytes_buffer* out);

/** Decompresses a complete stream, appending to out; out keeps its old length on
 * failure. Fails if the data is corrupt, truncated, has trailing bytes, or
 * expands past options->max_output.
 */
clm_int clm_compress_decode(
    clm_compress_algorithm algorithm, clm_bytes_view data,
    const clm_compress_options* options, clm_bytes_buffer* out);

/** Compresses the characters of a NUL-terminated string, without the NUL. */
clm_int clm_compress_encode_string(
    clm_compress_algorithm algorithm, const char* text,
    const clm_compress_options* options, clm_bytes_buffer* out);

/** Compresses the text of a CLM string block. */
clm_int clm_compress_encode_string_id(
    clm_compress_algorithm algorithm, clm_id string_id,
    const clm_compress_options* options, clm_bytes_buffer* out);

/** Decompresses into the text of an existing CLM string block. The destination
 * is unchanged unless the result is valid UTF-8 with no NUL byte.
 * @return SUCCESS, FAILURE (not valid text), INVALID_ARGUMENT (no such string),
 *         or a decode error.
 */
clm_int clm_compress_decode_to_string_id(
    clm_compress_algorithm algorithm, clm_bytes_view data,
    const clm_compress_options* options, clm_id destination_id);

/** Incremental encoder or decoder. After an error every later call returns the
 * same error. */
typedef struct clm_compress_codec clm_compress_codec;

clm_int clm_compress_codec_open(
    clm_compress_codec** codec, clm_compress_algorithm algorithm,
    clm_compress_mode mode, const clm_compress_options* options);

/** Processes input and appends whatever output is ready; pieces of any size,
 * including a byte at a time, are accepted. Output may lag until finish. */
clm_int clm_compress_codec_write(
    clm_compress_codec* codec, const void* data, size_t length, clm_bytes_buffer* out);

/** Ends the stream. An encoder writes its trailer; a decoder verifies that the
 * stream was complete (TRUNCATED otherwise). No writes are allowed afterwards. */
clm_int clm_compress_codec_finish(clm_compress_codec* codec, clm_bytes_buffer* out);

void clm_compress_codec_close(clm_compress_codec* codec);

/** Makes a sink that encodes or decodes what is written to it and forwards the
 * result to inner. Finishing the sink finishes the stream and then inner. Call
 * clm_stream_sink_dispose when done; inner must outlive it and is not disposed.
 */
clm_int clm_compress_stream_sink(
    clm_stream_sink* sink, clm_stream_sink* inner, clm_compress_algorithm algorithm,
    clm_compress_mode mode, const clm_compress_options* options);

/** Makes a source that reads from inner and yields the encoded or decoded bytes.
 * Call clm_stream_source_dispose when done; inner is not disposed. A decoder
 * reports a source that ends early as a failed read.
 */
clm_int clm_compress_stream_source(
    clm_stream_source* source, clm_stream_source* inner, clm_compress_algorithm algorithm,
    clm_compress_mode mode, const clm_compress_options* options);

/* ---- ZIP archives, held in memory ------------------------------------------ */

typedef struct {
    struct clm_compress_zip_writer_impl* impl;
} clm_compress_zip_writer;

typedef struct {
    struct clm_compress_zip_reader_impl* impl;
} clm_compress_zip_reader;

typedef struct {
    char name[512];
    uint64_t size;
    uint64_t compressed_size;
    bool is_directory;
} clm_compress_zip_entry;

/** True when an entry name is safe to join onto a directory: not empty, not
 * absolute or drive-qualified, and without ".." segments or control bytes.
 * Check this before writing extracted entries to disk. */
bool clm_compress_zip_name_is_safe(const char* name);

clm_int clm_compress_zip_writer_init(clm_compress_zip_writer* writer);

/** Adds a file compressed with deflate; level is as in clm_compress_options.
 * @return SUCCESS, INVALID_ARGUMENT (bad name, finished writer), or FAILURE.
 */
clm_int clm_compress_zip_writer_add(
    clm_compress_zip_writer* writer, const char* name, clm_bytes_view data, clm_int level);

/** Appends the finished archive to out. The writer cannot be added to again. */
clm_int clm_compress_zip_writer_finish(clm_compress_zip_writer* writer, clm_bytes_buffer* out);

void clm_compress_zip_writer_dispose(clm_compress_zip_writer* writer);

/** Opens an archive held in memory, which must outlive the reader.
 * @return SUCCESS, CORRUPT_DATA, INVALID_ARGUMENT, or NO_MEMORY.
 */
clm_int clm_compress_zip_reader_open(clm_compress_zip_reader* reader, clm_bytes_view archive);

size_t clm_compress_zip_reader_count(const clm_compress_zip_reader* reader);

clm_int clm_compress_zip_reader_entry(
    clm_compress_zip_reader* reader, size_t index, clm_compress_zip_entry* out);

/** @return SUCCESS with the entry index, or NOT_FOUND. */
clm_int clm_compress_zip_reader_find(
    clm_compress_zip_reader* reader, const char* name, size_t* index);

/** Appends an entry's contents to out; out keeps its old length on failure.
 * @param max_output Cap on the entry's size; 0 selects the default.
 * @return SUCCESS, LIMIT_EXCEEDED, CORRUPT_DATA (including a bad checksum),
 *         INVALID_ARGUMENT (directory or bad index), or NO_MEMORY.
 */
clm_int clm_compress_zip_reader_extract(
    clm_compress_zip_reader* reader, size_t index, size_t max_output, clm_bytes_buffer* out);

void clm_compress_zip_reader_dispose(clm_compress_zip_reader* reader);

