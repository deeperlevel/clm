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

/* Networking functions and shared socket state are implemented in net.c and
 * provided by the CLM library; no implementation macro is required. String-ID
 * adapters use the CLM string API. On strict C11 POSIX builds, define
 * _POSIX_C_SOURCE=200809L before including any POSIX system headers; the CLM
 * CMake target sets the required feature-test definitions for its consumers.
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "base.h"
#include "string.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "thread.h"

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "Ws2_32.lib")
#endif
#else
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

/** Portable operation result; native error details are available separately. */
typedef enum clm_net_status {
    CLM_NET_SUCCESS = 0,                   /**< Operation completed. */
    CLM_NET_ERROR_INVALID_ARGUMENT = -1,   /**< Input or operation is invalid. */
    CLM_NET_ERROR_NOT_INITIALIZED = -2,    /**< initialize() has not succeeded. */
    CLM_NET_ERROR_NOT_FOUND = -3,          /**< Socket ID is absent or closing. */
    CLM_NET_ERROR_ALREADY_EXISTS = -4,     /**< Socket ID is already registered. */
    CLM_NET_ERROR_SYSTEM = -5,             /**< Native operation failed. */
    CLM_NET_ERROR_RESOLUTION = -6,         /**< Name/address resolution failed. */
    CLM_NET_ERROR_BUFFER_TOO_SMALL = -7,   /**< Caller output capacity is short. */
    CLM_NET_ERROR_UNSUPPORTED = -8,        /**< Platform does not support feature. */
    CLM_NET_ERROR_BUSY = -9,               /**< Resource cannot close/finalize yet. */
    CLM_NET_WOULD_BLOCK = -10,             /**< Nonblocking I/O is not ready. */
    CLM_NET_IN_PROGRESS = -11,             /**< Nonblocking connect is pending. */
    CLM_NET_TIMEOUT = -12,                 /**< A readiness wait timed out. */
    CLM_NET_CLOSED = -13                  /**< TCP peer reached end-of-stream. */
} clm_net_status;

/** IP version used by an address and socket. */
typedef enum clm_net_family {
    CLM_NET_IPV4 = 4,
    CLM_NET_IPV6 = 6
} clm_net_family;

/** Supported transport protocol. */
typedef enum clm_net_transport {
    CLM_NET_TCP = 1,
    CLM_NET_UDP = 2
} clm_net_transport;

/** Direction to half-close on a TCP connection. */
typedef enum clm_net_shutdown_mode {
    CLM_NET_SHUTDOWN_READ = 1,
    CLM_NET_SHUTDOWN_WRITE = 2,
    CLM_NET_SHUTDOWN_BOTH = 3
} clm_net_shutdown_mode;

/** Binary Internet address; port is host byte order, IP bytes are network order.
 * IPv4 uses ip[0..3]; IPv6 uses all 16 bytes. There is no DNS name here. */
typedef struct clm_net_address {
    clm_net_family family;
    uint16_t port;
    uint8_t ip[16];
    uint32_t scope_id; /* IPv6 link-local interface scope, otherwise zero. */
} clm_net_address;

/** Poll interest/result: data or connection state can be read. */
#define CLM_NET_EVENT_READ   0x01u
/** Poll interest/result: a write can proceed without blocking. */
#define CLM_NET_EVENT_WRITE  0x02u
/** Poll result: the peer has closed or disconnected (platform-dependent). */
#define CLM_NET_EVENT_HANGUP 0x04u
/** Poll result: an asynchronous socket error or invalid descriptor occurred. */
#define CLM_NET_EVENT_ERROR  0x08u
/** Poll timeout value that waits without a deadline. */
#define CLM_NET_WAIT_FOREVER ((uint32_t)~(uint32_t)0)

/** One socket's requested/observed readiness in a poll call. */
typedef struct clm_net_poll_entry {
    clm_id socket_id;
    unsigned int events;  /* Input: READ and/or WRITE. */
    unsigned int revents; /* Output: READ, WRITE, HANGUP and/or ERROR. */
} clm_net_poll_entry;

/* ==================== Private implementation ==================== */

