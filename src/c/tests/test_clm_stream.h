#pragma once

#include "test.h"
#include "../clm/stream.h"
#include "../clm/file.h"
#include "../clm/gen.h"
#include "../clm/net.h"
#include "../clm/string_httpp.h"

/* Source that blocks on every other read and then yields at most three bytes. */
typedef struct {
    const unsigned char* data;
    size_t length;
    size_t position;
    unsigned calls;
} clm_stream_test_feed;

static clm_int clm_stream_test_feed_read(
    clm_stream_source* self, void* dest, size_t capacity, size_t* out_read)
{
    clm_stream_test_feed* feed = (clm_stream_test_feed*)self->user;
    if (++feed->calls % 2 == 1) return CLM_STREAM_WOULD_BLOCK;
    if (feed->position == feed->length) return CLM_STREAM_END;
    size_t count = feed->length - feed->position;
    if (count > 3) count = 3;
    if (count > capacity) count = capacity;
    memcpy(dest, feed->data + feed->position, count);
    feed->position += count;
    *out_read = count;
    return CLM_STREAM_SUCCESS;
}

/* Sink that blocks on every third write and accepts at most two bytes. */
typedef struct {
    clm_bytes_buffer* out;
    unsigned calls;
    bool finished;
} clm_stream_test_drain;

static clm_int clm_stream_test_drain_write(
    clm_stream_sink* self, const void* data, size_t length, size_t* out_written)
{
    clm_stream_test_drain* drain = (clm_stream_test_drain*)self->user;
    if (++drain->calls % 3 == 0) return CLM_STREAM_WOULD_BLOCK;
    const size_t count = length < 2 ? length : 2;
    if (clm_bytes_buffer_append(drain->out, data, count) != CLM_BYTES_SUCCESS) return CLM_STREAM_FAILURE;
    *out_written = count;
    return CLM_STREAM_SUCCESS;
}

static clm_int clm_stream_test_drain_finish(clm_stream_sink* self)
{
    ((clm_stream_test_drain*)self->user)->finished = true;
    return CLM_STREAM_SUCCESS;
}

static clm_int clm_stream_test_run_steps(clm_stream_pump* pump, unsigned* seen_waiting)
{
    clm_int status;
    int steps = 0;
    *seen_waiting = 0;
    do {
        status = clm_stream_pump_step(pump);
        *seen_waiting |= clm_stream_pump_waiting(pump);
    } while ((status == CLM_STREAM_SUCCESS || status == CLM_STREAM_WOULD_BLOCK) && ++steps < 1000);
    return status;
}

static int clm_stream_test_equals(clm_bytes_buffer* buffer, const char* expected)
{
    return clm_bytes_view_equals(clm_bytes_buffer_view(buffer), clm_bytes_view_from_string(expected));
}

