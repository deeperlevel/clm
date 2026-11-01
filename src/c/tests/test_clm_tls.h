#pragma once

#include "test.h"
#include "../clm/tls.h"
#include "tls_test_credentials.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Writes the throwaway fixtures to temp files because the API takes file paths. */
static bool clm_tls_test_write_file(char* path, size_t capacity, const char* name,
                                    const void* data, size_t length)
{
    const char* directory = getenv("TEMP");
    if (!directory)
        directory = getenv("TMPDIR");
    if (!directory)
        directory = ".";
    if (snprintf(path, capacity, "%s/%s", directory, name) >= (int)capacity)
        return false;
    FILE* file = fopen(path, "wb");
    if (!file)
        return false;
    bool ok = fwrite(data, 1, length, file) == length;
    return fclose(file) == 0 && ok;
}

typedef struct {
    char certificate_path[600];
    char key_path[600];
    clm_tls_options options;
} clm_tls_test_credentials;

static bool clm_tls_test_credentials_create(clm_tls_test_credentials* credentials)
{
    memset(credentials, 0, sizeof(*credentials));
    credentials->options.timeout_ms = 5000;
#ifdef _WIN32
    credentials->options.certificate_password = CLM_TEST_TLS_PFX_PASSWORD;
    credentials->options.certificate_file = credentials->certificate_path;
    return clm_tls_test_write_file(credentials->certificate_path,
        sizeof(credentials->certificate_path), "clm_test_server.pfx",
        clm_test_tls_pfx, sizeof(clm_test_tls_pfx));
#else
    credentials->options.certificate_file = credentials->certificate_path;
    credentials->options.private_key_file = credentials->key_path;
    return clm_tls_test_write_file(credentials->certificate_path,
               sizeof(credentials->certificate_path), "clm_test_server.crt",
               clm_test_tls_certificate_pem, sizeof(clm_test_tls_certificate_pem) - 1) &&
        clm_tls_test_write_file(credentials->key_path, sizeof(credentials->key_path),
               "clm_test_server.key", clm_test_tls_private_key_pem,
               sizeof(clm_test_tls_private_key_pem) - 1);
#endif
}

static void clm_tls_test_credentials_remove(clm_tls_test_credentials* credentials)
{
    if (credentials->certificate_path[0])
        remove(credentials->certificate_path);
    if (credentials->key_path[0])
        remove(credentials->key_path);
}

