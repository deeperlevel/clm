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
 * closing the underlying clm_net socket. A session serializes access to itself.
 *
 * Two I/O modes exist per session:
 *  - Blocking (the default, and what the *_open functions use): calls wait on the
 *    socket through clm_net, with timeout_ms applied to each readiness wait.
 *  - Nonblocking (clm_tls_set_nonblocking): calls never wait. When the socket cannot
 *    make progress they return CLM_TLS_WANT_READ or CLM_TLS_WANT_WRITE; the caller
 *    waits for that socket event (for example by parking on a poller) and repeats
 *    the same call. Timeouts are then the caller's responsibility.
 * TLS can need the opposite event from the operation: a read may need to write and a
 * write may need to read, so always honour the event that is requested.
 */

#include "net.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLM_TLS_SUCCESS ((clm_int)0)
#define CLM_TLS_CLOSED ((clm_int)1)
/** Nonblocking only: wait for the socket to become readable, then repeat the call. */
#define CLM_TLS_WANT_READ ((clm_int)2)
/** Nonblocking only: wait for the socket to become writable, then repeat the call. */
#define CLM_TLS_WANT_WRITE ((clm_int)3)
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
    /** Client only. When true the peer certificate and hostname are NOT verified, which
     * removes all protection against impersonation. Intended for tests against
     * self-signed servers; never enable it for real traffic. */
    bool insecure_skip_verify;
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

/** Like clm_tls_client_open but returns before any handshake I/O, so the handshake can
 * be driven nonblocking: call clm_tls_set_nonblocking, then clm_tls_handshake until it
 * stops returning WANT_READ / WANT_WRITE. */
clm_int clm_tls_client_new(
    clm_id socket_id, const char* server_name,
    const clm_tls_options* options, clm_tls_session** out_session);

/** Like clm_tls_server_open but returns before any handshake I/O. */
clm_int clm_tls_server_new(
    clm_id socket_id, const clm_tls_options* options,
    clm_tls_session** out_session);

/** Selects nonblocking (true) or blocking (false) I/O for this session. Switching is
 * safe between calls, including in the middle of a handshake. */
clm_int clm_tls_set_nonblocking(clm_tls_session* session, bool nonblocking);

/** Advances the handshake. Returns SUCCESS once complete (and on every later call, after
 * flushing any handshake bytes still queued), WANT_READ / WANT_WRITE in nonblocking mode,
 * or a handshake error. In blocking mode it never returns WANT_*. */
clm_int clm_tls_handshake(clm_tls_session* session);

/** Reads up to capacity decrypted bytes. CLOSED is returned only after the
 * peer's authenticated TLS close_notify; an unclean TCP EOF is TRUNCATED.
 * In nonblocking mode *out_received is left untouched on WANT_READ/WANT_WRITE.
 */
clm_int clm_tls_read(
    clm_tls_session* session, void* buffer, size_t capacity,
    size_t* out_received);

/** Writes up to length plaintext bytes; retry any remainder. In nonblocking mode a
 * successful write only means the bytes were accepted: encrypted bytes the socket
 * could not take yet stay queued inside the session, so call clm_tls_flush (or keep
 * writing) until it reports SUCCESS before relying on delivery or closing.
 * WANT_WRITE means nothing was accepted. */
clm_int clm_tls_write(
    clm_tls_session* session, const void* buffer, size_t length,
    size_t* out_written);

/** Sends encrypted bytes queued by earlier writes or the handshake. Returns SUCCESS when
 * nothing remains queued, or WANT_WRITE in nonblocking mode. Always SUCCESS if blocking. */
clm_int clm_tls_flush(clm_tls_session* session);

/** Sends close_notify where possible, releases TLS resources, and releases its
 * socket reference. It does not close the registered clm_net socket.
 */
clm_int clm_tls_close(clm_tls_session* session);

#ifdef __cplusplus
}
#endif