#ifdef _WIN32
typedef SOCKET clm_net_private_native_socket;
#define CLM_NET_PRIVATE_INVALID INVALID_SOCKET
typedef int clm_net_private_addrlen;
#else
typedef int clm_net_private_native_socket;
#define CLM_NET_PRIVATE_INVALID (-1)
typedef socklen_t clm_net_private_addrlen;
#endif

typedef struct clm_net_private_socket {
    clm_id id;
    clm_net_private_native_socket fd;
    clm_net_family family;
    clm_net_transport transport;
    size_t references;
    bool closing;
    struct clm_net_private_socket* next;
} clm_net_private_socket;

extern once_flag clm_net_private_once;
extern mtx_t clm_net_private_mutex;
extern cnd_t clm_net_private_changed;
extern bool clm_net_private_lock_valid;
extern bool clm_net_private_initialized;
extern clm_net_private_socket* clm_net_private_sockets;
extern _Thread_local int clm_net_private_last_error;


static void clm_net_private_initialize_mutex(void)
{
    if (mtx_init(&clm_net_private_mutex, mtx_plain) != thrd_success) {
        return;
    }
    /* A close waiting for in-flight socket calls sleeps here until their
     * reference count reaches zero. */
    if (cnd_init(&clm_net_private_changed) != thrd_success) {
        mtx_destroy(&clm_net_private_mutex);
        return;
    }
    clm_net_private_lock_valid = true;
}

bool clm_net_private_lock(void);

void clm_net_private_unlock(void);

int clm_net_private_os_error(void);

clm_net_status clm_net_private_system_error(void);

bool clm_net_private_valid_family(clm_net_family family);

bool clm_net_private_valid_transport(clm_net_transport transport);

int clm_net_private_af(clm_net_family family);

int clm_net_private_socket_type(clm_net_transport transport);

void clm_net_private_close_native(clm_net_private_native_socket fd);

clm_net_status clm_net_private_to_sockaddr(
    const clm_net_address* address, 
    struct sockaddr_storage* storage,
    clm_net_private_addrlen* length
);

clm_net_status clm_net_private_from_sockaddr(
    const struct sockaddr* native, 
    clm_net_private_addrlen length,
    clm_net_address* out_address
);

clm_net_private_socket* clm_net_private_acquire(clm_id id);

void clm_net_private_release(clm_net_private_socket* socket);

clm_net_status clm_net_private_register(
    clm_id id,
    clm_net_private_native_socket fd, 
    clm_net_family family,
    clm_net_transport transport);

/** Initialize networking and its process-wide socket registry. Idempotent.
 * Call before networking operations. On Windows this initializes Winsock.
 * @return SUCCESS or ERROR_SYSTEM.
 */
clm_net_status clm_net_initialize(void);

/** Finalize networking only when no sockets remain registered.
 * All other networking calls must have finished before finalization.
 * @return SUCCESS, ERROR_BUSY, ERROR_NOT_INITIALIZED, or ERROR_SYSTEM.
 */
clm_net_status clm_net_finalize(void);

/** Get the calling thread's last captured native error code.
 * DNS failures store getaddrinfo() codes; other errors store errno/WSA codes.
 * Successful calls do not necessarily clear this value.
 * @return Native error integer, or zero if none was recorded.
 */
int clm_net_last_native_error(void);

clm_net_status clm_net_private_string_view(
    clm_id string_id,
    const char** out_text, 
    size_t* out_bytes
);

/** Parse a numeric IPv4/IPv6 literal without performing DNS.
 * Port is host-order; output is unchanged on failure.
 * @param ip_text Numeric IP literal, without a port or scheme.
 * @param port Host-byte-order port.
 * @param out_address Caller-owned result structure.
 * @return SUCCESS or ERROR_INVALID_ARGUMENT.
 */
clm_net_status clm_net_address_parse(
    const char* ip_text,
    uint16_t port, 
    clm_net_address* out_address
);

/** Parse a numeric IP literal from an existing CLM UTF-8 string ID.
 * @param ip_string_id CLM string containing an IPv4/IPv6 literal.
 * @param port Host-byte-order port.
 * @param out_address Caller-owned result structure.
 * @return SUCCESS or ERROR_INVALID_ARGUMENT.
 */
