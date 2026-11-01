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

/* Incremental, bounded data transfer between a source and a sink.
 *
 * A source produces bytes and a sink consumes them. A pump moves bytes from
 * one to the other through a fixed-size staging buffer, one bounded step at a
 * time, so it works with blocking endpoints (files, memory) and nonblocking
 * ones (sockets) alike: a step that cannot progress returns WOULD_BLOCK and
 * reports which side to wait for. Memory use is bounded by the buffer size,
 * never by the amount transferred.
 *
 * Endpoint contract:
 *   read  - SUCCESS transfers at least one byte; END means no more data;
 *           WOULD_BLOCK means none is available yet; anything else is an error.
 *   write - SUCCESS accepts at least one byte and may accept fewer than given;
 *           WOULD_BLOCK accepts none now; anything else is an error.
 *   finish - signals end of data to a sink; may return WOULD_BLOCK and is then
 *           called again. It is optional.
 * Endpoints do not own what they point at (ids, buffers, other endpoints); do
 * not move or free those while a pump uses the endpoint. Built-in endpoints
 * keep their state inside the struct, so initialize one per transfer.
 */

#include "base.h"
#include "bytes.h"

#include <stdbool.h>
#include <stddef.h>

/** At least one byte moved, or the operation completed. */
#define CLM_STREAM_SUCCESS ((clm_int)0)
/** Source exhausted (read) or the whole transfer finished (pump). */
#define CLM_STREAM_END ((clm_int)1)
/** No progress is possible now; retry when the reported side is ready. */
#define CLM_STREAM_WOULD_BLOCK ((clm_int)2)
#define CLM_STREAM_FAILURE ((clm_int)-1)
#define CLM_STREAM_INVALID_ARGUMENT ((clm_int)-2)
#define CLM_STREAM_NO_MEMORY ((clm_int)-3)
/** More data arrived than the configured cap allows. */
#define CLM_STREAM_LIMIT_EXCEEDED ((clm_int)-4)
/** The source ended before the expected byte count. */
#define CLM_STREAM_TRUNCATED ((clm_int)-5)

/** Bits reported by clm_stream_pump_waiting. */
#define CLM_STREAM_WAIT_READ 1u
#define CLM_STREAM_WAIT_WRITE 2u

#define CLM_STREAM_DEFAULT_BUFFER_SIZE ((size_t)16384)

typedef struct clm_stream_source clm_stream_source;
typedef struct clm_stream_sink clm_stream_sink;

typedef clm_int (*clm_stream_read_fn)(
    clm_stream_source* self, void* dest, size_t capacity, size_t* out_read);
typedef clm_int (*clm_stream_write_fn)(
    clm_stream_sink* self, const void* data, size_t length, size_t* out_written);
typedef clm_int (*clm_stream_finish_fn)(clm_stream_sink* self);
typedef void (*clm_stream_dispose_fn)(clm_stream_sink* self);
typedef void (*clm_stream_source_dispose_fn)(clm_stream_source* self);

struct clm_stream_source {
    clm_stream_read_fn read;
    /** Optional; releases memory the endpoint owns. */
    clm_stream_source_dispose_fn dispose;
    /** Free for custom endpoints. */
    void* user;
    /** Private state of built-in endpoints. */
    union {
        struct { const unsigned char* data; size_t length; size_t position; } memory;
        clm_bytes_buffer* buffer;
        clm_id id;
        struct { clm_stream_source* first; clm_stream_source* second; bool first_done; } concat;
    } state;
};

struct clm_stream_sink {
    clm_stream_write_fn write;
    /** Optional; NULL means nothing to do at end of data. */
    clm_stream_finish_fn finish;
    /** Optional; releases memory the endpoint owns. */
    clm_stream_dispose_fn dispose;
    void* user;
    union {
        clm_bytes_buffer* buffer;
        struct { clm_id id; bool flag; } handle;
        struct { clm_stream_sink* inner; clm_bytes_buffer pending; bool terminated; bool inner_finished; } chunked;
    } state;
};

/** Initializes a custom source. */
clm_int clm_stream_source_init(clm_stream_source* source, clm_stream_read_fn read, void* user);

/** Initializes a custom sink; finish and dispose may be NULL. */
clm_int clm_stream_sink_init(
    clm_stream_sink* sink, clm_stream_write_fn write, clm_stream_finish_fn finish,
    clm_stream_dispose_fn dispose, void* user);

/** Releases memory a sink owns (the chunked sink); safe on any sink. */
void clm_stream_sink_dispose(clm_stream_sink* sink);

/** Releases memory a source owns (for example a decompressing source); safe on any source. */
void clm_stream_source_dispose(clm_stream_source* source);

