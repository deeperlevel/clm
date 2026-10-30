#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

/* Define CLM_NET_IMPLEMENTATION in exactly one translation unit. All public
 * functions are header-only; the macro instantiates the shared socket table.
 * String-ID adapters additionally use clm_string; instantiate
 * CLM_STRING_IMPLEMENTATION and its CLM_BPA_IMPLEMENTATION dependency as needed.
 * Include clm/net.h before other POSIX system headers on strict C11 builds,
 * or define _POSIX_C_SOURCE=200809L in your build configuration. */
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
#include <threads.h>

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

#ifdef CLM_NET_IMPLEMENTATION
once_flag clm_net_private_once = ONCE_FLAG_INIT;
mtx_t clm_net_private_mutex;
cnd_t clm_net_private_changed;
bool clm_net_private_lock_valid = false;
bool clm_net_private_initialized = false;
clm_net_private_socket* clm_net_private_sockets = NULL;
_Thread_local int clm_net_private_last_error = 0;
#endif

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

static inline bool clm_net_private_lock(void)
{
    call_once(&clm_net_private_once, clm_net_private_initialize_mutex);
    if (!clm_net_private_lock_valid) {
        return false;
    }
    return mtx_lock(&clm_net_private_mutex) == thrd_success;
}

static inline void clm_net_private_unlock(void)
{
    mtx_unlock(&clm_net_private_mutex);
}

static inline int clm_net_private_os_error(void)
{
    #ifdef _WIN32
    return WSAGetLastError();
    #else
    return errno;
    #endif
}

static inline clm_net_status clm_net_private_system_error(void)
{
    /* Preserve the native code for diagnostics while exposing a small,
     * consistent set of statuses to callers. */
    int error = clm_net_private_os_error();
    clm_net_private_last_error = error;
    #ifdef _WIN32
    if (error == WSAEWOULDBLOCK) {
        return CLM_NET_WOULD_BLOCK;
    }
    if (error == WSAEINPROGRESS || error == WSAEALREADY) {
        return CLM_NET_IN_PROGRESS;
    }
    if (error == WSAETIMEDOUT) {
        return CLM_NET_TIMEOUT;
    }
    #else
    if (error == EAGAIN || error == EWOULDBLOCK) {
        return CLM_NET_WOULD_BLOCK;
    }
    if (error == EINPROGRESS || error == EALREADY) {
        return CLM_NET_IN_PROGRESS;
    }
    if (error == ETIMEDOUT) {
        return CLM_NET_TIMEOUT;
    }
    #endif
    return CLM_NET_ERROR_SYSTEM;
}

static inline bool clm_net_private_valid_family(clm_net_family family)
{
    return family == CLM_NET_IPV4 || family == CLM_NET_IPV6;
}

static inline bool clm_net_private_valid_transport(clm_net_transport transport)
{
    return transport == CLM_NET_TCP || transport == CLM_NET_UDP;
}

static inline int clm_net_private_af(clm_net_family family)
{
    return family == CLM_NET_IPV4 ? AF_INET : AF_INET6;
}

static inline int clm_net_private_socket_type(clm_net_transport transport)
{
    return transport == CLM_NET_TCP ? SOCK_STREAM : SOCK_DGRAM;
}

static inline void clm_net_private_close_native(clm_net_private_native_socket fd)
{
    #ifdef _WIN32
    closesocket(fd);
    #else
    close(fd);
    #endif
}

static inline clm_net_status clm_net_private_to_sockaddr(
    const clm_net_address* address, 
    struct sockaddr_storage* storage,
    clm_net_private_addrlen* length
)
{
    if (address == NULL || storage == NULL || length == NULL ||
        !clm_net_private_valid_family(address->family)) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }

    memset(storage, 0, sizeof(*storage));
    /* Keep CLM's host-order port representation at the API boundary. */
    if (address->family == CLM_NET_IPV4) {
        struct sockaddr_in* ipv4 = (struct sockaddr_in*)storage;
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons(address->port);
        memcpy(&ipv4->sin_addr, address->ip, 4);
        *length = (clm_net_private_addrlen)sizeof(*ipv4);
    } else {
        struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)storage;
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons(address->port);
        ipv6->sin6_scope_id = address->scope_id;
        memcpy(&ipv6->sin6_addr, address->ip, 16);
        *length = (clm_net_private_addrlen)sizeof(*ipv6);
    }
    return CLM_NET_SUCCESS;
}

static inline clm_net_status clm_net_private_from_sockaddr(
    const struct sockaddr* native, 
    clm_net_private_addrlen length,
    clm_net_address* out_address
)
{
    clm_net_address address;
    memset(&address, 0, sizeof(address));

    if (native == NULL || out_address == NULL) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    if (native->sa_family == AF_INET && length >= sizeof(struct sockaddr_in)) {
        const struct sockaddr_in* ipv4 = (const struct sockaddr_in*)native;
        address.family = CLM_NET_IPV4;
        address.port = ntohs(ipv4->sin_port);
        memcpy(address.ip, &ipv4->sin_addr, 4);
    } else if (native->sa_family == AF_INET6 &&
               length >= sizeof(struct sockaddr_in6)) {
        const struct sockaddr_in6* ipv6 = (const struct sockaddr_in6*)native;
        address.family = CLM_NET_IPV6;
        address.port = ntohs(ipv6->sin6_port);
        address.scope_id = ipv6->sin6_scope_id;
        memcpy(address.ip, &ipv6->sin6_addr, 16);
    } else {
        return CLM_NET_ERROR_UNSUPPORTED;
    }
    *out_address = address;
    return CLM_NET_SUCCESS;
}

static inline clm_net_private_socket* clm_net_private_acquire(clm_id id)
{
    /* The reference pins the registry record and native handle after the
     * global lock is released, so I/O does not hold the registry lock. */
    clm_net_private_socket* socket = NULL;
    if (!clm_net_private_lock()) {
        return NULL;
    }
    if (clm_net_private_initialized) {
        for (socket = clm_net_private_sockets; socket != NULL;
             socket = socket->next) {
            if (socket->id == id) {
                if (!socket->closing) {
                    ++socket->references;
                } else {
                    socket = NULL;
                }
                break;
            }
        }
    }
    clm_net_private_unlock();
    return socket;
}