clm_net_status clm_net_address_parse_string(
    clm_id ip_string_id, 
    uint16_t port,
    clm_net_address* out_address
);

/** Format an address as a NUL-terminated numeric IP without its port.
 * @param address Binary IPv4/IPv6 address.
 * @param out_text Caller-owned output buffer.
 * @param capacity Buffer size in bytes.
 * @return SUCCESS, ERROR_BUFFER_TOO_SMALL, or error.
 */
clm_net_status clm_net_address_format(
    const clm_net_address* address, 
    char* out_text, size_t capacity
);

/** Format an address into a CLM string ID, creating or replacing its value.
 * @param address Binary IPv4/IPv6 address.
 * @param out_string_id Destination CLM string ID.
 * @return SUCCESS, ERROR_INVALID_ARGUMENT, or ERROR_SYSTEM on string failure.
 */
clm_net_status clm_net_address_format_string(
    const clm_net_address* address, 
    clm_id out_string_id
);

/** Resolve a host name or numeric IP to IPv4/IPv6 endpoints.
 * Resolution is blocking. Copies up to capacity results and reports total in
 * out_count; a short array returns BUFFER_TOO_SMALL with a partial result.
 * @param hostname Host name or numeric IP, without scheme or port.
 * @param port Destination port in host byte order.
 * @param transport TCP/UDP resolver hints.
 * @param addresses Caller-owned result array; NULL only when capacity is zero.
 * @param capacity Number of result slots.
 * @param out_count Total matching address count.
 * @return SUCCESS, ERROR_BUFFER_TOO_SMALL, ERROR_RESOLUTION, or error.
 */
clm_net_status clm_net_resolve(
    const char* hostname,
    uint16_t port, 
    clm_net_transport transport, 
    clm_net_address* addresses,
    size_t capacity, 
    size_t* out_count
);

/** Resolve a hostname held in a CLM string ID. Resolution is blocking.
 * @param hostname_string_id ID of a CLM string containing the host name/IP.
 * @param port Destination port in host byte order.
 * @param transport TCP or UDP lookup hints.
 * @param addresses Caller-owned result array (NULL only if capacity is zero).
 * @param capacity Number of result entries.
 * @param out_count Total matching count on SUCCESS/ERROR_BUFFER_TOO_SMALL.
 * @return Same statuses as clm_net_resolve(), or ERROR_INVALID_ARGUMENT for
 *         an invalid string ID/type.
 */
clm_net_status clm_net_resolve_string(
    clm_id hostname_string_id, 
    uint16_t port, 
    clm_net_transport transport,
    clm_net_address* addresses, 
    size_t capacity, 
    size_t* out_count
);

/** Create a blocking socket under a unique caller-chosen CLM ID.
 * @param socket_id Unique ID among currently open sockets.
 * @param family IPv4 or IPv6.
 * @param transport TCP or UDP.
 * @return SUCCESS, ERROR_ALREADY_EXISTS, ERROR_NOT_INITIALIZED, or error.
 */
clm_net_status clm_net_socket_create(
    clm_id socket_id,
    clm_net_family family, 
    clm_net_transport transport
);

/** Close a socket, remove its ID, and wait for in-flight operations to finish.
 * Do not call while another thread can remain blocked in socket I/O.
 * @param socket_id Registered socket ID.
 * @return SUCCESS, ERROR_NOT_FOUND, ERROR_BUSY, or ERROR_SYSTEM.
 */
clm_net_status clm_net_socket_close(clm_id socket_id);

clm_net_status clm_net_private_set_option(
    clm_id socket_id,
    int level, int option, 
    bool enabled
);

/** Enable or disable nonblocking I/O on a socket.
 * @param socket_id Registered socket ID.
 * @param enabled True to enable nonblocking mode.
 * @return SUCCESS or error.
 */
clm_net_status clm_net_socket_set_nonblocking(
    clm_id socket_id, 
    bool enabled
);

/** Enable or disable SO_REUSEADDR; set it before bind when needed.
 * This does not enable SO_REUSEPORT.
 * @param socket_id Registered socket ID.
 * @param enabled True to enable address reuse.
 * @return SUCCESS or error.
 */