/* Connects a loopback pair of nonblocking sockets. */
static bool clm_tls_test_pair(clm_id listener, clm_id client, clm_id server)
{
    clm_net_address address, bound;
    bool ok = clm_net_socket_create(listener, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS &&
        clm_net_address_parse("127.0.0.1", 0, &address) == CLM_NET_SUCCESS &&
        clm_net_bind(listener, &address) == CLM_NET_SUCCESS &&
        clm_net_listen(listener, 4) == CLM_NET_SUCCESS &&
        clm_net_socket_local_address(listener, &bound) == CLM_NET_SUCCESS &&
        clm_net_socket_create(client, CLM_NET_IPV4, CLM_NET_TCP) == CLM_NET_SUCCESS &&
        clm_net_connect(client, &bound) == CLM_NET_SUCCESS;
    for (int attempt = 0; ok && attempt < 2000; ++attempt) {
        clm_net_status status = clm_net_accept(listener, server, NULL);
        if (status == CLM_NET_SUCCESS)
            return clm_net_socket_set_nonblocking(client, true) == CLM_NET_SUCCESS &&
                clm_net_socket_set_nonblocking(server, true) == CLM_NET_SUCCESS;
        if (status != CLM_NET_WOULD_BLOCK)
            break;
    }
    return false;
}

static void clm_tls_test_pair_close(clm_id listener, clm_id client, clm_id server)
{
    (void)clm_net_socket_close(server);
    (void)clm_net_socket_close(client);
    (void)clm_net_socket_close(listener);
}

/* Drives both handshakes on one thread; returns the number of WANT_* results seen,
 * or -1 on failure/timeout. */
static int clm_tls_test_handshake(clm_tls_session* client, clm_tls_session* server,
                                  clm_int* client_status, clm_int* server_status)
{
    int waits = 0;
    *client_status = *server_status = CLM_TLS_WANT_READ;
    for (int step = 0; step < 200000; ++step) {
        if (*client_status == CLM_TLS_WANT_READ || *client_status == CLM_TLS_WANT_WRITE) {
            *client_status = clm_tls_handshake(client);
            waits += *client_status == CLM_TLS_WANT_READ || *client_status == CLM_TLS_WANT_WRITE;
        }
        if (*server_status == CLM_TLS_WANT_READ || *server_status == CLM_TLS_WANT_WRITE) {
            *server_status = clm_tls_handshake(server);
            waits += *server_status == CLM_TLS_WANT_READ || *server_status == CLM_TLS_WANT_WRITE;
        }
        bool client_done = *client_status != CLM_TLS_WANT_READ && *client_status != CLM_TLS_WANT_WRITE;
        bool server_done = *server_status != CLM_TLS_WANT_READ && *server_status != CLM_TLS_WANT_WRITE;
        if (client_done && server_done)
            return waits;
        if ((client_done && *client_status != CLM_TLS_SUCCESS) ||
            (server_done && *server_status != CLM_TLS_SUCCESS))
            return waits;
    }
    return -1;
}

static int clm_tls_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - TLS");

    clm_tls_session* session = (clm_tls_session*)(uintptr_t)1;
    unsigned char byte = 0;
    size_t transferred = 99;
    clm_tls_options invalid_server = {0};
    CLM_ASSERT(clm_tls_client_open(1, NULL, NULL, &session) == CLM_TLS_INVALID_ARGUMENT &&
                   session == (clm_tls_session*)(uintptr_t)1 &&
                   clm_tls_client_open(1, "", NULL, &session) == CLM_TLS_INVALID_ARGUMENT &&
                   clm_tls_server_open(1, &invalid_server, &session) == CLM_TLS_INVALID_ARGUMENT,
               "reject invalid client names and missing server credentials without changing output");
    CLM_ASSERT(clm_tls_read(NULL, &byte, 1, &transferred) == CLM_TLS_INVALID_ARGUMENT &&
                   clm_tls_write(NULL, &byte, 1, &transferred) == CLM_TLS_INVALID_ARGUMENT &&
                   clm_tls_close(NULL) == CLM_TLS_INVALID_ARGUMENT,
               "reject operations on a missing TLS session");
    CLM_ASSERT(clm_tls_client_open(1, "example.test", NULL, NULL) == CLM_TLS_INVALID_ARGUMENT &&
                   clm_tls_server_open(1, NULL, NULL) == CLM_TLS_INVALID_ARGUMENT,
               "require a caller-owned session output");

    clm_tls_test_credentials credentials;
    bool have_credentials = clm_tls_test_credentials_create(&credentials);
    CLM_ASSERT(have_credentials && clm_net_initialize() == CLM_NET_SUCCESS,
               "write throwaway server credentials");
    const clm_id listener = 99401, client_id = 99402, server_id = 99403;
    clm_tls_options client_options = {0};
    client_options.timeout_ms = 5000;
    client_options.insecure_skip_verify = true;
    clm_tls_session* client = NULL;
    clm_tls_session* server = NULL;
    clm_int client_status = 0, server_status = 0;
    bool created = have_credentials && clm_tls_test_pair(listener, client_id, server_id) &&
        clm_tls_client_new(client_id, "localhost", &client_options, &client) == CLM_TLS_SUCCESS &&
        clm_tls_server_new(server_id, &credentials.options, &server) == CLM_TLS_SUCCESS &&
        clm_tls_set_nonblocking(client, true) == CLM_TLS_SUCCESS &&
        clm_tls_set_nonblocking(server, true) == CLM_TLS_SUCCESS;
    CLM_ASSERT(created, "create nonblocking client and server sessions without handshaking");
    int waits = created ? clm_tls_test_handshake(client, server, &client_status, &server_status) : -1;
    CLM_ASSERT(waits > 0 && client_status == CLM_TLS_SUCCESS && server_status == CLM_TLS_SUCCESS,
               "complete a TLS handshake through WANT_READ/WANT_WRITE retries");

    unsigned char data[16] = {0};
    transferred = 0;
    bool idle_read = waits > 0 &&
        clm_tls_read(server, data, sizeof(data), &transferred) == CLM_TLS_WANT_READ;
    CLM_ASSERT(idle_read, "report WANT_READ instead of blocking when no data has arrived");
    bool ping = waits > 0 && clm_tls_write(client, "ping", 4, &transferred) == CLM_TLS_SUCCESS &&
        transferred == 4;
    for (int step = 0; ping && step < 1000 && clm_tls_flush(client) != CLM_TLS_SUCCESS; ++step) {}
    size_t received = 0;
    clm_int read_status = CLM_TLS_WANT_READ;
    for (int step = 0; ping && step < 200000 && read_status == CLM_TLS_WANT_READ; ++step)
        read_status = clm_tls_read(server, data, sizeof(data), &received);
    CLM_ASSERT(ping && read_status == CLM_TLS_SUCCESS && received == 4 &&
                   memcmp(data, "ping", 4) == 0,
               "carry application data over a nonblocking session");

    /* A peer that stops reading must produce WANT_WRITE, and every accepted byte must
     * still arrive once the peer resumes. */
    static unsigned char block[65536];
    size_t accepted = 0, delivered = 0;
    bool saw_blocked = false;
    for (int blocks = 0; waits > 0 && blocks < 512 && !saw_blocked; ++blocks) {
        size_t written = 0;
        clm_int status = clm_tls_write(server, block, sizeof(block), &written);
        if (status == CLM_TLS_SUCCESS)
            accepted += written;
        else
            saw_blocked = status == CLM_TLS_WANT_WRITE;
        if (status != CLM_TLS_SUCCESS && !saw_blocked)
            break;
    }
    CLM_ASSERT(saw_blocked, "report WANT_WRITE when the peer stops reading");
    static unsigned char sink[65536];
    for (int step = 0; saw_blocked && step < 200000 && delivered < accepted; ++step) {
        (void)clm_tls_flush(server);
        clm_int status = clm_tls_read(client, sink, sizeof(sink), &received);
        if (status == CLM_TLS_SUCCESS)
            delivered += received;
    }
    CLM_ASSERT(saw_blocked && delivered == accepted && clm_tls_flush(server) == CLM_TLS_SUCCESS,
               "deliver every accepted byte after a blocked write");
    if (client)
        clm_tls_close(client);
    if (server)
        clm_tls_close(server);
    clm_tls_test_pair_close(listener, client_id, server_id);

    /* An untrusted self-signed certificate must be refused unless explicitly skipped. */
    client = server = NULL;
    clm_tls_options strict_options = {0};
    strict_options.timeout_ms = 5000;
    bool strict_created = have_credentials && clm_tls_test_pair(listener, client_id, server_id) &&
        clm_tls_client_new(client_id, "localhost", &strict_options, &client) == CLM_TLS_SUCCESS &&
        clm_tls_server_new(server_id, &credentials.options, &server) == CLM_TLS_SUCCESS &&
        clm_tls_set_nonblocking(client, true) == CLM_TLS_SUCCESS &&
        clm_tls_set_nonblocking(server, true) == CLM_TLS_SUCCESS;
    (void)clm_tls_test_handshake(client, server, &client_status, &server_status);
    CLM_ASSERT(strict_created && client_status != CLM_TLS_SUCCESS &&
                   client_status != CLM_TLS_WANT_READ && client_status != CLM_TLS_WANT_WRITE,
               "reject an untrusted certificate when verification is on");
    if (client)
        clm_tls_close(client);
    if (server)
        clm_tls_close(server);
    clm_tls_test_pair_close(listener, client_id, server_id);
    clm_tls_test_credentials_remove(&credentials);

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0;
}