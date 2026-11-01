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

#include "stream.h"
#include "net.h"
#include "file.h"

#include <stdio.h>
#include <string.h>

static bool stream_read_args_ok(const void* dest, const size_t capacity, const size_t* out)
{
    return dest != NULL && capacity != 0 && out != NULL;
}

static bool stream_write_args_ok(const void* data, const size_t length, const size_t* out)
{
    return data != NULL && length != 0 && out != NULL;
}

clm_int clm_stream_source_init(clm_stream_source* source, clm_stream_read_fn read, void* user)
{
    if (source == NULL || read == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    memset(source, 0, sizeof(*source));
    source->read = read;
    source->user = user;
    return CLM_STREAM_SUCCESS;
}

clm_int clm_stream_sink_init(
    clm_stream_sink* sink, clm_stream_write_fn write, clm_stream_finish_fn finish,
    clm_stream_dispose_fn dispose, void* user)
{
    if (sink == NULL || write == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    memset(sink, 0, sizeof(*sink));
    sink->write = write;
    sink->finish = finish;
    sink->dispose = dispose;
    sink->user = user;
    return CLM_STREAM_SUCCESS;
}

void clm_stream_sink_dispose(clm_stream_sink* sink)
{
    if (sink != NULL && sink->dispose != NULL) sink->dispose(sink);
}

void clm_stream_source_dispose(clm_stream_source* source)
{
    if (source != NULL && source->dispose != NULL) source->dispose(source);
}

static clm_int memory_read(clm_stream_source* self, void* dest, size_t capacity, size_t* out_read)
{
    if (!stream_read_args_ok(dest, capacity, out_read)) return CLM_STREAM_INVALID_ARGUMENT;
    const size_t remaining = self->state.memory.length - self->state.memory.position;
    if (remaining == 0) return CLM_STREAM_END;
    const size_t count = capacity < remaining ? capacity : remaining;
    memcpy(dest, self->state.memory.data + self->state.memory.position, count);
    self->state.memory.position += count;
    *out_read = count;
    return CLM_STREAM_SUCCESS;
}

clm_int clm_stream_source_memory(clm_stream_source* source, const clm_bytes_view view)
{
    if (source == NULL || (view.length != 0 && view.data == NULL)) return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_source_init(source, memory_read, NULL);
    source->state.memory.data = view.data;
    source->state.memory.length = view.length;
    return CLM_STREAM_SUCCESS;
}

static clm_int buffer_read(clm_stream_source* self, void* dest, size_t capacity, size_t* out_read)
{
    if (!stream_read_args_ok(dest, capacity, out_read)) return CLM_STREAM_INVALID_ARGUMENT;
    const size_t length = clm_bytes_buffer_length(self->state.buffer);
    if (length == 0) return CLM_STREAM_END;
    const size_t count = capacity < length ? capacity : length;
    if (clm_bytes_buffer_read(self->state.buffer, dest, count) != CLM_BYTES_SUCCESS) return CLM_STREAM_FAILURE;
    *out_read = count;
    return CLM_STREAM_SUCCESS;
}

clm_int clm_stream_source_buffer(clm_stream_source* source, clm_bytes_buffer* buffer)
{
    if (source == NULL || buffer == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_source_init(source, buffer_read, NULL);
    source->state.buffer = buffer;
    return CLM_STREAM_SUCCESS;
}

static clm_int file_read(clm_stream_source* self, void* dest, size_t capacity, size_t* out_read)
{
    if (!stream_read_args_ok(dest, capacity, out_read)) return CLM_STREAM_INVALID_ARGUMENT;
    size_t count = 0;
    if (clm_file_read_to(self->state.id, dest, capacity, &count) != CLM_FILE_OPERATION_SUCCESS)
        return CLM_STREAM_FAILURE;
    if (count == 0) return CLM_STREAM_END;
    *out_read = count;
    return CLM_STREAM_SUCCESS;
}

clm_int clm_stream_source_file(clm_stream_source* source, const clm_id file_id)
{
    if (source == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_source_init(source, file_read, NULL);
    source->state.id = file_id;
    return CLM_STREAM_SUCCESS;
}

static clm_int socket_read(clm_stream_source* self, void* dest, size_t capacity, size_t* out_read)
{
    if (!stream_read_args_ok(dest, capacity, out_read)) return CLM_STREAM_INVALID_ARGUMENT;
    size_t count = 0;
    switch (clm_net_receive(self->state.id, dest, capacity, &count)) {
    case CLM_NET_SUCCESS:
        if (count == 0) return CLM_STREAM_WOULD_BLOCK;
        *out_read = count;
        return CLM_STREAM_SUCCESS;
    case CLM_NET_CLOSED:
        return CLM_STREAM_END;
    case CLM_NET_WOULD_BLOCK:
    case CLM_NET_IN_PROGRESS:
    case CLM_NET_TIMEOUT:
        return CLM_STREAM_WOULD_BLOCK;
    default:
        return CLM_STREAM_FAILURE;
    }
}

clm_int clm_stream_source_socket(clm_stream_source* source, const clm_id socket_id)
{
    if (source == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_source_init(source, socket_read, NULL);
    source->state.id = socket_id;
    return CLM_STREAM_SUCCESS;
}

static clm_int concat_read(clm_stream_source* self, void* dest, size_t capacity, size_t* out_read)
{
    if (!self->state.concat.first_done) {
        clm_stream_source* first = self->state.concat.first;
        const clm_int status = first->read(first, dest, capacity, out_read);
        if (status != CLM_STREAM_END) return status;
        self->state.concat.first_done = true;
    }
    clm_stream_source* second = self->state.concat.second;
    return second->read(second, dest, capacity, out_read);
}

clm_int clm_stream_source_concat(
    clm_stream_source* source, clm_stream_source* first, clm_stream_source* second)
{
    if (source == NULL || first == NULL || second == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_source_init(source, concat_read, NULL);
    source->state.concat.first = first;
    source->state.concat.second = second;
    return CLM_STREAM_SUCCESS;
}

static clm_int buffer_write(clm_stream_sink* self, const void* data, size_t length, size_t* out_written)
{
    if (!stream_write_args_ok(data, length, out_written)) return CLM_STREAM_INVALID_ARGUMENT;
    switch (clm_bytes_buffer_append(self->state.buffer, data, length)) {
    case CLM_BYTES_SUCCESS:
        *out_written = length;
        return CLM_STREAM_SUCCESS;
    case CLM_BYTES_LIMIT_EXCEEDED:
        return CLM_STREAM_LIMIT_EXCEEDED;
    case CLM_BYTES_NO_MEMORY:
        return CLM_STREAM_NO_MEMORY;
    default:
        return CLM_STREAM_FAILURE;
    }
}

clm_int clm_stream_sink_buffer(clm_stream_sink* sink, clm_bytes_buffer* buffer)
{
    if (sink == NULL || buffer == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_sink_init(sink, buffer_write, NULL, NULL, NULL);
    sink->state.buffer = buffer;
    return CLM_STREAM_SUCCESS;
}

static clm_int file_write(clm_stream_sink* self, const void* data, size_t length, size_t* out_written)
{
    if (!stream_write_args_ok(data, length, out_written)) return CLM_STREAM_INVALID_ARGUMENT;
    size_t count = 0;
    const clm_int status = self->state.handle.flag
        ? clm_file_append_open_from(self->state.handle.id, data, length, &count)
        : clm_file_write_from(self->state.handle.id, data, length, &count);
    if (status != CLM_FILE_OPERATION_SUCCESS || count == 0 || count > length) return CLM_STREAM_FAILURE;
    *out_written = count;
    return CLM_STREAM_SUCCESS;
}

clm_int clm_stream_sink_file(clm_stream_sink* sink, const clm_id file_id, const bool append)
{
    if (sink == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_sink_init(sink, file_write, NULL, NULL, NULL);
    sink->state.handle.id = file_id;
    sink->state.handle.flag = append;
    return CLM_STREAM_SUCCESS;
}

static clm_int socket_write(clm_stream_sink* self, const void* data, size_t length, size_t* out_written)
{
    if (!stream_write_args_ok(data, length, out_written)) return CLM_STREAM_INVALID_ARGUMENT;
    size_t count = 0;
    switch (clm_net_send(self->state.handle.id, data, length, &count)) {
    case CLM_NET_SUCCESS:
        if (count == 0) return CLM_STREAM_WOULD_BLOCK;
        if (count > length) return CLM_STREAM_FAILURE;
        *out_written = count;
        return CLM_STREAM_SUCCESS;
    case CLM_NET_WOULD_BLOCK:
    case CLM_NET_IN_PROGRESS:
    case CLM_NET_TIMEOUT:
        return CLM_STREAM_WOULD_BLOCK;
    default:
        return CLM_STREAM_FAILURE;
    }
}

static clm_int socket_finish(clm_stream_sink* self)
{
    return clm_net_socket_shutdown(self->state.handle.id, CLM_NET_SHUTDOWN_WRITE) == CLM_NET_SUCCESS
        ? CLM_STREAM_SUCCESS : CLM_STREAM_FAILURE;
}

clm_int clm_stream_sink_socket(clm_stream_sink* sink, const clm_id socket_id, const bool shutdown_on_finish)
{
    if (sink == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_sink_init(sink, socket_write, shutdown_on_finish ? socket_finish : NULL, NULL, NULL);
    sink->state.handle.id = socket_id;
    return CLM_STREAM_SUCCESS;
}

static clm_int discard_write(clm_stream_sink* self, const void* data, size_t length, size_t* out_written)
{
    (void)self;
    if (!stream_write_args_ok(data, length, out_written)) return CLM_STREAM_INVALID_ARGUMENT;
    *out_written = length;
    return CLM_STREAM_SUCCESS;
}

clm_int clm_stream_sink_discard(clm_stream_sink* sink)
{
    return clm_stream_sink_init(sink, discard_write, NULL, NULL, NULL);
}

/* Pushes staged chunk bytes into the inner sink; WOULD_BLOCK keeps the rest. */
static clm_int chunked_flush(clm_stream_sink* self)
{
    clm_stream_sink* inner = self->state.chunked.inner;
    clm_bytes_buffer* pending = &self->state.chunked.pending;
    while (clm_bytes_buffer_length(pending) != 0) {
        const clm_bytes_view view = clm_bytes_buffer_view(pending);
        size_t count = 0;
        const clm_int status = inner->write(inner, view.data, view.length, &count);
        if (status != CLM_STREAM_SUCCESS) return status;
        if (count == 0 || count > view.length) return CLM_STREAM_FAILURE;
        clm_bytes_buffer_consume(pending, count);
    }
    return CLM_STREAM_SUCCESS;
}

static clm_int chunked_write(clm_stream_sink* self, const void* data, size_t length, size_t* out_written)
{
    if (!stream_write_args_ok(data, length, out_written)) return CLM_STREAM_INVALID_ARGUMENT;
    if (self->state.chunked.terminated) return CLM_STREAM_FAILURE;
    clm_int status = chunked_flush(self);
    if (status != CLM_STREAM_SUCCESS) return status;
    char head[2 * sizeof(unsigned long long) + 3];
    snprintf(head, sizeof(head), "%llx\r\n", (unsigned long long)length);
    clm_bytes_buffer* pending = &self->state.chunked.pending;
    if (clm_bytes_buffer_append_string(pending, head) != CLM_BYTES_SUCCESS ||
        clm_bytes_buffer_append(pending, data, length) != CLM_BYTES_SUCCESS ||
        clm_bytes_buffer_append(pending, "\r\n", 2) != CLM_BYTES_SUCCESS) {
        clm_bytes_buffer_clear(pending);
        return CLM_STREAM_NO_MEMORY;
    }
    *out_written = length;
    status = chunked_flush(self);
    return status == CLM_STREAM_WOULD_BLOCK ? CLM_STREAM_SUCCESS : status;
}

static clm_int chunked_finish(clm_stream_sink* self)
{
    clm_bytes_buffer* pending = &self->state.chunked.pending;
    if (!self->state.chunked.terminated) {
        if (clm_bytes_buffer_append(pending, "0\r\n\r\n", 5) != CLM_BYTES_SUCCESS) return CLM_STREAM_NO_MEMORY;
        self->state.chunked.terminated = true;
    }
    clm_int status = chunked_flush(self);
    if (status != CLM_STREAM_SUCCESS) return status;
    clm_stream_sink* inner = self->state.chunked.inner;
    if (!self->state.chunked.inner_finished) {
        status = inner->finish != NULL ? inner->finish(inner) : CLM_STREAM_SUCCESS;
        if (status != CLM_STREAM_SUCCESS) return status;
        self->state.chunked.inner_finished = true;
    }
    return CLM_STREAM_SUCCESS;
}

static void chunked_dispose(clm_stream_sink* self)
{
    clm_bytes_buffer_dispose(&self->state.chunked.pending);
}

clm_int clm_stream_sink_chunked(clm_stream_sink* sink, clm_stream_sink* inner)
{
    if (sink == NULL || inner == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_sink_init(sink, chunked_write, chunked_finish, chunked_dispose, NULL);
    sink->state.chunked.inner = inner;
    return CLM_STREAM_SUCCESS;
}

clm_int clm_stream_pump_init(
    clm_stream_pump* pump, clm_stream_source* source, clm_stream_sink* sink,
    const clm_stream_options* options)
{
    if (pump == NULL || source == NULL || sink == NULL || source->read == NULL || sink->write == NULL)
        return CLM_STREAM_INVALID_ARGUMENT;
    clm_stream_options settings;
    memset(&settings, 0, sizeof(settings));
    if (options != NULL) settings = *options;
    if (settings.has_expected && settings.max_bytes != 0 && settings.expected > settings.max_bytes)
        return CLM_STREAM_LIMIT_EXCEEDED;
    memset(pump, 0, sizeof(*pump));
    pump->source = source;
    pump->sink = sink;
    pump->buffer_size = settings.buffer_size != 0 ? settings.buffer_size : CLM_STREAM_DEFAULT_BUFFER_SIZE;
    pump->has_expected = settings.has_expected;
    pump->expected = settings.expected;
    pump->max_bytes = settings.max_bytes;
    if (clm_bytes_buffer_init(&pump->buffer, pump->buffer_size) != CLM_BYTES_SUCCESS)
        return CLM_STREAM_NO_MEMORY;
    clm_bytes_buffer_set_limit(&pump->buffer, pump->buffer_size);
    return CLM_STREAM_SUCCESS;
}

void clm_stream_pump_dispose(clm_stream_pump* pump)
{
    if (pump != NULL) clm_bytes_buffer_dispose(&pump->buffer);
}

unsigned clm_stream_pump_waiting(const clm_stream_pump* pump)
{
    return pump != NULL ? pump->waiting : 0;
}

static clm_int pump_fail(clm_stream_pump* pump, const clm_int status)
{
    pump->error = status < 0 ? status : CLM_STREAM_FAILURE;
    return pump->error;
}

/* Returns SUCCESS or an error; a blocked sink only sets the waiting bit. */
static clm_int pump_write(clm_stream_pump* pump, bool* progress)
{
    const clm_bytes_view view = clm_bytes_buffer_view(&pump->buffer);
    if (view.length == 0) return CLM_STREAM_SUCCESS;
    size_t count = 0;
    const clm_int status = pump->sink->write(pump->sink, view.data, view.length, &count);
    if (status == CLM_STREAM_WOULD_BLOCK || (status == CLM_STREAM_SUCCESS && count == 0)) {
        pump->waiting |= CLM_STREAM_WAIT_WRITE;
        return CLM_STREAM_SUCCESS;
    }
    if (status != CLM_STREAM_SUCCESS) return status;
    if (count > view.length) return CLM_STREAM_FAILURE;
    clm_bytes_buffer_consume(&pump->buffer, count);
    pump->bytes_written += count;
    *progress = true;
    return CLM_STREAM_SUCCESS;
}

static clm_int pump_read(clm_stream_pump* pump, bool* progress, bool* read_something)
{
    size_t room = pump->buffer_size - clm_bytes_buffer_length(&pump->buffer);
    if (pump->has_expected) {
        const clm_ulong remaining = pump->expected - pump->bytes_read;
        if (remaining == 0) {
            pump->source_done = true;
            *progress = true;
            return CLM_STREAM_SUCCESS;
        }
        if (remaining < room) room = (size_t)remaining;
    } else if (pump->max_bytes != 0) {
        const clm_ulong allowed = pump->max_bytes - pump->bytes_read + 1;
        if (allowed < room) room = (size_t)allowed;
    }
    if (room == 0) return CLM_STREAM_SUCCESS;

    unsigned char* tail = NULL;
    size_t available = 0;
    if (clm_bytes_buffer_prepare(&pump->buffer, 1, &tail, &available) != CLM_BYTES_SUCCESS)
        return CLM_STREAM_NO_MEMORY;
    if (available < room) room = available;
    if (room == 0) return CLM_STREAM_SUCCESS;

    size_t count = 0;
    const clm_int status = pump->source->read(pump->source, tail, room, &count);
    if (status == CLM_STREAM_END) {
        pump->source_done = true;
        *progress = true;
        return pump->has_expected && pump->bytes_read < pump->expected ? CLM_STREAM_TRUNCATED : CLM_STREAM_SUCCESS;
    }
    if (status == CLM_STREAM_WOULD_BLOCK || (status == CLM_STREAM_SUCCESS && count == 0)) {
        pump->waiting |= CLM_STREAM_WAIT_READ;
        return CLM_STREAM_SUCCESS;
    }
    if (status != CLM_STREAM_SUCCESS) return status;
    if (count > room) return CLM_STREAM_FAILURE;
    if (!pump->has_expected && pump->max_bytes != 0 && pump->bytes_read + count > pump->max_bytes)
        return CLM_STREAM_LIMIT_EXCEEDED;
    clm_bytes_buffer_commit(&pump->buffer, count);
    pump->bytes_read += count;
    *progress = true;
    *read_something = true;
    return CLM_STREAM_SUCCESS;
}

clm_int clm_stream_pump_step(clm_stream_pump* pump)
{
    if (pump == NULL) return CLM_STREAM_INVALID_ARGUMENT;
    if (pump->error != CLM_STREAM_SUCCESS) return pump->error;
    if (pump->finished) return CLM_STREAM_END;
    pump->waiting = 0;
    bool progress = false;
    bool read_something = false;

    clm_int status = pump_write(pump, &progress);
    if (status != CLM_STREAM_SUCCESS) return pump_fail(pump, status);
    if (!pump->source_done) {
        status = pump_read(pump, &progress, &read_something);
        if (status != CLM_STREAM_SUCCESS) return pump_fail(pump, status);
        if (read_something && !(pump->waiting & CLM_STREAM_WAIT_WRITE)) {
            status = pump_write(pump, &progress);
            if (status != CLM_STREAM_SUCCESS) return pump_fail(pump, status);
        }
    }
    if (pump->source_done && clm_bytes_buffer_length(&pump->buffer) == 0) {
        status = pump->sink->finish != NULL ? pump->sink->finish(pump->sink) : CLM_STREAM_SUCCESS;
        if (status == CLM_STREAM_WOULD_BLOCK) pump->waiting |= CLM_STREAM_WAIT_WRITE;
        else if (status != CLM_STREAM_SUCCESS) return pump_fail(pump, status);
        else pump->finished = true;
    }
    if (pump->finished) return CLM_STREAM_END;
    return progress ? CLM_STREAM_SUCCESS : CLM_STREAM_WOULD_BLOCK;
}

clm_int clm_stream_pump_run(clm_stream_pump* pump)
{
    clm_int status;
    do status = clm_stream_pump_step(pump);
    while (status == CLM_STREAM_SUCCESS);
    return status;
}

clm_int clm_stream_copy(
    clm_stream_source* source, clm_stream_sink* sink,
    const clm_stream_options* options, clm_ulong* out_copied)
{
    clm_stream_pump pump;
    clm_int status = clm_stream_pump_init(&pump, source, sink, options);
    if (status != CLM_STREAM_SUCCESS) return status;
    status = clm_stream_pump_run(&pump);
    if (out_copied != NULL) *out_copied = pump.bytes_written;
    clm_stream_pump_dispose(&pump);
    return status == CLM_STREAM_END ? CLM_STREAM_SUCCESS : status;
}