clm_net_status clm_net_socket_set_reuse_address(
    clm_id socket_id, 
    bool enabled
);

/** Enable or disable TCP_NODELAY for a TCP socket.
 * @param socket_id Registered TCP socket ID.
 * @param enabled True to disable Nagle's algorithm.
 * @return SUCCESS or error.
 */
clm_net_status clm_net_socket_set_no_delay(
    clm_id socket_id, 
    bool enabled);

/** Configure whether an IPv6 socket accepts IPv4-mapped connections.
 * Usually set before bind; not available on every platform.
 * @param socket_id Registered IPv6 socket ID.
 * @param enabled True to enable IPV6_V6ONLY.
 * @return SUCCESS, ERROR_UNSUPPORTED, or error.
 */
clm_net_status clm_net_socket_set_ipv6_only(
    clm_id socket_id, 
    bool enabled);

/** Bind a socket to a local numeric endpoint; port zero requests an ephemeral
 * port, which can be retrieved with clm_net_socket_local_address().
 * @param socket_id Registered socket ID.
 * @param local_address Local address matching the socket family.
 * @return SUCCESS or error.
 */
clm_net_status clm_net_bind(
    clm_id socket_id,
    const clm_net_address* local_address
);

/** Mark a bound TCP socket as a listener.
 * @param socket_id Registered TCP socket ID.
 * @param backlog Nonnegative pending-connection queue hint.
 * @return SUCCESS or error.
 */
clm_net_status clm_net_listen(
    clm_id socket_id, 
    int backlog
);

/** Accept a TCP connection and register it under a new socket ID.
 * Accepted socket mode may be platform-dependent; configure it explicitly.
 * @param listener_id Registered listening socket ID.
 * @param new_socket_id Unused ID for the accepted socket.
 * @param out_peer_address Optional caller-owned peer result.
 * @return SUCCESS, WOULD_BLOCK, ERROR_ALREADY_EXISTS, or error.
 */
clm_net_status clm_net_accept(
    clm_id listener_id,
    clm_id new_socket_id, 
    clm_net_address* out_peer_address
);

/** Connect a TCP/UDP socket to a numeric endpoint. UDP connect sets a default
 * peer only; it performs no handshake. Nonblocking TCP may return IN_PROGRESS.
 * @param socket_id Registered socket ID.
 * @param remote_address Destination matching the socket family.
 * @return SUCCESS, IN_PROGRESS, or error.
 */
clm_net_status clm_net_connect(
    clm_id socket_id,
    const clm_net_address* remote_address
);

/** Inspect completion of a nonblocking TCP connect using SO_ERROR.
 * @param socket_id Socket with a pending connection.
 * @return SUCCESS when connected, IN_PROGRESS while pending, or error.
 */
clm_net_status clm_net_connect_finish(clm_id socket_id);

size_t clm_net_private_chunk(size_t length);

/** Send bytes on a connected TCP/UDP socket. TCP may send fewer bytes than
 * requested; retry any remainder. The payload is borrowed for this call.
 * @param socket_id Connected socket ID.
 * @param bytes Payload; may be NULL only when length is zero.
 * @param length Number of bytes to send.
 * @param out_sent Receives the number sent on SUCCESS.
 * @return SUCCESS, WOULD_BLOCK, or error.
 */
clm_net_status clm_net_send(
    clm_id socket_id,
    const void* bytes, 
    size_t length, 
    size_t* out_sent
);

/** Send the UTF-8 bytes of a CLM string on a connected socket.
 * No NUL terminator or message framing is added; TCP can send partially.
 * @param socket_id Connected socket ID.
 * @param string_id ID of a CLM string borrowed for this call.
 * @param out_sent Receives bytes sent on SUCCESS.
 * @return Same statuses as clm_net_send(), or INVALID_ARGUMENT for bad string.
 */
clm_net_status clm_net_send_string(
    clm_id socket_id,
    clm_id string_id, 
    size_t* out_sent
);