/** Reads from borrowed memory, which must outlive the transfer. */
clm_int clm_stream_source_memory(clm_stream_source* source, clm_bytes_view view);

/** Reads by consuming bytes from a buffer; the buffer being empty is END, so
 * use it for data that is already complete, such as bytes left over after
 * parsing a header. */
clm_int clm_stream_source_buffer(clm_stream_source* source, clm_bytes_buffer* buffer);

/** Reads from an open file (clm_file_open); end of file is END. */
clm_int clm_stream_source_file(clm_stream_source* source, clm_id file_id);

/** Reads from a connected TCP socket; an orderly close is END. */
clm_int clm_stream_source_socket(clm_stream_source* source, clm_id socket_id);

/** Reads everything from first, then everything from second. */
clm_int clm_stream_source_concat(
    clm_stream_source* source, clm_stream_source* first, clm_stream_source* second);

/** Appends to a buffer. A buffer limit is reported as LIMIT_EXCEEDED. */
clm_int clm_stream_sink_buffer(clm_stream_sink* sink, clm_bytes_buffer* buffer);

/** Writes to an open file; with append, the file must be opened in append mode. */
clm_int clm_stream_sink_file(clm_stream_sink* sink, clm_id file_id, bool append);

/** Sends on a connected TCP socket. With shutdown_on_finish the write side is
 * half-closed at the end of data. */
clm_int clm_stream_sink_socket(clm_stream_sink* sink, clm_id socket_id, bool shutdown_on_finish);

/** Accepts and drops everything, for example to drain an unwanted body. */
clm_int clm_stream_sink_discard(clm_stream_sink* sink);

/** Wraps another sink in HTTP chunked transfer coding: each write becomes one
 * chunk and finish sends the terminating zero chunk, then finishes inner.
 * Trailers are not supported. Call clm_stream_sink_dispose when done; inner
 * must outlive this sink and is not disposed by it. */
clm_int clm_stream_sink_chunked(clm_stream_sink* sink, clm_stream_sink* inner);

/** Transfer settings; a zeroed struct (or NULL) selects the defaults. */
typedef struct {
    /** Staging buffer size; 0 selects CLM_STREAM_DEFAULT_BUFFER_SIZE. */
    size_t buffer_size;
    /** When set, exactly expected bytes are transferred and nothing beyond them
     * is read from the source; an earlier END is TRUNCATED. */
    bool has_expected;
    clm_ulong expected;
    /** Nonzero caps the bytes read; one byte more is LIMIT_EXCEEDED. */
    clm_ulong max_bytes;
} clm_stream_options;

typedef struct {
    clm_stream_source* source;
    clm_stream_sink* sink;
    clm_bytes_buffer buffer;
    size_t buffer_size;
    bool has_expected;
    clm_ulong expected;
    clm_ulong max_bytes;
    clm_ulong bytes_read;
    clm_ulong bytes_written;
    unsigned waiting;
    bool source_done;
    bool finished;
    clm_int error;
} clm_stream_pump;

/** Prepares a pump; source and sink must outlive it.
 * @return SUCCESS, INVALID_ARGUMENT, NO_MEMORY, or LIMIT_EXCEEDED when
 *         expected exceeds max_bytes.
 */
clm_int clm_stream_pump_init(
    clm_stream_pump* pump, clm_stream_source* source, clm_stream_sink* sink,
    const clm_stream_options* options);

/** Frees the staging buffer. The endpoints are not touched. */
void clm_stream_pump_dispose(clm_stream_pump* pump);

/** Does one bounded round of writing and reading.
 * @return SUCCESS when progress was made (call again), END when the transfer is
 *         complete, WOULD_BLOCK when nothing can move now (see
 *         clm_stream_pump_waiting), or an error. Errors are permanent.
 */
clm_int clm_stream_pump_step(clm_stream_pump* pump);

/** Steps until the transfer ends, blocks, or fails; returns that result.
 * With blocking endpoints this runs the whole transfer. */
clm_int clm_stream_pump_run(clm_stream_pump* pump);

/** Returns the CLM_STREAM_WAIT_* sides that blocked in the last step. */
unsigned clm_stream_pump_waiting(const clm_stream_pump* pump);

/** Moves a whole transfer with blocking endpoints.
 * @param out_copied Receives the bytes written; may be NULL.
 * @return SUCCESS when complete, WOULD_BLOCK if an endpoint cannot progress
 *         (use a pump for nonblocking endpoints), or an error.
 */
clm_int clm_stream_copy(
    clm_stream_source* source, clm_stream_sink* sink,
    const clm_stream_options* options, clm_ulong* out_copied);