static inline void clm_net_private_release(clm_net_private_socket* socket)
{
    if (socket == NULL) {
        return;
    }
    if (!clm_net_private_lock()) {
        return;
    }
    --socket->references;
    /* Wake close only after the final operation lets go of this socket. */
    if (socket->closing && socket->references == 0) {
        cnd_broadcast(&clm_net_private_changed);
    }
    clm_net_private_unlock();
}

static inline clm_net_status clm_net_private_register(
    clm_id id,
    clm_net_private_native_socket fd, 
    clm_net_family family,
    clm_net_transport transport)
{
    clm_net_private_socket* created =
        (clm_net_private_socket*)calloc(1, sizeof(*created));
    if (created == NULL) {
        return CLM_NET_ERROR_SYSTEM;
    }
    created->id = id;
    created->fd = fd;
    created->family = family;
    created->transport = transport;

    if (!clm_net_private_lock()) {
        free(created);
        return CLM_NET_ERROR_SYSTEM;
    }
    if (!clm_net_private_initialized) {
        clm_net_private_unlock();
        free(created);
        return CLM_NET_ERROR_NOT_INITIALIZED;
    }
    /* Registration and duplicate-ID checking share the same lock so creation
     * and accept cannot replace a live resource under concurrent callers. */
    for (clm_net_private_socket* current = clm_net_private_sockets;
         current != NULL; current = current->next) {
        if (current->id == id) {
            clm_net_private_unlock();
            free(created);
            return CLM_NET_ERROR_ALREADY_EXISTS;
        }
    }
    created->next = clm_net_private_sockets;
    clm_net_private_sockets = created;
    clm_net_private_unlock();
    return CLM_NET_SUCCESS;
}

/** Initialize networking and its process-wide socket registry. Idempotent.
 * Call before networking operations. On Windows this initializes Winsock.
 * @return SUCCESS or ERROR_SYSTEM.
 */
static inline clm_net_status clm_net_initialize(void)
{
    clm_net_private_last_error = 0;
    if (!clm_net_private_lock()) {
        return CLM_NET_ERROR_SYSTEM;
    }
    if (clm_net_private_initialized) {
        clm_net_private_unlock();
        return CLM_NET_SUCCESS;
    }
    #ifdef _WIN32
    WSADATA data;
    int result = WSAStartup(MAKEWORD(2, 2), &data);
    if (result != 0) {
        clm_net_private_last_error = result;
        clm_net_private_unlock();
        return CLM_NET_ERROR_SYSTEM;
    }
    #endif
    clm_net_private_initialized = true;
    clm_net_private_unlock();
    return CLM_NET_SUCCESS;
}

/** Finalize networking only when no sockets remain registered.
 * All other networking calls must have finished before finalization.
 * @return SUCCESS, ERROR_BUSY, ERROR_NOT_INITIALIZED, or ERROR_SYSTEM.
 */
static inline clm_net_status clm_net_finalize(void)
{
    clm_net_private_last_error = 0;
    if (!clm_net_private_lock()) {
        return CLM_NET_ERROR_SYSTEM;
    }
    if (!clm_net_private_initialized) {
        clm_net_private_unlock();
        return CLM_NET_ERROR_NOT_INITIALIZED;
    }
    if (clm_net_private_sockets != NULL) {
        clm_net_private_unlock();
        return CLM_NET_ERROR_BUSY;
    }
    #ifdef _WIN32
    if (WSACleanup() != 0) {
        clm_net_status status = clm_net_private_system_error();
        clm_net_private_unlock();
        return status;
    }
    #endif
    clm_net_private_initialized = false;
    clm_net_private_unlock();
    return CLM_NET_SUCCESS;
}

/** Get the calling thread's last captured native error code.
 * DNS failures store getaddrinfo() codes; other errors store errno/WSA codes.
 * Successful calls do not necessarily clear this value.
 * @return Native error integer, or zero if none was recorded.
 */
static inline int clm_net_last_native_error(void)
{
    return clm_net_private_last_error;
}

static inline clm_net_status clm_net_private_string_view(
    clm_id string_id,
    const char** out_text, 
    size_t* out_bytes
)
{
    clm_int character_count = clm_string_length(string_id);
    if (out_text == NULL || out_bytes == NULL || character_count < 0 ||
        clm_string_slice_borrow(string_id, 0, character_count, out_text,
            out_bytes) != CLM_BPA_SUCCESS) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    return CLM_NET_SUCCESS;
}

/** Parse a numeric IPv4/IPv6 literal without performing DNS.
 * Port is host-order; output is unchanged on failure.
 * @param ip_text Numeric IP literal, without a port or scheme.
 * @param port Host-byte-order port.
 * @param out_address Caller-owned result structure.
 * @return SUCCESS or ERROR_INVALID_ARGUMENT.
 */