/** Receive up to capacity bytes from a connected TCP/UDP socket.
 * TCP returning zero means orderly close; a zero-byte UDP datagram is SUCCESS.
 * @param socket_id Connected socket ID.
 * @param buffer Caller-owned writable byte buffer.
 * @param capacity Positive buffer size.
 * @param out_received Receives bytes read on SUCCESS or zero on CLOSED.
 * @return SUCCESS, CLOSED, WOULD_BLOCK, or error.
 */
clm_net_status clm_net_receive(
    clm_id socket_id,
    void* buffer, 
    size_t capacity, 
    size_t* out_received
);

/** Send one UDP datagram to a numeric endpoint.
 * The complete datagram is sent or an error is returned.
 * @param socket_id UDP socket ID.
 * @param destination Numeric endpoint matching the socket family.
 * @param bytes Payload; NULL is allowed only for an empty datagram.
 * @param length Datagram payload size.
 * @param out_sent Receives datagram size on SUCCESS.
 * @return SUCCESS, WOULD_BLOCK, or error.
 */
clm_net_status clm_net_send_to(
    clm_id socket_id,
    const clm_net_address* destination, 
    const void* bytes,
    size_t length, 
    size_t* out_sent
);

/** Send a CLM string's UTF-8 bytes as one UDP datagram, without a NUL byte.
 * @param socket_id UDP socket ID.
 * @param destination Numeric recipient endpoint.
 * @param string_id ID of a CLM string borrowed for this call.
 * @param out_sent Receives bytes sent on SUCCESS.
 * @return Same statuses as clm_net_send_to(), or INVALID_ARGUMENT for bad string.
 */
clm_net_status clm_net_send_to_string(
    clm_id socket_id,
    const clm_net_address* destination, 
    clm_id string_id, 
    size_t* out_sent
);

/** Receive one UDP datagram, optionally returning its numeric source.
 * Datagrams larger than capacity may be truncated without a separate flag.
 * @param socket_id UDP socket ID.
 * @param buffer Caller-owned writable destination.
 * @param capacity Positive buffer size.
 * @param out_received Receives bytes received on SUCCESS.
 * @param out_source Optional caller-owned sender endpoint.
 * @return SUCCESS, WOULD_BLOCK, or error.
 */
clm_net_status clm_net_receive_from(
    clm_id socket_id,
    void* buffer, 
    size_t capacity, 
    size_t* out_received,
    clm_net_address* out_source
);

/** Half-close a connected TCP socket without releasing its registered ID.
 * @param socket_id Connected TCP socket ID.
 * @param mode READ, WRITE, or BOTH.
 * @return SUCCESS or error.
 */
clm_net_status clm_net_socket_shutdown(
    clm_id socket_id,
    clm_net_shutdown_mode mode
);

clm_net_status clm_net_private_get_address(
    clm_id socket_id,
    bool peer, 
    clm_net_address* out_address
);

/** Read the socket's current local numeric endpoint.
 * @param socket_id Registered socket ID.
 * @param out_address Caller-owned address result.
 * @return SUCCESS or error.
 */
clm_net_status clm_net_socket_local_address(
    clm_id socket_id,
    clm_net_address* out_address
);

/** Read the connected peer's numeric endpoint.
 * @param socket_id Connected socket ID.
 * @param out_address Caller-owned address result.
 * @return SUCCESS or error; output is unchanged on failure.
 */
clm_net_status clm_net_socket_peer_address(
    clm_id socket_id,
    clm_net_address* out_address
);

/** Wait for readiness on unique socket IDs; performs no socket I/O.
 * Each entry requests READ/WRITE. TIMEOUT zeros all revents and out_ready;
 * other errors leave outputs unchanged. This call blocks the calling thread.
 * @param entries Mutable poll entries.
 * @param count Number of entries; must be nonzero.
 * @param timeout_ms Milliseconds or CLM_NET_WAIT_FOREVER.
 * @param out_ready Number of entries reporting any event.
 * @return SUCCESS, TIMEOUT, or error.
 */
clm_net_status clm_net_poll(
    clm_net_poll_entry* entries,
    size_t count, 
    uint32_t timeout_ms, 
    size_t* out_ready
);

