#pragma once

/*
 * =============================================================================
 * CLM - Cross Language Modules
 *
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain - CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 * =============================================================================
 */

/* TLS 1.2+ client and server sessions over registered CLM TCP sockets.
 * Client sessions verify the peer certificate and hostname by default.
 * Sessions keep a reference to their socket; dispose the TLS session before
 * closing the underlying clm_net socket. Calls are blocking and serialize
 * access to one session; nonblocking sockets are waited on through clm_net,
 * with timeout_ms applied to each readiness wait.
 */

#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLM_TLS_SUCCESS ((clm_int)0)
#define CLM_TLS_CLOSED ((clm_int)1)
#define CLM_TLS_INVALID_ARGUMENT ((clm_int)-1)
#define CLM_TLS_NOT_FOUND ((clm_int)-2)
#define CLM_TLS_UNSUPPORTED ((clm_int)-3)
#define CLM_TLS_SYSTEM_ERROR ((clm_int)-4)
#define CLM_TLS_HANDSHAKE_FAILED ((clm_int)-5)
#define CLM_TLS_CERTIFICATE_ERROR ((clm_int)-6)
#define CLM_TLS_TRUNCATED ((clm_int)-7)
#define CLM_TLS_TIMEOUT ((clm_int)-8)
#define CLM_TLS_NO_MEMORY ((clm_int)-9)

#define CLM_TLS_DEFAULT_TIMEOUT_MS ((uint32_t)30000)

typedef struct clm_tls_session clm_tls_session;

/** TLS settings. timeout_ms zero selects the 30-second default.
 * For servers, Windows expects certificate_file to be a PKCS#12 (.pfx) file
 * and certificate_password to be its password. POSIX expects a PEM certificate
 * chain in certificate_file and a PEM private key in private_key_file.
 */
typedef struct {
    uint32_t timeout_ms;
    const char* certificate_file;
    const char* private_key_file;
    const char* certificate_password;
} clm_tls_options;

/** Creates a client session and completes a verified TLS handshake.
 * server_name is a UTF-8 DNS name or IP literal used for SNI/identity checks.
 * The socket must be a connected TCP socket. On failure, *out_session is
 * unchanged. The caller retains socket ownership.
 */
clm_int clm_tls_client_open(
    clm_id socket_id, const char* server_name,
    const clm_tls_options* options, clm_tls_session** out_session);

/** Creates a server session and completes its TLS handshake using options. */
clm_int clm_tls_server_open(
    clm_id socket_id, const clm_tls_options* options,
    clm_tls_session** out_session);

/** Reads up to capacity decrypted bytes. CLOSED is returned only after the
 * peer's authenticated TLS close_notify; an unclean TCP EOF is TRUNCATED.
 */
clm_int clm_tls_read(
    clm_tls_session* session, void* buffer, size_t capacity,
    size_t* out_received);

/** Writes up to length plaintext bytes; retry any remainder. */
clm_int clm_tls_write(
    clm_tls_session* session, const void* buffer, size_t length,
    size_t* out_written);

/** Sends close_notify where possible, releases TLS resources, and releases its
 * socket reference. It does not close the registered clm_net socket.
 */
clm_int clm_tls_close(clm_tls_session* session);

#ifdef __cplusplus
}
#endif