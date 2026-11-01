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

#include "net.h"

once_flag clm_net_private_once = ONCE_FLAG_INIT;
mtx_t clm_net_private_mutex;
cnd_t clm_net_private_changed;
bool clm_net_private_lock_valid = false;
bool clm_net_private_initialized = false;
clm_net_private_socket* clm_net_private_sockets = NULL;
_Thread_local int clm_net_private_last_error = 0;

bool clm_net_private_lock(void) {
    call_once(&clm_net_private_once, clm_net_private_initialize_mutex);
    if (!clm_net_private_lock_valid) {
        return false;
    }
    return mtx_lock(&clm_net_private_mutex) == thrd_success;
}

void clm_net_private_unlock(void) {
    mtx_unlock(&clm_net_private_mutex);
}

int clm_net_private_os_error(void) {
    #ifdef _WIN32
    return WSAGetLastError();
    #else
    return errno;
    #endif
}

clm_net_status clm_net_private_system_error(void) {
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

bool clm_net_private_valid_family(clm_net_family family) {
    return family == CLM_NET_IPV4 || family == CLM_NET_IPV6;
}

bool clm_net_private_valid_transport(clm_net_transport transport) {
    return transport == CLM_NET_TCP || transport == CLM_NET_UDP;
}

int clm_net_private_af(clm_net_family family) {
    return family == CLM_NET_IPV4 ? AF_INET : AF_INET6;
}

int clm_net_private_socket_type(clm_net_transport transport) {
    return transport == CLM_NET_TCP ? SOCK_STREAM : SOCK_DGRAM;
}

void clm_net_private_close_native(clm_net_private_native_socket fd) {
    #ifdef _WIN32
    closesocket(fd);
    #else
    close(fd);
    #endif
}

clm_net_status clm_net_private_to_sockaddr(
    const clm_net_address* address, 
    struct sockaddr_storage* storage,
    clm_net_private_addrlen* length
) {
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

clm_net_status clm_net_private_from_sockaddr(
    const struct sockaddr* native, 
    clm_net_private_addrlen length,
    clm_net_address* out_address
) {
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

clm_net_private_socket* clm_net_private_acquire(clm_id id) {
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

void clm_net_private_release(clm_net_private_socket* socket) {
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

clm_net_status clm_net_private_register(
    clm_id id,
    clm_net_private_native_socket fd, 
    clm_net_family family,
    clm_net_transport transport) {
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

clm_net_status clm_net_initialize(void) {
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

clm_net_status clm_net_finalize(void) {
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

int clm_net_last_native_error(void) {
    return clm_net_private_last_error;
}

clm_net_status clm_net_private_string_view(
    clm_id string_id,
    const char** out_text, 
    size_t* out_bytes
) {
    clm_int character_count = clm_string_length(string_id);
    if (out_text == NULL || out_bytes == NULL || character_count < 0 ||
        clm_string_slice_borrow(string_id, 0, character_count, out_text,
            out_bytes) != CLM_BPA_SUCCESS) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    return CLM_NET_SUCCESS;
}

clm_net_status clm_net_address_parse(
    const char* ip_text,
    uint16_t port, 
    clm_net_address* out_address
) {
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

clm_net_status clm_net_address_parse_string(
    clm_id ip_string_id, 
    uint16_t port,
    clm_net_address* out_address
) {
    const char* ip_text;
    size_t ip_bytes;
    clm_net_status status = clm_net_private_string_view(ip_string_id,
        &ip_text, &ip_bytes);
    (void)ip_bytes;
    return status == CLM_NET_SUCCESS ?
        clm_net_address_parse(ip_text, port, out_address) : status;
}

clm_net_status clm_net_address_format(
    const clm_net_address* address, 
    char* out_text, size_t capacity
) {
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

clm_net_status clm_net_address_format_string(
    const clm_net_address* address, 
    clm_id out_string_id
) {
    char text[INET6_ADDRSTRLEN];
    clm_net_status status = clm_net_address_format(address, text,
        sizeof(text));
    if (status != CLM_NET_SUCCESS) {
        return status;
    }
    return clm_string_from_from(out_string_id, text) == CLM_BPA_SUCCESS ?
        CLM_NET_SUCCESS : CLM_NET_ERROR_SYSTEM;
}

clm_net_status clm_net_resolve(
    const char* hostname,
    uint16_t port, 
    clm_net_transport transport, 
    clm_net_address* addresses,
    size_t capacity, 
    size_t* out_count
) {
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

clm_net_status clm_net_resolve_string(
    clm_id hostname_string_id, 
    uint16_t port, 
    clm_net_transport transport,
    clm_net_address* addresses, 
    size_t capacity, 
    size_t* out_count
) {
    const char* hostname;
    size_t hostname_bytes;
    clm_net_status status = clm_net_private_string_view(hostname_string_id,
        &hostname, &hostname_bytes);
    (void)hostname_bytes;
    return status == CLM_NET_SUCCESS ?
        clm_net_resolve(hostname, port, transport, addresses, capacity,
            out_count) : status;
}

clm_net_status clm_net_socket_create(
    clm_id socket_id,
    clm_net_family family, 
    clm_net_transport transport
) {
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

clm_net_status clm_net_socket_close(clm_id socket_id) {
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

clm_net_status clm_net_private_set_option(
    clm_id socket_id,
    int level, int option, 
    bool enabled
) {
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

clm_net_status clm_net_socket_set_nonblocking(
    clm_id socket_id, 
    bool enabled
) {
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

clm_net_status clm_net_socket_set_reuse_address(
    clm_id socket_id, 
    bool enabled
) {
    return clm_net_private_set_option(socket_id, SOL_SOCKET, SO_REUSEADDR,enabled);
}

clm_net_status clm_net_socket_set_no_delay(
    clm_id socket_id, 
    bool enabled) {
    return clm_net_private_set_option(socket_id, IPPROTO_TCP, TCP_NODELAY,
        enabled);
}

clm_net_status clm_net_socket_set_ipv6_only(
    clm_id socket_id, 
    bool enabled) {
    #ifdef IPV6_V6ONLY
    return clm_net_private_set_option(socket_id, IPPROTO_IPV6, IPV6_V6ONLY,
        enabled);
    #else
    (void)socket_id;
    (void)enabled;
    return CLM_NET_ERROR_UNSUPPORTED;
    #endif
}

clm_net_status clm_net_bind(
    clm_id socket_id,
    const clm_net_address* local_address
) {
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

clm_net_status clm_net_listen(
    clm_id socket_id, 
    int backlog
) {
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

clm_net_status clm_net_accept(
    clm_id listener_id,
    clm_id new_socket_id, 
    clm_net_address* out_peer_address
) {
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

clm_net_status clm_net_connect(
    clm_id socket_id,
    const clm_net_address* remote_address
) {
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

clm_net_status clm_net_connect_finish(clm_id socket_id) {
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

size_t clm_net_private_chunk(size_t length) {
    return length > (size_t)INT_MAX ? (size_t)INT_MAX : length;
}

clm_net_status clm_net_send(
    clm_id socket_id,
    const void* bytes, 
    size_t length, 
    size_t* out_sent
) {
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

clm_net_status clm_net_send_string(
    clm_id socket_id,
    clm_id string_id, 
    size_t* out_sent
) {
    const char* text;
    size_t byte_length;
    clm_net_status status = clm_net_private_string_view(string_id, &text,
        &byte_length);
    return status == CLM_NET_SUCCESS ?
        clm_net_send(socket_id, text, byte_length, out_sent) : status;
}

clm_net_status clm_net_receive(
    clm_id socket_id,
    void* buffer, 
    size_t capacity, 
    size_t* out_received
) {
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

clm_net_status clm_net_send_to(
    clm_id socket_id,
    const clm_net_address* destination, 
    const void* bytes,
    size_t length, 
    size_t* out_sent
) {
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

clm_net_status clm_net_send_to_string(
    clm_id socket_id,
    const clm_net_address* destination, 
    clm_id string_id, 
    size_t* out_sent
) {
    const char* text;
    size_t byte_length;
    clm_net_status status = clm_net_private_string_view(string_id, &text,
        &byte_length);
    return status == CLM_NET_SUCCESS ?
        clm_net_send_to(socket_id, destination, text, byte_length, out_sent) :
        status;
}

clm_net_status clm_net_receive_from(
    clm_id socket_id,
    void* buffer, 
    size_t capacity, 
    size_t* out_received,
    clm_net_address* out_source
) {
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

clm_net_status clm_net_socket_shutdown(
    clm_id socket_id,
    clm_net_shutdown_mode mode
) {
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

clm_net_status clm_net_private_get_address(
    clm_id socket_id,
    bool peer, 
    clm_net_address* out_address
) {
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

clm_net_status clm_net_socket_local_address(
    clm_id socket_id,
    clm_net_address* out_address
) {
    return clm_net_private_get_address(socket_id, false, out_address);
}

clm_net_status clm_net_socket_peer_address(
    clm_id socket_id,
    clm_net_address* out_address
) {
    return clm_net_private_get_address(socket_id, true, out_address);
}

static int clm_net_private_compare_ids(const void* left, const void* right)
{
    clm_id a = *(const clm_id*)left;
    clm_id b = *(const clm_id*)right;
    return (a > b) - (a < b);
}

clm_net_status clm_net_poll(
    clm_net_poll_entry* entries,
    size_t count, 
    uint32_t timeout_ms, 
    size_t* out_ready
) {
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
        if (status != CLM_NET_SUCCESS) {
            break;
        }
        held[index] = clm_net_private_acquire(entries[index].socket_id);
        if (held[index] == NULL) {
            status = CLM_NET_ERROR_NOT_FOUND;
            break;
        }
    }
    if (status == CLM_NET_SUCCESS && count > 1) {
        /* O(n log n) duplicate detection on a sorted copy of the socket ids. */
        clm_id* ids = (clm_id*)malloc(count * sizeof(*ids));
        if (ids == NULL) {
            status = CLM_NET_ERROR_SYSTEM;
        } else {
            for (size_t index = 0; index < count; ++index) {
                ids[index] = entries[index].socket_id;
            }
            qsort(ids, count, sizeof(*ids), clm_net_private_compare_ids);
            for (size_t index = 1; index < count; ++index) {
                if (ids[index] == ids[index - 1]) {
                    status = CLM_NET_ERROR_INVALID_ARGUMENT;
                    break;
                }
            }
            free(ids);
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

clm_net_status clm_net_wait(
    clm_id socket_id,
    unsigned int events, 
    uint32_t timeout_ms, 
    unsigned int* out_events
) {
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