/** Wait for readiness on one socket; convenience wrapper around poll().
 * @param socket_id Registered socket ID.
 * @param events Requested READ/WRITE flags.
 * @param timeout_ms Milliseconds or CLM_NET_WAIT_FOREVER.
 * @param out_events Receives observed events on SUCCESS/TIMEOUT.
 * @return SUCCESS, TIMEOUT, or error.
 */
clm_net_status clm_net_wait(
    clm_id socket_id,
    unsigned int events, 
    uint32_t timeout_ms, 
    unsigned int* out_events
);

/* ==================== Persistent poller ====================
 *
 * clm_net_poll() re-describes the whole interest set on every call, so its cost
 * is O(sockets) per wake-up and, on Windows, it is capped at FD_SETSIZE sockets.
 * A poller keeps the interest set registered between waits:
 *   Linux            epoll   - O(ready) per wait, no socket-count cap
 *   BSD / macOS      kqueue  - O(ready) per wait, no socket-count cap
 *   Windows / other  WSAPoll / poll over the registered set - no cap, O(n) wait
 * (IOCP is completion-based, not readiness-based, so it cannot back this API.)
 *
 * Semantics are level-triggered on every backend: a socket that is still ready
 * is reported again by the next wait.
 *
 * A poller is NOT thread-safe: use one from a single thread, or serialize calls.
 * It does not own or pin sockets. Call clm_net_poller_remove() BEFORE closing a
 * socket; a socket closed while registered is tolerated but may report an ERROR
 * event until it is removed.
 */

/** Opaque persistent readiness set. */
typedef struct clm_net_poller clm_net_poller;

/** One ready socket returned by clm_net_poller_wait(). */
typedef struct clm_net_poller_event {
    clm_id socket_id;
    void* user_data;       /* The pointer given to clm_net_poller_set(). */
    unsigned int revents;  /* READ, WRITE, HANGUP and/or ERROR. */
} clm_net_poller_event;

/** Create an empty poller.
 * @param out_poller Receives the poller; unchanged on failure.
 * @return SUCCESS or error.
 */
clm_net_status clm_net_poller_create(clm_net_poller** out_poller);

/** Destroy a poller. Registered sockets are not closed. NULL is ignored. */
void clm_net_poller_destroy(clm_net_poller* poller);

/** Name of the readiness backend in use: "epoll", "kqueue", or "poll". */
const char* clm_net_poller_backend(void);

/** Register a socket, or update the interest and user data of a registered one.
 * Re-registering after the socket ID was closed and recreated is supported.
 * @param poller Poller.
 * @param socket_id Registered socket ID.
 * @param events Nonzero READ and/or WRITE interest.
 * @param user_data Opaque pointer returned in events for this socket.
 * @return SUCCESS, NOT_FOUND for an unknown socket, INVALID_ARGUMENT, or error.
 */
clm_net_status clm_net_poller_set(
    clm_net_poller* poller,
    clm_id socket_id,
    unsigned int events,
    void* user_data
);

/** Stop watching a socket. Safe after the socket has already been closed.
 * @return SUCCESS, or NOT_FOUND when the socket is not registered.
 */
clm_net_status clm_net_poller_remove(clm_net_poller* poller, clm_id socket_id);

/** Number of sockets currently registered. */
size_t clm_net_poller_count(const clm_net_poller* poller);

/** Wait for registered sockets to become ready; performs no socket I/O.
 * Returns at most capacity events. Excess ready sockets are reported by later
 * waits. TIMEOUT sets out_count to zero, and so can a signal interruption on
 * POSIX, so callers must recompute their deadline and loop. Waiting forever on
 * an empty poller is INVALID_ARGUMENT because it could never return.
 * @param poller Poller.
 * @param out_events Caller array receiving ready sockets.
 * @param capacity Length of out_events; must be nonzero.
 * @param timeout_ms Milliseconds or CLM_NET_WAIT_FOREVER.
 * @param out_count Number of events written; unchanged on other errors.
 * @return SUCCESS (out_count > 0), TIMEOUT, or error.
 */
clm_net_status clm_net_poller_wait(
    clm_net_poller* poller,
    clm_net_poller_event* out_events,
    size_t capacity,
    uint32_t timeout_ms,
    size_t* out_count
);