static int clm_stream_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Stream");
    clm_stream_source source;
    clm_stream_sink sink;
    clm_bytes_buffer out = {0};
    clm_ulong copied = 0;
    clm_stream_options four_bytes = {0};
    four_bytes.buffer_size = 4;

    CLM_ASSERT(clm_stream_source_memory(&source, clm_bytes_view_from_string("hello world")) == CLM_STREAM_SUCCESS &&
                   clm_stream_sink_buffer(&sink, &out) == CLM_STREAM_SUCCESS &&
                   clm_stream_copy(&source, &sink, &four_bytes, &copied) == CLM_STREAM_SUCCESS && copied == 11 &&
                   clm_stream_test_equals(&out, "hello world"),
               "copy memory to a buffer through a four-byte staging buffer");
    clm_bytes_buffer_clear(&out);
    const unsigned char binary[] = {0x00, 0xFF, 0x10, 0x00, 0x7F, 0x00};
    clm_stream_source_memory(&source, (clm_bytes_view){binary, sizeof(binary)});
    clm_stream_sink_buffer(&sink, &out);
    CLM_ASSERT(clm_stream_copy(&source, &sink, NULL, &copied) == CLM_STREAM_SUCCESS && copied == 6 &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&out), (clm_bytes_view){binary, sizeof(binary)}),
               "copy binary data with NUL bytes using the default settings");
    clm_bytes_buffer_clear(&out);
    clm_stream_source_memory(&source, (clm_bytes_view){NULL, 0});
    clm_stream_sink_buffer(&sink, &out);
    CLM_ASSERT(clm_stream_copy(&source, &sink, NULL, &copied) == CLM_STREAM_SUCCESS && copied == 0 &&
                   clm_stream_source_memory(&source, (clm_bytes_view){NULL, 1}) == CLM_STREAM_INVALID_ARGUMENT &&
                   clm_stream_copy(NULL, &sink, NULL, NULL) == CLM_STREAM_INVALID_ARGUMENT,
               "copy an empty source and reject invalid arguments");

    const char* text = "The quick brown fox jumps over the lazy dog";
    clm_stream_test_feed feed = {(const unsigned char*)text, strlen(text), 0, 0};
    clm_stream_test_drain drain = {&out, 0, false};
    clm_stream_pump pump;
    unsigned seen = 0;
    clm_bytes_buffer_clear(&out);
    clm_stream_source_init(&source, clm_stream_test_feed_read, &feed);
    clm_stream_sink_init(&sink, clm_stream_test_drain_write, clm_stream_test_drain_finish, NULL, &drain);
    CLM_ASSERT(clm_stream_pump_init(&pump, &source, &sink, &four_bytes) == CLM_STREAM_SUCCESS &&
                   clm_stream_test_run_steps(&pump, &seen) == CLM_STREAM_END &&
                   clm_stream_test_equals(&out, text) && drain.finished &&
                   (seen & CLM_STREAM_WAIT_READ) && (seen & CLM_STREAM_WAIT_WRITE) &&
                   pump.bytes_read == strlen(text) && pump.bytes_written == strlen(text) &&
                   clm_stream_pump_step(&pump) == CLM_STREAM_END,
               "step through blocking and partial endpoints and report which side waits");
    clm_stream_pump_dispose(&pump);

    clm_bytes_buffer_clear(&out);
    clm_stream_options exact = {0};
    exact.has_expected = true;
    exact.expected = 4;
    clm_stream_source_memory(&source, clm_bytes_view_from_string("0123456789"));
    clm_stream_sink_buffer(&sink, &out);
    CLM_ASSERT(clm_stream_copy(&source, &sink, &exact, &copied) == CLM_STREAM_SUCCESS && copied == 4 &&
                   clm_stream_test_equals(&out, "0123") && source.state.memory.position == 4,
               "transfer exactly the expected count without reading further");
    exact.expected = 11;
    clm_bytes_buffer_clear(&out);
    clm_stream_source_memory(&source, clm_bytes_view_from_string("0123456789"));
    CLM_ASSERT(clm_stream_copy(&source, &sink, &exact, &copied) == CLM_STREAM_TRUNCATED && copied == 10,
               "report a source that ends before the expected count");
    exact.expected = 0;
    clm_stream_source_memory(&source, clm_bytes_view_from_string("0123456789"));
    CLM_ASSERT(clm_stream_copy(&source, &sink, &exact, &copied) == CLM_STREAM_SUCCESS && copied == 0 &&
                   source.state.memory.position == 0,
               "an expected count of zero reads nothing");

    clm_stream_options capped = {0};
    capped.max_bytes = 10;
    clm_bytes_buffer_clear(&out);
    clm_stream_source_memory(&source, clm_bytes_view_from_string("0123456789"));
    CLM_ASSERT(clm_stream_copy(&source, &sink, &capped, &copied) == CLM_STREAM_SUCCESS && copied == 10,
               "accept data that exactly fills the cap");
    capped.max_bytes = 9;
    clm_bytes_buffer_clear(&out);
    clm_stream_source_memory(&source, clm_bytes_view_from_string("0123456789"));
    CLM_ASSERT(clm_stream_copy(&source, &sink, &capped, &copied) == CLM_STREAM_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_length(&out) <= 9,
               "refuse data past the cap");
    capped.has_expected = true;
    capped.expected = 10;
    CLM_ASSERT(clm_stream_pump_init(&pump, &source, &sink, &capped) == CLM_STREAM_LIMIT_EXCEEDED,
               "refuse an expected count above the cap");

    clm_bytes_buffer limited = {0};
    clm_bytes_buffer_set_limit(&limited, 4);
    clm_stream_source_memory(&source, clm_bytes_view_from_string("0123456789"));
    clm_stream_sink_buffer(&sink, &limited);
    CLM_ASSERT(clm_stream_copy(&source, &sink, &four_bytes, &copied) == CLM_STREAM_LIMIT_EXCEEDED &&
                   clm_bytes_buffer_length(&limited) == 4,
               "report a limited destination buffer as LIMIT_EXCEEDED");
    clm_bytes_buffer_dispose(&limited);

    clm_bytes_buffer leftover = {0};
    clm_bytes_buffer_append_string(&leftover, "abc");
    clm_stream_source first, second, joined;
    clm_bytes_buffer_clear(&out);
    clm_stream_source_buffer(&first, &leftover);
    clm_stream_source_memory(&second, clm_bytes_view_from_string("def"));
    clm_stream_source_concat(&joined, &first, &second);
    clm_stream_sink_buffer(&sink, &out);
    CLM_ASSERT(clm_stream_copy(&joined, &sink, &four_bytes, &copied) == CLM_STREAM_SUCCESS &&
                   clm_stream_test_equals(&out, "abcdef") && clm_bytes_buffer_length(&leftover) == 0,
               "read leftover buffer bytes before a second source");
    clm_bytes_buffer_dispose(&leftover);

    clm_bytes_buffer_clear(&out);
    clm_stream_sink inner, chunked;
    clm_stream_options five = {0};
    five.buffer_size = 5;
    clm_stream_source_memory(&source, clm_bytes_view_from_string("hello world!"));
    clm_stream_sink_buffer(&inner, &out);
    clm_stream_sink_chunked(&chunked, &inner);
    CLM_ASSERT(clm_stream_copy(&source, &chunked, &five, &copied) == CLM_STREAM_SUCCESS && copied == 12 &&
                   clm_stream_test_equals(&out, "5\r\nhello\r\n5\r\n worl\r\n2\r\nd!\r\n0\r\n\r\n"),
               "frame each write as one chunk and end with the zero chunk");
    char decoded[32];
    size_t decoded_length = 0;
    CLM_ASSERT(clm_string_httpp_decode_chunked((const char*)clm_bytes_buffer_view(&out).data,
                       clm_bytes_buffer_length(&out), decoded, sizeof(decoded), &decoded_length, NULL) ==
                       CLM_STRING_HTTPP_SUCCESS && decoded_length == 12 && memcmp(decoded, "hello world!", 12) == 0,
               "decode the chunked output with the HTTP parser");
    clm_stream_sink_dispose(&chunked);

    clm_bytes_buffer wire = {0};
    clm_stream_test_drain slow = {&wire, 0, false};
    clm_stream_test_feed slow_feed = {(const unsigned char*)text, strlen(text), 0, 0};
    clm_stream_source_init(&source, clm_stream_test_feed_read, &slow_feed);
    clm_stream_sink_init(&inner, clm_stream_test_drain_write, clm_stream_test_drain_finish, NULL, &slow);
    clm_stream_sink_chunked(&chunked, &inner);
    CLM_ASSERT(clm_stream_pump_init(&pump, &source, &chunked, &five) == CLM_STREAM_SUCCESS &&
                   clm_stream_test_run_steps(&pump, &seen) == CLM_STREAM_END && slow.finished &&
                   clm_string_httpp_decode_chunked((const char*)clm_bytes_buffer_view(&wire).data,
                       clm_bytes_buffer_length(&wire), decoded, 0, &decoded_length, NULL) ==
                       CLM_STRING_HTTPP_BUFFER_TOO_SMALL && decoded_length == strlen(text),
               "chunk correctly through an inner sink that accepts partial and blocked writes");
    clm_stream_pump_dispose(&pump);
    clm_stream_sink_dispose(&chunked);
    CLM_ASSERT(clm_stream_sink_chunked(NULL, &inner) == CLM_STREAM_INVALID_ARGUMENT,
               "reject invalid chunked sink arguments");
    clm_bytes_buffer_dispose(&wire);

    clm_bytes_buffer_clear(&out);
    unsigned char big[300];
    memset(big, 0x5A, sizeof(big));
    clm_stream_source_memory(&source, (clm_bytes_view){big, sizeof(big)});
    clm_stream_sink_discard(&sink);
    CLM_ASSERT(clm_stream_copy(&source, &sink, &four_bytes, &copied) == CLM_STREAM_SUCCESS && copied == 300,
               "drain a source into the discard sink");

    char path[128];
    snprintf(path, sizeof(path), "clm_stream_test_%llu.tmp", (unsigned long long)clm_gen_random_u64(0, ULONG_MAX));
    const clm_id file_id = 77500;
    const clm_id append_id = 77501;
    const clm_id read_id = 77502;
    clm_bytes_buffer_clear(&out);
    clm_stream_source_memory(&source, clm_bytes_view_from_string("file data;"));
    clm_stream_sink_file(&sink, file_id, false);
    CLM_ASSERT(clm_file_open_as(file_id, path, CLM_FILE_OPEN_MODE_WRITE | CLM_FILE_OPEN_MODE_CREATE |
                                                   CLM_FILE_OPEN_MODE_TRUNCATE) == 0 &&
                   clm_stream_copy(&source, &sink, &four_bytes, &copied) == CLM_STREAM_SUCCESS && copied == 10 &&
                   clm_file_close(file_id) == 0,
               "copy memory into a file sink");
    clm_stream_source_memory(&source, clm_bytes_view_from_string("more"));
    clm_stream_sink_file(&sink, append_id, true);
    CLM_ASSERT(clm_file_open_as(append_id, path, CLM_FILE_OPEN_MODE_APPEND | CLM_FILE_OPEN_MODE_CREATE) == 0 &&
                   clm_stream_copy(&source, &sink, NULL, &copied) == CLM_STREAM_SUCCESS && copied == 4 &&
                   clm_file_close(append_id) == 0,
               "append memory to a file through an append-mode file sink");
    clm_stream_source_file(&source, read_id);
    clm_stream_sink_buffer(&sink, &out);
    CLM_ASSERT(clm_file_open_as(read_id, path, CLM_FILE_OPEN_MODE_READ) == 0 &&
                   clm_stream_copy(&source, &sink, &four_bytes, &copied) == CLM_STREAM_SUCCESS && copied == 14 &&
                   clm_stream_test_equals(&out, "file data;more") && clm_file_close(read_id) == 0,
               "copy a file into a buffer until end of file");
    CLM_ASSERT(clm_file_delete_as(path) == 0 && clm_file_shutdown() == 0, "remove the stream test file");

    const clm_id listener_id = 77510;
    const clm_id client_id = 77511;
    const clm_id accepted_id = 77512;
    clm_net_address loopback = {0};
    clm_net_address listener_address = {0};
    unsigned char payload[5000];
    for (size_t i = 0; i < sizeof(payload); ++i)
        payload[i] = (unsigned char)(i * 31u);
    int network_ready = clm_net_initialize() == CLM_NET_SUCCESS &&
        clm_net_socket_create(listener_id, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS &&
        clm_net_socket_create(client_id, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS &&
        clm_net_address_parse("127.0.0.1", 0, &loopback) == CLM_NET_SUCCESS &&
        clm_net_bind(listener_id, &loopback) == CLM_NET_SUCCESS &&
        clm_net_listen(listener_id, 4) == CLM_NET_SUCCESS &&
        clm_net_socket_local_address(listener_id, &listener_address) == CLM_NET_SUCCESS &&
        clm_net_connect(client_id, &listener_address) == CLM_NET_SUCCESS &&
        clm_net_accept(listener_id, accepted_id, NULL) == CLM_NET_SUCCESS;
    CLM_ASSERT(network_ready, "connect a loopback TCP pair for the socket endpoints");
    if (network_ready) {
        clm_bytes_buffer_clear(&out);
        clm_stream_source_memory(&source, (clm_bytes_view){payload, sizeof(payload)});
        clm_stream_sink_socket(&sink, client_id, true);
        CLM_ASSERT(clm_stream_copy(&source, &sink, &four_bytes, &copied) == CLM_STREAM_SUCCESS &&
                       copied == sizeof(payload),
                   "send a payload over TCP and half-close at the end of data");
        clm_stream_source_socket(&source, accepted_id);
        clm_stream_sink_buffer(&sink, &out);
        CLM_ASSERT(clm_stream_copy(&source, &sink, NULL, &copied) == CLM_STREAM_SUCCESS &&
                       copied == sizeof(payload) &&
                       clm_bytes_view_equals(clm_bytes_buffer_view(&out), (clm_bytes_view){payload, sizeof(payload)}),
                   "receive until the peer closes and match the payload");
    }
    CLM_ASSERT(clm_net_socket_close(client_id) == CLM_NET_SUCCESS || !network_ready,
               "close the client socket");
    clm_net_socket_close(accepted_id);
    clm_net_socket_close(listener_id);
    CLM_ASSERT(clm_net_finalize() == CLM_NET_SUCCESS, "finalize networking");

    clm_bytes_buffer_dispose(&out);
    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