static inline clm_net_status clm_net_address_parse(
    const char* ip_text,
    uint16_t port, 
    clm_net_address* out_address
)
{
    if (ip_text == NULL || out_address == NULL) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_address address;
    memset(&address, 0, sizeof(address));
    address.port = port;

    if (inet_pton(AF_INET, ip_text, address.ip) == 1) {
        address.family = CLM_NET_IPV4;
    } else if (inet_pton(AF_INET6, ip_text, address.ip) == 1) {
        address.family = CLM_NET_IPV6;
    } else {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    *out_address = address;
    return CLM_NET_SUCCESS;
}

/** Parse a numeric IP literal from an existing CLM UTF-8 string ID.
 * @param ip_string_id CLM string containing an IPv4/IPv6 literal.
 * @param port Host-byte-order port.
 * @param out_address Caller-owned result structure.
 * @return SUCCESS or ERROR_INVALID_ARGUMENT.
 */
static inline clm_net_status clm_net_address_parse_string(
    clm_id ip_string_id, 
    uint16_t port,
    clm_net_address* out_address
)
{
    const char* ip_text;
    size_t ip_bytes;
    clm_net_status status = clm_net_private_string_view(ip_string_id,
        &ip_text, &ip_bytes);
    (void)ip_bytes;
    return status == CLM_NET_SUCCESS ?
        clm_net_address_parse(ip_text, port, out_address) : status;
}

/** Format an address as a NUL-terminated numeric IP without its port.
 * @param address Binary IPv4/IPv6 address.
 * @param out_text Caller-owned output buffer.
 * @param capacity Buffer size in bytes.
 * @return SUCCESS, ERROR_BUFFER_TOO_SMALL, or error.
 */
static inline clm_net_status clm_net_address_format(
    const clm_net_address* address, 
    char* out_text, size_t capacity
)
{
    if (address == NULL || out_text == NULL || capacity == 0 ||
        !clm_net_private_valid_family(address->family)) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }

    char text[INET6_ADDRSTRLEN];
    int af = clm_net_private_af(address->family);
    if (inet_ntop(af, address->ip, text, (clm_net_private_addrlen)sizeof(text)) == NULL) {
        return clm_net_private_system_error();
    }
    size_t required = strlen(text) + 1;
    if (capacity < required) {
        return CLM_NET_ERROR_BUFFER_TOO_SMALL;
    }
    memcpy(out_text, text, required);
    return CLM_NET_SUCCESS;
}

/** Format an address into a CLM string ID, creating or replacing its value.
 * @param address Binary IPv4/IPv6 address.
 * @param out_string_id Destination CLM string ID.
 * @return SUCCESS, ERROR_INVALID_ARGUMENT, or ERROR_SYSTEM on string failure.
 */
static inline clm_net_status clm_net_address_format_string(
    const clm_net_address* address, 
    clm_id out_string_id
)
{
    char text[INET6_ADDRSTRLEN];
    clm_net_status status = clm_net_address_format(address, text,
        sizeof(text));
    if (status != CLM_NET_SUCCESS) {
        return status;
    }
    return clm_string_from_from(out_string_id, text) == CLM_BPA_SUCCESS ?
        CLM_NET_SUCCESS : CLM_NET_ERROR_SYSTEM;
}

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
static inline clm_net_status clm_net_resolve(
    const char* hostname,
    uint16_t port, 
    clm_net_transport transport, 
    clm_net_address* addresses,
    size_t capacity, 
    size_t* out_count
)
{
    if (hostname == NULL || out_count == NULL ||
        !clm_net_private_valid_transport(transport) ||
        (capacity > 0 && addresses == NULL)) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }

    struct addrinfo hints;
    struct addrinfo* results = NULL;
    char service[8];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = clm_net_private_socket_type(transport);
    hints.ai_protocol = transport == CLM_NET_TCP ? IPPROTO_TCP : IPPROTO_UDP;
    snprintf(service, sizeof(service), "%u", (unsigned int)port);

    clm_net_private_last_error = 0;
    if (!clm_net_private_lock()) {
        return CLM_NET_ERROR_SYSTEM;
    }
    bool initialized = clm_net_private_initialized;
    clm_net_private_unlock();
    if (!initialized) {
        return CLM_NET_ERROR_NOT_INITIALIZED;
    }

    /* DNS can block, so do not hold the registry mutex while resolving. */
    int result = getaddrinfo(hostname, service, &hints, &results);
    if (result != 0) {
        clm_net_private_last_error = result;
        return CLM_NET_ERROR_RESOLUTION;
    }

    /* Copy only the caller's capacity but count every supported result. */
    size_t count = 0;
    for (struct addrinfo* current = results; current != NULL;
         current = current->ai_next) {
        clm_net_address address;
        if (clm_net_private_from_sockaddr(current->ai_addr,
                (clm_net_private_addrlen)current->ai_addrlen, &address) != CLM_NET_SUCCESS) {
            continue;
        }
        if (count < capacity) {
            addresses[count] = address;
        }
        ++count;
    }
    freeaddrinfo(results);
    *out_count = count;
    return count > capacity ? CLM_NET_ERROR_BUFFER_TOO_SMALL : CLM_NET_SUCCESS;
}

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
static inline clm_net_status clm_net_resolve_string(
    clm_id hostname_string_id, 
    uint16_t port, 
    clm_net_transport transport,
    clm_net_address* addresses, 
    size_t capacity, 
    size_t* out_count
)
{
    const char* hostname;
    size_t hostname_bytes;
    clm_net_status status = clm_net_private_string_view(hostname_string_id,
        &hostname, &hostname_bytes);
    (void)hostname_bytes;
    return status == CLM_NET_SUCCESS ?
        clm_net_resolve(hostname, port, transport, addresses, capacity,
            out_count) : status;
}

/** Create a blocking socket under a unique caller-chosen CLM ID.
 * @param socket_id Unique ID among currently open sockets.
 * @param family IPv4 or IPv6.
 * @param transport TCP or UDP.
 * @return SUCCESS, ERROR_ALREADY_EXISTS, ERROR_NOT_INITIALIZED, or error.
 */
static inline clm_net_status clm_net_socket_create(
    clm_id socket_id,
    clm_net_family family, 
    clm_net_transport transport
)
{
    if (!clm_net_private_valid_family(family) ||
        !clm_net_private_valid_transport(transport)) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    if (!clm_net_private_lock()) {
        return CLM_NET_ERROR_SYSTEM;
    }
    bool initialized = clm_net_private_initialized;
    clm_net_private_unlock();
    if (!initialized) {
        return CLM_NET_ERROR_NOT_INITIALIZED;
    }

    clm_net_private_last_error = 0;
    clm_net_private_native_socket fd = socket(clm_net_private_af(family),
        clm_net_private_socket_type(transport),
        transport == CLM_NET_TCP ? IPPROTO_TCP : IPPROTO_UDP);
    if (fd == CLM_NET_PRIVATE_INVALID) {
        return clm_net_private_system_error();
    }
    #ifdef SO_NOSIGPIPE
    if (transport == CLM_NET_TCP) {
        int enabled = 1;
        (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE,
            (const char*)&enabled, (clm_net_private_addrlen)sizeof(enabled));
    }
    #endif
    clm_net_status status = clm_net_private_register(socket_id, fd, family,
        transport);
    if (status != CLM_NET_SUCCESS) {
        clm_net_private_close_native(fd);
    }
    return status;
}

/** Close a socket, remove its ID, and wait for in-flight operations to finish.
 * Do not call while another thread can remain blocked in socket I/O.
 * @param socket_id Registered socket ID.
 * @return SUCCESS, ERROR_NOT_FOUND, ERROR_BUSY, or ERROR_SYSTEM.
 */
static inline clm_net_status clm_net_socket_close(clm_id socket_id)
{
    if (!clm_net_private_lock()) {
        return CLM_NET_ERROR_SYSTEM;
    }
    clm_net_private_socket** link = &clm_net_private_sockets;
    while (*link != NULL && (*link)->id != socket_id) {
        link = &(*link)->next;
    }
    clm_net_private_socket* socket = *link;
    if (socket == NULL) {
        clm_net_private_unlock();
        return CLM_NET_ERROR_NOT_FOUND;
    }
    if (socket->closing) {
        clm_net_private_unlock();
        return CLM_NET_ERROR_BUSY;
    }
    /* Mark it unavailable before waiting: no new operation can acquire it. */
    socket->closing = true;
    /* Existing blocking operations must complete before this call can return.
     * The caller should coordinate their completion (for example by waking a
     * reactor) before closing; shutdown on a listener is not portable. */
    while (socket->references != 0) {
        cnd_wait(&clm_net_private_changed, &clm_net_private_mutex);
    }
    /* Earlier close() calls can modify list links while cnd_wait unlocks.
     * Relocate the current link before removing this record. */
    link = &clm_net_private_sockets;
    while (*link != NULL && *link != socket) {
        link = &(*link)->next;
    }
    clm_net_private_close_native(socket->fd);
    *link = socket->next;
    clm_net_private_unlock();
    free(socket);
    return CLM_NET_SUCCESS;
}

static inline clm_net_status clm_net_private_set_option(
    clm_id socket_id,
    int level, int option, 
    bool enabled
)
{
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    int value = enabled ? 1 : 0;
    int result = setsockopt(socket->fd, level, option, (const char*)&value,
        (clm_net_private_addrlen)sizeof(value));
    clm_net_status status = result == 0 ? CLM_NET_SUCCESS :
        clm_net_private_system_error();
    clm_net_private_release(socket);
    return status;
}

/** Enable or disable nonblocking I/O on a socket.
 * @param socket_id Registered socket ID.
 * @param enabled True to enable nonblocking mode.
 * @return SUCCESS or error.
 */
static inline clm_net_status clm_net_socket_set_nonblocking(
    clm_id socket_id, 
    bool enabled
)
{
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    clm_net_status status = CLM_NET_SUCCESS;
    #ifdef _WIN32
    u_long mode = enabled ? 1UL : 0UL;
    if (ioctlsocket(socket->fd, FIONBIO, &mode) != 0) {
        status = clm_net_private_system_error();
    }
    #else
    int flags = fcntl(socket->fd, F_GETFL, 0);
    if (flags == -1 || fcntl(socket->fd, F_SETFL,
            enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK)) == -1) {
        status = clm_net_private_system_error();
    }
    #endif
    clm_net_private_release(socket);
    return status;
}

/** Enable or disable SO_REUSEADDR; set it before bind when needed.
 * This does not enable SO_REUSEPORT.
 * @param socket_id Registered socket ID.
 * @param enabled True to enable address reuse.
 * @return SUCCESS or error.
 */
static inline clm_net_status clm_net_socket_set_reuse_address(
    clm_id socket_id, 
    bool enabled
)
{
    return clm_net_private_set_option(socket_id, SOL_SOCKET, SO_REUSEADDR,enabled);
}

/** Enable or disable TCP_NODELAY for a TCP socket.
 * @param socket_id Registered TCP socket ID.
 * @param enabled True to disable Nagle's algorithm.
 * @return SUCCESS or error.
 */
static inline clm_net_status clm_net_socket_set_no_delay(
    clm_id socket_id, 
    bool enabled)
{
    return clm_net_private_set_option(socket_id, IPPROTO_TCP, TCP_NODELAY,
        enabled);
}

/** Configure whether an IPv6 socket accepts IPv4-mapped connections.
 * Usually set before bind; not available on every platform.
 * @param socket_id Registered IPv6 socket ID.
 * @param enabled True to enable IPV6_V6ONLY.
 * @return SUCCESS, ERROR_UNSUPPORTED, or error.
 */
static inline clm_net_status clm_net_socket_set_ipv6_only(
    clm_id socket_id, 
    bool enabled)
{
    #ifdef IPV6_V6ONLY
    return clm_net_private_set_option(socket_id, IPPROTO_IPV6, IPV6_V6ONLY,
        enabled);
    #else
    (void)socket_id;
    (void)enabled;
    return CLM_NET_ERROR_UNSUPPORTED;
    #endif
}

/** Bind a socket to a local numeric endpoint; port zero requests an ephemeral
 * port, which can be retrieved with clm_net_socket_local_address().
 * @param socket_id Registered socket ID.
 * @param local_address Local address matching the socket family.
 * @return SUCCESS or error.
 */
static inline clm_net_status clm_net_bind(
    clm_id socket_id,
    const clm_net_address* local_address
)
{
    struct sockaddr_storage native;
    clm_net_private_addrlen length;
    clm_net_status status = clm_net_private_to_sockaddr(local_address,
        &native, &length);
    if (status != CLM_NET_SUCCESS) {
        return status;
    }
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    if (socket->family != local_address->family) {
        clm_net_private_release(socket);
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    if (bind(socket->fd, (struct sockaddr*)&native, length) != 0) {
        status = clm_net_private_system_error();
    }
    clm_net_private_release(socket);
    return status;
}

/** Mark a bound TCP socket as a listener.
 * @param socket_id Registered TCP socket ID.
 * @param backlog Nonnegative pending-connection queue hint.
 * @return SUCCESS or error.
 */
static inline clm_net_status clm_net_listen(
    clm_id socket_id, 
    int backlog
)
{
    if (backlog < 0) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    clm_net_status status = CLM_NET_SUCCESS;
    if (socket->transport != CLM_NET_TCP) {
        status = CLM_NET_ERROR_INVALID_ARGUMENT;
    } else if (listen(socket->fd, backlog) != 0) {
        status = clm_net_private_system_error();
    }
    clm_net_private_release(socket);
    return status;
}

/** Accept a TCP connection and register it under a new socket ID.
 * Accepted socket mode may be platform-dependent; configure it explicitly.
 * @param listener_id Registered listening socket ID.
 * @param new_socket_id Unused ID for the accepted socket.
 * @param out_peer_address Optional caller-owned peer result.
 * @return SUCCESS, WOULD_BLOCK, ERROR_ALREADY_EXISTS, or error.
 */
static inline clm_net_status clm_net_accept(
    clm_id listener_id,
    clm_id new_socket_id, 
    clm_net_address* out_peer_address
)
{
    clm_net_private_socket* listener = clm_net_private_acquire(listener_id);
    if (listener == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    if (listener->transport != CLM_NET_TCP) {
        clm_net_private_release(listener);
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    struct sockaddr_storage native;
    clm_net_private_addrlen length = (clm_net_private_addrlen)sizeof(native);
    clm_net_private_native_socket fd = accept(listener->fd,
        (struct sockaddr*)&native, &length);
    clm_net_family family = listener->family;
    clm_net_status accept_status = fd == CLM_NET_PRIVATE_INVALID ?
        clm_net_private_system_error() : CLM_NET_SUCCESS;
    clm_net_private_release(listener);
    if (accept_status != CLM_NET_SUCCESS) {
        return accept_status;
    }
    clm_net_address peer;
    clm_net_status status = clm_net_private_from_sockaddr(
        (struct sockaddr*)&native, length, &peer);
    if (status == CLM_NET_SUCCESS) {
    #ifdef SO_NOSIGPIPE
        int suppress_sigpipe = 1;
        (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE,
            (const char*)&suppress_sigpipe,
            (clm_net_private_addrlen)sizeof(suppress_sigpipe));
    #endif
        status = clm_net_private_register(new_socket_id, fd, family, CLM_NET_TCP);
    }
    if (status != CLM_NET_SUCCESS) {
        clm_net_private_close_native(fd);
        return status;
    }
    if (out_peer_address != NULL) {
        *out_peer_address = peer;
    }
    return CLM_NET_SUCCESS;
}

/** Connect a TCP/UDP socket to a numeric endpoint. UDP connect sets a default
 * peer only; it performs no handshake. Nonblocking TCP may return IN_PROGRESS.
 * @param socket_id Registered socket ID.
 * @param remote_address Destination matching the socket family.
 * @return SUCCESS, IN_PROGRESS, or error.
 */
static inline clm_net_status clm_net_connect(
    clm_id socket_id,
    const clm_net_address* remote_address
)
{
    struct sockaddr_storage native;
    clm_net_private_addrlen length;
    clm_net_status status = clm_net_private_to_sockaddr(remote_address,
        &native, &length);
    if (status != CLM_NET_SUCCESS) {
        return status;
    }
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    if (socket->family != remote_address->family) {
        clm_net_private_release(socket);
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    if (connect(socket->fd, (struct sockaddr*)&native, length) != 0) {
        status = clm_net_private_system_error();
        if (status == CLM_NET_WOULD_BLOCK) {
            status = CLM_NET_IN_PROGRESS;
        }
    }
    clm_net_private_release(socket);
    return status;
}

/** Inspect completion of a nonblocking TCP connect using SO_ERROR.
 * @param socket_id Socket with a pending connection.
 * @return SUCCESS when connected, IN_PROGRESS while pending, or error.
 */
static inline clm_net_status clm_net_connect_finish(clm_id socket_id)
{
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    int error = 0;
    clm_net_private_addrlen length = (clm_net_private_addrlen)sizeof(error);
    clm_net_status status = CLM_NET_SUCCESS;
    if (getsockopt(socket->fd, SOL_SOCKET, SO_ERROR,
            (char*)&error, &length) != 0) {
        status = clm_net_private_system_error();
    } else if (error == 0 && socket->transport == CLM_NET_TCP) {
        /* SO_ERROR == 0 alone does not establish that an asynchronous
         * connection has completed. Check the peer endpoint as well. */
        struct sockaddr_storage peer;
        clm_net_private_addrlen peer_length =
            (clm_net_private_addrlen)sizeof(peer);
        if (getpeername(socket->fd, (struct sockaddr*)&peer,
                &peer_length) != 0) {
            clm_net_private_last_error = clm_net_private_os_error();
            status = CLM_NET_IN_PROGRESS;
        }
    } else if (error != 0) {
        clm_net_private_last_error = error;
        #ifdef _WIN32
        status = (error == WSAEWOULDBLOCK || error == WSAEINPROGRESS ||
            error == WSAEALREADY) ? CLM_NET_IN_PROGRESS : CLM_NET_ERROR_SYSTEM;
        #else
        status = (error == EINPROGRESS || error == EALREADY) ?
            CLM_NET_IN_PROGRESS : CLM_NET_ERROR_SYSTEM;
        #endif
    }
    clm_net_private_release(socket);
    return status;
}

static inline size_t clm_net_private_chunk(size_t length)
{
    return length > (size_t)INT_MAX ? (size_t)INT_MAX : length;
}

/** Send bytes on a connected TCP/UDP socket. TCP may send fewer bytes than
 * requested; retry any remainder. The payload is borrowed for this call.
 * @param socket_id Connected socket ID.
 * @param bytes Payload; may be NULL only when length is zero.
 * @param length Number of bytes to send.
 * @param out_sent Receives the number sent on SUCCESS.
 * @return SUCCESS, WOULD_BLOCK, or error.
 */
static inline clm_net_status clm_net_send(
    clm_id socket_id,
    const void* bytes, 
    size_t length, 
    size_t* out_sent
)
{
    if (out_sent == NULL || (length != 0 && bytes == NULL)) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    int flags = 0;
    #ifdef MSG_NOSIGNAL
    flags |= MSG_NOSIGNAL;
    #endif
    #ifdef _WIN32
    int written = send(socket->fd, (const char*)bytes,
        (int)clm_net_private_chunk(length), flags);
    #else
    ssize_t written = send(socket->fd, bytes,
        clm_net_private_chunk(length), flags);
    #endif
    clm_net_status status = CLM_NET_SUCCESS;
    if (written < 0) {
        status = clm_net_private_system_error();
    } else {
        *out_sent = (size_t)written;
    }
    clm_net_private_release(socket);
    return status;
}

/** Send the UTF-8 bytes of a CLM string on a connected socket.
 * No NUL terminator or message framing is added; TCP can send partially.
 * @param socket_id Connected socket ID.
 * @param string_id ID of a CLM string borrowed for this call.
 * @param out_sent Receives bytes sent on SUCCESS.
 * @return Same statuses as clm_net_send(), or INVALID_ARGUMENT for bad string.
 */
static inline clm_net_status clm_net_send_string(
    clm_id socket_id,
    clm_id string_id, 
    size_t* out_sent
)
{
    const char* text;
    size_t byte_length;
    clm_net_status status = clm_net_private_string_view(string_id, &text,
        &byte_length);
    return status == CLM_NET_SUCCESS ?
        clm_net_send(socket_id, text, byte_length, out_sent) : status;
}

/** Receive up to capacity bytes from a connected TCP/UDP socket.
 * TCP returning zero means orderly close; a zero-byte UDP datagram is SUCCESS.
 * @param socket_id Connected socket ID.
 * @param buffer Caller-owned writable byte buffer.
 * @param capacity Positive buffer size.
 * @param out_received Receives bytes read on SUCCESS or zero on CLOSED.
 * @return SUCCESS, CLOSED, WOULD_BLOCK, or error.
 */
static inline clm_net_status clm_net_receive(
    clm_id socket_id,
    void* buffer, 
    size_t capacity, 
    size_t* out_received
)
{
    if (out_received == NULL || capacity == 0 || buffer == NULL) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    #ifdef _WIN32
    int received = recv(socket->fd, (char*)buffer,
        (int)clm_net_private_chunk(capacity), 0);
    #else
    ssize_t received = recv(socket->fd, buffer,
        clm_net_private_chunk(capacity), 0);
    #endif
    clm_net_status status = CLM_NET_SUCCESS;
    if (received < 0) {
        status = clm_net_private_system_error();
    } else if (received == 0 && socket->transport == CLM_NET_TCP) {
        /* A zero-length UDP datagram is data, but TCP zero means EOF. */
        status = CLM_NET_CLOSED;
        *out_received = 0;
    } else {
        *out_received = (size_t)received;
    }
    clm_net_private_release(socket);
    return status;
}

/** Send one UDP datagram to a numeric endpoint.
 * The complete datagram is sent or an error is returned.
 * @param socket_id UDP socket ID.
 * @param destination Numeric endpoint matching the socket family.
 * @param bytes Payload; NULL is allowed only for an empty datagram.
 * @param length Datagram payload size.
 * @param out_sent Receives datagram size on SUCCESS.
 * @return SUCCESS, WOULD_BLOCK, or error.
 */
static inline clm_net_status clm_net_send_to(
    clm_id socket_id,
    const clm_net_address* destination, 
    const void* bytes,
    size_t length, 
    size_t* out_sent
)
{
    if (out_sent == NULL || length > (size_t)INT_MAX ||
        (length != 0 && bytes == NULL)) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    struct sockaddr_storage native;
    clm_net_private_addrlen addr_length;
    clm_net_status status = clm_net_private_to_sockaddr(destination,
        &native, &addr_length);
    if (status != CLM_NET_SUCCESS) {
        return status;
    }
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    if (socket->transport != CLM_NET_UDP || socket->family != destination->family) {
        clm_net_private_release(socket);
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    const char empty_datagram = 0;
    if (length == 0 && bytes == NULL) {
        /* Some native APIs require a valid pointer even for a zero-byte send;
         * the pointed-to byte is not included in the datagram. */
        bytes = &empty_datagram;
    }
    #ifdef _WIN32
    int written = sendto(socket->fd, (const char*)bytes, (int)clm_net_private_chunk(length), 0, (struct sockaddr*)&native, addr_length);
    #else
    ssize_t written = sendto(socket->fd, bytes, clm_net_private_chunk(length), 0, (struct sockaddr*)&native, addr_length);
    #endif
    if (written < 0) {
        status = clm_net_private_system_error();
    } else if ((size_t)written != length) {
        status = CLM_NET_ERROR_SYSTEM;
    } else {
        *out_sent = (size_t)written;
    }
    clm_net_private_release(socket);
    return status;
}

/** Send a CLM string's UTF-8 bytes as one UDP datagram, without a NUL byte.
 * @param socket_id UDP socket ID.
 * @param destination Numeric recipient endpoint.
 * @param string_id ID of a CLM string borrowed for this call.
 * @param out_sent Receives bytes sent on SUCCESS.
 * @return Same statuses as clm_net_send_to(), or INVALID_ARGUMENT for bad string.
 */
static inline clm_net_status clm_net_send_to_string(
    clm_id socket_id,
    const clm_net_address* destination, 
    clm_id string_id, 
    size_t* out_sent
)
{
    const char* text;
    size_t byte_length;
    clm_net_status status = clm_net_private_string_view(string_id, &text,
        &byte_length);
    return status == CLM_NET_SUCCESS ?
        clm_net_send_to(socket_id, destination, text, byte_length, out_sent) :
        status;
}

/** Receive one UDP datagram, optionally returning its numeric source.
 * Datagrams larger than capacity may be truncated without a separate flag.
 * @param socket_id UDP socket ID.
 * @param buffer Caller-owned writable destination.
 * @param capacity Positive buffer size.
 * @param out_received Receives bytes received on SUCCESS.
 * @param out_source Optional caller-owned sender endpoint.
 * @return SUCCESS, WOULD_BLOCK, or error.
 */
static inline clm_net_status clm_net_receive_from(
    clm_id socket_id,
    void* buffer, 
    size_t capacity, 
    size_t* out_received,
    clm_net_address* out_source
)
{
    if (out_received == NULL || capacity == 0 || buffer == NULL) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    if (socket->transport != CLM_NET_UDP) {
        clm_net_private_release(socket);
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    struct sockaddr_storage native;
    clm_net_private_addrlen address_length = (clm_net_private_addrlen)sizeof(native);
    #ifdef _WIN32
    int received = recvfrom(socket->fd, (char*)buffer,
        (int)clm_net_private_chunk(capacity), 0, (struct sockaddr*)&native,
        &address_length);
    #else
    ssize_t received = recvfrom(socket->fd, buffer,
        clm_net_private_chunk(capacity), 0, (struct sockaddr*)&native,
        &address_length);
    #endif
    clm_net_status status = CLM_NET_SUCCESS;
    clm_net_address source;
    if (received < 0) {
        status = clm_net_private_system_error();
    } else if (out_source != NULL &&
        (status = clm_net_private_from_sockaddr((struct sockaddr*)&native,
            address_length, &source)) != CLM_NET_SUCCESS) {
        /* Preserve output arguments on unsupported source families. */
    } else {
        *out_received = (size_t)received;
        if (out_source != NULL) {
            *out_source = source;
        }
    }
    clm_net_private_release(socket);
    return status;
}

/** Half-close a connected TCP socket without releasing its registered ID.
 * @param socket_id Connected TCP socket ID.
 * @param mode READ, WRITE, or BOTH.
 * @return SUCCESS or error.
 */
static inline clm_net_status clm_net_socket_shutdown(
    clm_id socket_id,
    clm_net_shutdown_mode mode
)
{
    if (mode != CLM_NET_SHUTDOWN_READ && mode != CLM_NET_SHUTDOWN_WRITE &&
        mode != CLM_NET_SHUTDOWN_BOTH) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    if (socket->transport != CLM_NET_TCP) {
        clm_net_private_release(socket);
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    #ifdef _WIN32
    int how = mode == CLM_NET_SHUTDOWN_READ ? SD_RECEIVE :
        (mode == CLM_NET_SHUTDOWN_WRITE ? SD_SEND : SD_BOTH);
    #else
    int how = mode == CLM_NET_SHUTDOWN_READ ? SHUT_RD :
        (mode == CLM_NET_SHUTDOWN_WRITE ? SHUT_WR : SHUT_RDWR);
    #endif
    clm_net_status status = shutdown(socket->fd, how) == 0 ? CLM_NET_SUCCESS : clm_net_private_system_error();
    clm_net_private_release(socket);
    return status;
}

static inline clm_net_status clm_net_private_get_address(
    clm_id socket_id,
    bool peer, 
    clm_net_address* out_address
)
{
    if (out_address == NULL) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    struct sockaddr_storage native;
    clm_net_private_addrlen length = (clm_net_private_addrlen)sizeof(native);
    int result = peer ? getpeername(socket->fd, (struct sockaddr*)&native,
        &length) : getsockname(socket->fd, (struct sockaddr*)&native, &length);
    clm_net_status status = result == 0 ?
        clm_net_private_from_sockaddr((struct sockaddr*)&native, length,
            out_address) : clm_net_private_system_error();
    clm_net_private_release(socket);
    return status;
}

/** Read the socket's current local numeric endpoint.
 * @param socket_id Registered socket ID.
 * @param out_address Caller-owned address result.
 * @return SUCCESS or error.
 */
static inline clm_net_status clm_net_socket_local_address(
    clm_id socket_id,
    clm_net_address* out_address
)
{
    return clm_net_private_get_address(socket_id, false, out_address);
}

/** Read the connected peer's numeric endpoint.
 * @param socket_id Connected socket ID.
 * @param out_address Caller-owned address result.
 * @return SUCCESS or error; output is unchanged on failure.
 */
static inline clm_net_status clm_net_socket_peer_address(
    clm_id socket_id,
    clm_net_address* out_address
)
{
    return clm_net_private_get_address(socket_id, true, out_address);
}

/** Wait for readiness on unique socket IDs; performs no socket I/O.
 * Each entry requests READ/WRITE. TIMEOUT zeros all revents and out_ready;
 * other errors leave outputs unchanged. This call blocks the calling thread.
 * @param entries Mutable poll entries.
 * @param count Number of entries; must be nonzero.
 * @param timeout_ms Milliseconds or CLM_NET_WAIT_FOREVER.
 * @param out_ready Number of entries reporting any event.
 * @return SUCCESS, TIMEOUT, or error.
 */
static inline clm_net_status clm_net_poll(
    clm_net_poll_entry* entries,
    size_t count, 
    uint32_t timeout_ms, 
    size_t* out_ready
)
{
    if (out_ready == NULL || (count != 0 && entries == NULL) || count == 0 ||
        (timeout_ms != CLM_NET_WAIT_FOREVER && timeout_ms > INT_MAX) ||
        count > ((size_t)-1) / sizeof(clm_net_private_socket*) ||
        count > ((size_t)-1) / sizeof(clm_net_poll_entry)) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_private_socket** held = (clm_net_private_socket**)calloc(count,
        sizeof(*held));
    if (held == NULL) {
        return CLM_NET_ERROR_SYSTEM;
    }
    clm_net_status status = CLM_NET_SUCCESS;
    for (size_t index = 0; index < count; ++index) {
        if ((entries[index].events & (CLM_NET_EVENT_READ |
                CLM_NET_EVENT_WRITE)) == 0 ||
            (entries[index].events & ~(CLM_NET_EVENT_READ |
                CLM_NET_EVENT_WRITE)) != 0) {
            status = CLM_NET_ERROR_INVALID_ARGUMENT;
            break;
        }
        for (size_t earlier = 0; earlier < index; ++earlier) {
            if (entries[index].socket_id == entries[earlier].socket_id) {
                status = CLM_NET_ERROR_INVALID_ARGUMENT;
                break;
            }
        }
        if (status != CLM_NET_SUCCESS) {
            break;
        }
        held[index] = clm_net_private_acquire(entries[index].socket_id);
        if (held[index] == NULL) {
            status = CLM_NET_ERROR_NOT_FOUND;
            break;
        }
    }
    unsigned int* ready_flags = NULL;
    if (status == CLM_NET_SUCCESS) {
        ready_flags = (unsigned int*)calloc(count, sizeof(*ready_flags));
        if (ready_flags == NULL) {
            status = CLM_NET_ERROR_SYSTEM;
        }
    }
    /* Stage results so errors leave caller entries untouched. Held references
     * keep every native handle valid for the duration of the OS wait. */
    #ifdef _WIN32
    if (status == CLM_NET_SUCCESS) {
        if (count > FD_SETSIZE) {
            status = CLM_NET_ERROR_UNSUPPORTED;
        } else {
            fd_set read_set;
            fd_set write_set;
            fd_set exception_set;
            FD_ZERO(&read_set);
            FD_ZERO(&write_set);
            FD_ZERO(&exception_set);
            for (size_t index = 0; index < count; ++index) {
                SOCKET fd = held[index]->fd;
                if (entries[index].events & CLM_NET_EVENT_READ) {
                    FD_SET(fd, &read_set);
                }
                if (entries[index].events & CLM_NET_EVENT_WRITE) {
                    FD_SET(fd, &write_set);
                }
                FD_SET(fd, &exception_set);
            }
            struct timeval duration;
            struct timeval* timeout = NULL;
            if (timeout_ms != CLM_NET_WAIT_FOREVER) {
                duration.tv_sec = (long)(timeout_ms / 1000u);
                duration.tv_usec = (long)((timeout_ms % 1000u) * 1000u);
                timeout = &duration;
            }
            int result = select(0, &read_set, &write_set, &exception_set,
                timeout);
            if (result < 0) {
                status = clm_net_private_system_error();
            } else if (result == 0) {
                status = CLM_NET_TIMEOUT;
            } else {
                for (size_t index = 0; index < count; ++index) {
                    SOCKET fd = held[index]->fd;
                    if (FD_ISSET(fd, &read_set)) {
                        ready_flags[index] |= CLM_NET_EVENT_READ;
                    }
                    if (FD_ISSET(fd, &write_set)) {
                        ready_flags[index] |= CLM_NET_EVENT_WRITE;
                    }
                    if (FD_ISSET(fd, &exception_set)) {
                        ready_flags[index] |= CLM_NET_EVENT_ERROR;
                    }
                }
            }
        }
    }
    #else
    if (status == CLM_NET_SUCCESS) {
        if (count > SIZE_MAX / sizeof(struct pollfd) || count > (size_t)UINT_MAX) {
            status = CLM_NET_ERROR_INVALID_ARGUMENT;
        } else {
            struct pollfd* native = (struct pollfd*)calloc(count, sizeof(*native));
            if (native == NULL) {
                status = CLM_NET_ERROR_SYSTEM;
            } else {
                for (size_t index = 0; index < count; ++index) {
                    native[index].fd = held[index]->fd;
                    native[index].events = 0;
                    if (entries[index].events & CLM_NET_EVENT_READ) {
                        native[index].events |= POLLIN;
                    }
                    if (entries[index].events & CLM_NET_EVENT_WRITE) {
                        native[index].events |= POLLOUT;
                    }
                }
                int timeout = timeout_ms == CLM_NET_WAIT_FOREVER ? -1 :
                    (int)timeout_ms;
                int result = poll(native, (nfds_t)count, timeout);
                if (result < 0) {
                    status = clm_net_private_system_error();
                } else if (result == 0) {
                    status = CLM_NET_TIMEOUT;
                } else {
                    for (size_t index = 0; index < count; ++index) {
                        short observed = native[index].revents;
                        if (observed & (POLLIN | POLLPRI)) {
                            ready_flags[index] |= CLM_NET_EVENT_READ;
                        }
                        if (observed & POLLOUT) {
                            ready_flags[index] |= CLM_NET_EVENT_WRITE;
                        }
                        if (observed & POLLHUP) {
                            ready_flags[index] |= CLM_NET_EVENT_HANGUP;
                        }
                        if (observed & (POLLERR | POLLNVAL)) {
                            ready_flags[index] |= CLM_NET_EVENT_ERROR;
                        }
                    }
                }
                free(native);
            }
        }
    }
    #endif
    if (status == CLM_NET_SUCCESS || status == CLM_NET_TIMEOUT) {
        size_t ready = 0;
        for (size_t index = 0; index < count; ++index) {
            entries[index].revents = ready_flags[index];
            if (ready_flags[index] != 0) {
                ++ready;
            }
        }
        *out_ready = ready;
    }
    for (size_t index = 0; index < count; ++index) {
        clm_net_private_release(held[index]);
    }
    free(ready_flags);
    free(held);
    return status;
}

/** Wait for readiness on one socket; convenience wrapper around poll().
 * @param socket_id Registered socket ID.
 * @param events Requested READ/WRITE flags.
 * @param timeout_ms Milliseconds or CLM_NET_WAIT_FOREVER.
 * @param out_events Receives observed events on SUCCESS/TIMEOUT.
 * @return SUCCESS, TIMEOUT, or error.
 */
static inline clm_net_status clm_net_wait(
    clm_id socket_id,
    unsigned int events, 
    uint32_t timeout_ms, 
    unsigned int* out_events
)
{
    if (out_events == NULL) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_poll_entry entry = {socket_id, events, 0};
    size_t count = 0;
    clm_net_status status = clm_net_poll(&entry, 1, timeout_ms, &count);
    if (status == CLM_NET_SUCCESS || status == CLM_NET_TIMEOUT) {
        *out_events = entry.revents;
    }
    return status;
}
