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
 *
 * Persistent readiness poller for clm_net sockets (see net.h).
 *
 * Layout: every backend shares one registration table (socket ID -> record), so
 * set/remove/lookup behave identically everywhere. Only the "tell the kernel"
 * hooks differ: epoll and kqueue keep the interest set in the kernel; the
 * generic backend rebuilds a pollfd array from the table at each wait.
 */

#include "net.h"

#if defined(_WIN32)
#define CLM_NET_POLLER_GENERIC 1
#elif defined(__linux__)
#include <sys/epoll.h>
#define CLM_NET_POLLER_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__DragonFly__)
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#define CLM_NET_POLLER_KQUEUE 1
#else
#define CLM_NET_POLLER_GENERIC 1
#endif

#define CLM_NET_POLLER_INITIAL_CAPACITY 16u
#define CLM_NET_POLLER_MAX_NATIVE_BATCH 1024u
#define CLM_NET_POLLER_INTEREST (CLM_NET_EVENT_READ | CLM_NET_EVENT_WRITE)

/* Records are individually allocated so their addresses stay stable while the
 * table rehashes; epoll/kqueue hand that address back to us with each event. */
typedef struct clm_net_poller_registration {
    clm_id socket_id;
    clm_net_private_native_socket fd;
    unsigned int events;
    void* user_data;
    uint64_t stamp;     /* Wait serial that last emitted this record. */
    size_t out_index;   /* Output slot used by that wait, for merging. */
} clm_net_poller_registration;

struct clm_net_poller {
    /* Open-addressing hash table, linear probing, load factor <= 1/2, so a
     * probe always reaches an empty slot. Capacity is a power of two. */
    clm_net_poller_registration** table;
    size_t capacity;
    size_t count;
    uint64_t serial;
    #if defined(CLM_NET_POLLER_EPOLL)
    int handle;
    struct epoll_event* native;
    size_t native_capacity;
    #elif defined(CLM_NET_POLLER_KQUEUE)
    int handle;
    struct kevent* native;
    size_t native_capacity;
    #else
    #ifdef _WIN32
    WSAPOLLFD* native;
    #else
    struct pollfd* native;
    #endif
    clm_net_poller_registration** native_regs;
    size_t native_capacity;
    size_t cursor; /* Rotating scan start so a small output array stays fair. */
    #endif
};

/* ---------- registration table ---------- */

static size_t clm_net_poller_hash(clm_id id)
{
    uint64_t value = id;
    value ^= value >> 33;
    value *= 0xff51afd7ed558ccdULL;
    value ^= value >> 33;
    value *= 0xc4ceb9fe1a85ec53ULL;
    value ^= value >> 33;
    return (size_t)value;
}

/* Returns the record, or NULL. *out_slot is the match, or the empty slot where
 * the ID would be inserted. */
static clm_net_poller_registration* clm_net_poller_lookup(
    const clm_net_poller* poller, clm_id id, size_t* out_slot)
{
    size_t mask = poller->capacity - 1;
    size_t slot = clm_net_poller_hash(id) & mask;
    while (poller->table[slot] != NULL) {
        if (poller->table[slot]->socket_id == id) {
            *out_slot = slot;
            return poller->table[slot];
        }
        slot = (slot + 1) & mask;
    }
    *out_slot = slot;
    return NULL;
}

static bool clm_net_poller_grow(clm_net_poller* poller)
{
    size_t capacity = poller->capacity * 2;
    if (capacity < poller->capacity ||
        capacity > ((size_t)-1) / sizeof(*poller->table)) {
        return false;
    }
    clm_net_poller_registration** table =
        (clm_net_poller_registration**)calloc(capacity, sizeof(*table));
    if (table == NULL) {
        return false;
    }
    for (size_t index = 0; index < poller->capacity; ++index) {
        clm_net_poller_registration* record = poller->table[index];
        if (record == NULL) {
            continue;
        }
        size_t slot = clm_net_poller_hash(record->socket_id) & (capacity - 1);
        while (table[slot] != NULL) {
            slot = (slot + 1) & (capacity - 1);
        }
        table[slot] = record;
    }
    free(poller->table);
    poller->table = table;
    poller->capacity = capacity;
    return true;
}

/* Backward-shift deletion keeps probe chains intact without tombstones. */
static void clm_net_poller_erase_slot(clm_net_poller* poller, size_t hole)
{
    size_t mask = poller->capacity - 1;
    size_t scan = (hole + 1) & mask;
    while (poller->table[scan] != NULL) {
        size_t home = clm_net_poller_hash(poller->table[scan]->socket_id) & mask;
        /* Leave the record if its home slot lies cyclically in (hole, scan]:
         * moving it to the hole would put it before its home. */
        bool stays = hole <= scan ? (hole < home && home <= scan)
                                  : (hole < home || home <= scan);
        if (!stays) {
            poller->table[hole] = poller->table[scan];
            hole = scan;
        }
        scan = (scan + 1) & mask;
    }
    poller->table[hole] = NULL;
    --poller->count;
}

/* ---------- backend hooks ---------- */

#if defined(CLM_NET_POLLER_EPOLL)

static uint32_t clm_net_poller_epoll_mask(unsigned int events)
{
    uint32_t mask = 0;
    if (events & CLM_NET_EVENT_READ) {
        mask |= EPOLLIN;
    }
    if (events & CLM_NET_EVENT_WRITE) {
        mask |= EPOLLOUT;
    }
    return mask; /* HUP and ERR are always reported by the kernel. */
}

static clm_net_status clm_net_poller_backend_create(clm_net_poller* poller)
{
    poller->handle = epoll_create1(EPOLL_CLOEXEC);
    return poller->handle < 0 ? clm_net_private_system_error()
                              : CLM_NET_SUCCESS;
}

static void clm_net_poller_backend_destroy(clm_net_poller* poller)
{
    if (poller->handle >= 0) {
        close(poller->handle);
    }
    free(poller->native);
}

static clm_net_status clm_net_poller_backend_register(
    clm_net_poller* poller, clm_net_poller_registration* record)
{
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = clm_net_poller_epoll_mask(record->events);
    event.data.ptr = record;
    if (epoll_ctl(poller->handle, EPOLL_CTL_ADD, record->fd, &event) == 0) {
        return CLM_NET_SUCCESS;
    }
    /* A leftover registration for this descriptor number is simply replaced. */
    if (errno == EEXIST &&
        epoll_ctl(poller->handle, EPOLL_CTL_MOD, record->fd, &event) == 0) {
        return CLM_NET_SUCCESS;
    }
    return clm_net_private_system_error();
}

static clm_net_status clm_net_poller_backend_modify(
    clm_net_poller* poller, clm_net_poller_registration* record,
    unsigned int events)
{
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = clm_net_poller_epoll_mask(events);
    event.data.ptr = record;
    if (epoll_ctl(poller->handle, EPOLL_CTL_MOD, record->fd, &event) == 0) {
        return CLM_NET_SUCCESS;
    }
    /* ENOENT: the descriptor was closed and its number reused by a new socket
     * with the same ID; the kernel dropped the old registration. */
    if (errno == ENOENT &&
        epoll_ctl(poller->handle, EPOLL_CTL_ADD, record->fd, &event) == 0) {
        return CLM_NET_SUCCESS;
    }
    return clm_net_private_system_error();
}

/* Best effort: closing the descriptor already removed it from the kernel set. */
static void clm_net_poller_backend_unregister(
    clm_net_poller* poller, clm_net_poller_registration* record)
{
    (void)epoll_ctl(poller->handle, EPOLL_CTL_DEL, record->fd, NULL);
}

#elif defined(CLM_NET_POLLER_KQUEUE)

/* kqueue tracks READ and WRITE as separate filters, so interest changes are
 * applied one filter at a time; deleting an absent filter is not an error. */
static clm_net_status clm_net_poller_kqueue_change(
    clm_net_poller* poller, clm_net_poller_registration* record,
    short filter, unsigned short flags)
{
    struct kevent change;
    EV_SET(&change, (uintptr_t)record->fd, filter, flags, 0, 0, record);
    if (kevent(poller->handle, &change, 1, NULL, 0, NULL) == 0) {
        return CLM_NET_SUCCESS;
    }
    if ((flags & EV_DELETE) != 0 && (errno == ENOENT || errno == EBADF)) {
        return CLM_NET_SUCCESS;
    }
    return clm_net_private_system_error();
}

static clm_net_status clm_net_poller_kqueue_apply(
    clm_net_poller* poller, clm_net_poller_registration* record,
    unsigned int from, unsigned int to)
{
    static const struct { unsigned int flag; short filter; } filters[2] = {
        {CLM_NET_EVENT_READ, EVFILT_READ},
        {CLM_NET_EVENT_WRITE, EVFILT_WRITE}
    };
    for (size_t index = 0; index < 2; ++index) {
        bool had = (from & filters[index].flag) != 0;
        bool want = (to & filters[index].flag) != 0;
        if (had == want) {
            continue;
        }
        clm_net_status status = clm_net_poller_kqueue_change(poller, record,
            filters[index].filter, want ? (EV_ADD | EV_ENABLE) : EV_DELETE);
        if (status != CLM_NET_SUCCESS) {
            return status;
        }
    }
    return CLM_NET_SUCCESS;
}

static clm_net_status clm_net_poller_backend_create(clm_net_poller* poller)
{
    poller->handle = kqueue();
    if (poller->handle < 0) {
        return clm_net_private_system_error();
    }
    (void)fcntl(poller->handle, F_SETFD, FD_CLOEXEC);
    return CLM_NET_SUCCESS;
}

static void clm_net_poller_backend_destroy(clm_net_poller* poller)
{
    if (poller->handle >= 0) {
        close(poller->handle);
    }
    free(poller->native);
}

static clm_net_status clm_net_poller_backend_register(
    clm_net_poller* poller, clm_net_poller_registration* record)
{
    clm_net_status status = clm_net_poller_kqueue_apply(poller, record, 0,
        record->events);
    if (status != CLM_NET_SUCCESS) {
        (void)clm_net_poller_kqueue_apply(poller, record, record->events, 0);
    }
    return status;
}

static clm_net_status clm_net_poller_backend_modify(
    clm_net_poller* poller, clm_net_poller_registration* record,
    unsigned int events)
{
    /* EV_ADD is idempotent, so re-adding every wanted filter also repairs a
     * registration the kernel dropped when a descriptor number was reused. */
    clm_net_status status = clm_net_poller_kqueue_apply(poller, record, 0,
        events);
    if (status == CLM_NET_SUCCESS && (record->events & ~events) != 0) {
        status = clm_net_poller_kqueue_apply(poller, record,
            record->events & ~events, 0);
    }
    return status;
}

static void clm_net_poller_backend_unregister(
    clm_net_poller* poller, clm_net_poller_registration* record)
{
    (void)clm_net_poller_kqueue_apply(poller, record, record->events, 0);
}

#else /* generic poll()/WSAPoll(): nothing lives in the kernel between waits */

static clm_net_status clm_net_poller_backend_create(clm_net_poller* poller)
{
    (void)poller;
    return CLM_NET_SUCCESS;
}

static void clm_net_poller_backend_destroy(clm_net_poller* poller)
{
    free(poller->native);
    free(poller->native_regs);
}

static clm_net_status clm_net_poller_backend_register(
    clm_net_poller* poller, clm_net_poller_registration* record)
{
    (void)poller;
    (void)record;
    return CLM_NET_SUCCESS;
}

static clm_net_status clm_net_poller_backend_modify(
    clm_net_poller* poller, clm_net_poller_registration* record,
    unsigned int events)
{
    (void)poller;
    (void)record;
    (void)events;
    return CLM_NET_SUCCESS;
}

static void clm_net_poller_backend_unregister(
    clm_net_poller* poller, clm_net_poller_registration* record)
{
    (void)poller;
    (void)record;
}

#endif

/* ---------- public API ---------- */

/* Drops the kernel registration only while the ID still names the same native
 * handle. Once a socket is closed the kernel has already forgotten it, and its
 * descriptor number may now belong to a different socket whose registration an
 * unconditional delete would silently destroy. */
static void clm_net_poller_release_native(
    clm_net_poller* poller, clm_net_poller_registration* record)
{
    clm_net_private_socket* socket = clm_net_private_acquire(record->socket_id);
    if (socket == NULL) {
        return;
    }
    if (socket->fd == record->fd) {
        clm_net_poller_backend_unregister(poller, record);
    }
    clm_net_private_release(socket);
}

const char* clm_net_poller_backend(void)
{
    #if defined(CLM_NET_POLLER_EPOLL)
    return "epoll";
    #elif defined(CLM_NET_POLLER_KQUEUE)
    return "kqueue";
    #else
    return "poll";
    #endif
}

clm_net_status clm_net_poller_create(clm_net_poller** out_poller)
{
    if (out_poller == NULL) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    clm_net_poller* poller = (clm_net_poller*)calloc(1, sizeof(*poller));
    if (poller == NULL) {
        return CLM_NET_ERROR_SYSTEM;
    }
    poller->capacity = CLM_NET_POLLER_INITIAL_CAPACITY;
    poller->table = (clm_net_poller_registration**)calloc(poller->capacity,
        sizeof(*poller->table));
    if (poller->table == NULL) {
        free(poller);
        return CLM_NET_ERROR_SYSTEM;
    }
    clm_net_status status = clm_net_poller_backend_create(poller);
    if (status != CLM_NET_SUCCESS) {
        free(poller->table);
        free(poller);
        return status;
    }
    *out_poller = poller;
    return CLM_NET_SUCCESS;
}

void clm_net_poller_destroy(clm_net_poller* poller)
{
    if (poller == NULL) {
        return;
    }
    clm_net_poller_backend_destroy(poller);
    for (size_t index = 0; index < poller->capacity; ++index) {
        free(poller->table[index]);
    }
    free(poller->table);
    free(poller);
}

size_t clm_net_poller_count(const clm_net_poller* poller)
{
    return poller == NULL ? 0 : poller->count;
}

clm_net_status clm_net_poller_set(
    clm_net_poller* poller,
    clm_id socket_id,
    unsigned int events,
    void* user_data)
{
    if (poller == NULL || (events & CLM_NET_POLLER_INTEREST) == 0 ||
        (events & ~CLM_NET_POLLER_INTEREST) != 0) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    /* Borrow the native handle only to learn it; the poller deliberately does
     * not pin the socket, so closing stays possible while registered. */
    clm_net_private_socket* socket = clm_net_private_acquire(socket_id);
    if (socket == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    clm_net_private_native_socket fd = socket->fd;
    clm_net_private_release(socket);

    size_t slot = 0;
    clm_net_poller_registration* record =
        clm_net_poller_lookup(poller, socket_id, &slot);
    if (record == NULL) {
        if ((poller->count + 1) * 2 > poller->capacity) {
            if (!clm_net_poller_grow(poller)) {
                return CLM_NET_ERROR_SYSTEM;
            }
            (void)clm_net_poller_lookup(poller, socket_id, &slot);
        }
        record = (clm_net_poller_registration*)calloc(1, sizeof(*record));
        if (record == NULL) {
            return CLM_NET_ERROR_SYSTEM;
        }
        record->socket_id = socket_id;
        record->fd = fd;
        record->events = events;
        record->user_data = user_data;
        clm_net_status status = clm_net_poller_backend_register(poller, record);
        if (status != CLM_NET_SUCCESS) {
            free(record);
            return status;
        }
        poller->table[slot] = record;
        ++poller->count;
        return CLM_NET_SUCCESS;
    }
    if (record->fd != fd) {
        /* The ID now names a different native socket (closed and recreated).
         * Drop the stale kernel registration and register the new handle. */
        clm_net_poller_release_native(poller, record);
        record->fd = fd;
        record->events = events;
        clm_net_status status = clm_net_poller_backend_register(poller, record);
        if (status != CLM_NET_SUCCESS) {
            clm_net_poller_erase_slot(poller, slot);
            free(record);
            return status;
        }
    } else {
        /* Refreshed even when the interest is unchanged; see backend_modify. */
        clm_net_status status = clm_net_poller_backend_modify(poller, record,
            events);
        if (status != CLM_NET_SUCCESS) {
            return status;
        }
        record->events = events;
    }
    record->user_data = user_data;
    return CLM_NET_SUCCESS;
}

clm_net_status clm_net_poller_remove(clm_net_poller* poller, clm_id socket_id)
{
    if (poller == NULL) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    size_t slot = 0;
    clm_net_poller_registration* record =
        clm_net_poller_lookup(poller, socket_id, &slot);
    if (record == NULL) {
        return CLM_NET_ERROR_NOT_FOUND;
    }
    clm_net_poller_release_native(poller, record);
    clm_net_poller_erase_slot(poller, slot);
    free(record);
    return CLM_NET_SUCCESS;
}

/* Adds one readiness report. A socket can be reported twice in one native batch
 * (kqueue read and write filters), so the second report merges into the first.
 * Excess records beyond the caller's capacity are dropped; level-triggered
 * semantics make the next wait report them again. */
static void clm_net_poller_emit(
    clm_net_poller* poller,
    clm_net_poller_registration* record,
    unsigned int revents,
    clm_net_poller_event* out_events,
    size_t capacity,
    size_t* produced)
{
    if (revents == 0) {
        return;
    }
    if (record->stamp == poller->serial) {
        out_events[record->out_index].revents |= revents;
        return;
    }
    if (*produced >= capacity) {
        return;
    }
    record->stamp = poller->serial;
    record->out_index = *produced;
    out_events[*produced].socket_id = record->socket_id;
    out_events[*produced].user_data = record->user_data;
    out_events[*produced].revents = revents;
    ++*produced;
}

#if defined(CLM_NET_POLLER_EPOLL)

static clm_net_status clm_net_poller_backend_wait(
    clm_net_poller* poller, clm_net_poller_event* out_events, size_t capacity,
    uint32_t timeout_ms, size_t* produced)
{
    size_t wanted = capacity < CLM_NET_POLLER_MAX_NATIVE_BATCH ? capacity
        : CLM_NET_POLLER_MAX_NATIVE_BATCH;
    if (wanted > poller->native_capacity) {
        struct epoll_event* grown = (struct epoll_event*)realloc(poller->native,
            wanted * sizeof(*grown));
        if (grown == NULL) {
            return CLM_NET_ERROR_SYSTEM;
        }
        poller->native = grown;
        poller->native_capacity = wanted;
    }
    int ready = epoll_wait(poller->handle, poller->native, (int)wanted,
        timeout_ms == CLM_NET_WAIT_FOREVER ? -1 : (int)timeout_ms);
    if (ready < 0) {
        /* A signal is a spurious wake-up, not a failure; callers loop. */
        return errno == EINTR ? CLM_NET_TIMEOUT : clm_net_private_system_error();
    }
    for (int index = 0; index < ready; ++index) {
        uint32_t observed = poller->native[index].events;
        unsigned int revents = 0;
        if (observed & (EPOLLIN | EPOLLPRI)) {
            revents |= CLM_NET_EVENT_READ;
        }
        if (observed & EPOLLOUT) {
            revents |= CLM_NET_EVENT_WRITE;
        }
        if (observed & EPOLLHUP) {
            revents |= CLM_NET_EVENT_HANGUP;
        }
        if (observed & EPOLLERR) {
            revents |= CLM_NET_EVENT_ERROR;
        }
        clm_net_poller_emit(poller,
            (clm_net_poller_registration*)poller->native[index].data.ptr,
            revents, out_events, capacity, produced);
    }
    return ready == 0 ? CLM_NET_TIMEOUT : CLM_NET_SUCCESS;
}

#elif defined(CLM_NET_POLLER_KQUEUE)

static clm_net_status clm_net_poller_backend_wait(
    clm_net_poller* poller, clm_net_poller_event* out_events, size_t capacity,
    uint32_t timeout_ms, size_t* produced)
{
    /* Up to two native events (read + write) per ready socket. */
    size_t wanted = capacity < CLM_NET_POLLER_MAX_NATIVE_BATCH ? capacity * 2
        : CLM_NET_POLLER_MAX_NATIVE_BATCH;
    if (wanted > CLM_NET_POLLER_MAX_NATIVE_BATCH) {
        wanted = CLM_NET_POLLER_MAX_NATIVE_BATCH;
    }
    if (wanted > poller->native_capacity) {
        struct kevent* grown = (struct kevent*)realloc(poller->native,
            wanted * sizeof(*grown));
        if (grown == NULL) {
            return CLM_NET_ERROR_SYSTEM;
        }
        poller->native = grown;
        poller->native_capacity = wanted;
    }
    struct timespec duration;
    struct timespec* timeout = NULL;
    if (timeout_ms != CLM_NET_WAIT_FOREVER) {
        duration.tv_sec = (time_t)(timeout_ms / 1000u);
        duration.tv_nsec = (long)(timeout_ms % 1000u) * 1000000L;
        timeout = &duration;
    }
    int ready = kevent(poller->handle, NULL, 0, poller->native, (int)wanted,
        timeout);
    if (ready < 0) {
        return errno == EINTR ? CLM_NET_TIMEOUT : clm_net_private_system_error();
    }
    for (int index = 0; index < ready; ++index) {
        const struct kevent* event = &poller->native[index];
        unsigned int revents = 0;
        if (event->filter == EVFILT_READ) {
            revents |= CLM_NET_EVENT_READ;
        } else if (event->filter == EVFILT_WRITE) {
            revents |= CLM_NET_EVENT_WRITE;
        }
        if (event->flags & EV_EOF) {
            revents |= CLM_NET_EVENT_HANGUP;
        }
        if (event->flags & EV_ERROR) {
            revents |= CLM_NET_EVENT_ERROR;
        }
        clm_net_poller_emit(poller,
            (clm_net_poller_registration*)event->udata, revents, out_events,
            capacity, produced);
    }
    return ready == 0 ? CLM_NET_TIMEOUT : CLM_NET_SUCCESS;
}

#else

static clm_net_status clm_net_poller_backend_wait(
    clm_net_poller* poller, clm_net_poller_event* out_events, size_t capacity,
    uint32_t timeout_ms, size_t* produced)
{
    if (poller->count > poller->native_capacity) {
        size_t wanted = poller->count;
        #ifdef _WIN32
        WSAPOLLFD* native = (WSAPOLLFD*)realloc(poller->native,
            wanted * sizeof(*native));
        #else
        struct pollfd* native = (struct pollfd*)realloc(poller->native,
            wanted * sizeof(*native));
        #endif
        if (native == NULL) {
            return CLM_NET_ERROR_SYSTEM;
        }
        poller->native = native;
        clm_net_poller_registration** regs =
            (clm_net_poller_registration**)realloc(poller->native_regs,
                wanted * sizeof(*regs));
        if (regs == NULL) {
            return CLM_NET_ERROR_SYSTEM;
        }
        poller->native_regs = regs;
        poller->native_capacity = wanted;
    }
    size_t total = 0;
    for (size_t index = 0; index < poller->capacity; ++index) {
        clm_net_poller_registration* record = poller->table[index];
        if (record == NULL) {
            continue;
        }
        poller->native[total].fd = record->fd;
        poller->native[total].events = (short)(
            ((record->events & CLM_NET_EVENT_READ) ? POLLIN : 0) |
            ((record->events & CLM_NET_EVENT_WRITE) ? POLLOUT : 0));
        poller->native[total].revents = 0;
        poller->native_regs[total] = record;
        ++total;
    }
    int timeout = timeout_ms == CLM_NET_WAIT_FOREVER ? -1 : (int)timeout_ms;
    #ifdef _WIN32
    int ready = WSAPoll(poller->native, (ULONG)total, timeout);
    #else
    int ready = poll(poller->native, (nfds_t)total, timeout);
    #endif
    if (ready < 0) {
        #ifndef _WIN32
        if (errno == EINTR) {
            return CLM_NET_TIMEOUT;
        }
        #endif
        return clm_net_private_system_error();
    }
    if (ready == 0) {
        return CLM_NET_TIMEOUT;
    }
    size_t start = poller->cursor % total;
    for (size_t step = 0; step < total; ++step) {
        size_t index = (start + step) % total;
        short observed = poller->native[index].revents;
        if (observed == 0) {
            continue;
        }
        unsigned int revents = 0;
        if (observed & (POLLIN | POLLPRI)) {
            revents |= CLM_NET_EVENT_READ;
        }
        if (observed & POLLOUT) {
            revents |= CLM_NET_EVENT_WRITE;
        }
        if (observed & POLLHUP) {
            revents |= CLM_NET_EVENT_HANGUP;
        }
        if (observed & (POLLERR | POLLNVAL)) {
            revents |= CLM_NET_EVENT_ERROR;
        }
        clm_net_poller_emit(poller, poller->native_regs[index], revents,
            out_events, capacity, produced);
        if (*produced >= capacity) {
            poller->cursor = index + 1;
            break;
        }
    }
    return CLM_NET_SUCCESS;
}

#endif

clm_net_status clm_net_poller_wait(
    clm_net_poller* poller,
    clm_net_poller_event* out_events,
    size_t capacity,
    uint32_t timeout_ms,
    size_t* out_count)
{
    if (poller == NULL || out_events == NULL || capacity == 0 ||
        out_count == NULL ||
        (timeout_ms != CLM_NET_WAIT_FOREVER && timeout_ms > INT_MAX) ||
        (poller->count == 0 && timeout_ms == CLM_NET_WAIT_FOREVER)) {
        return CLM_NET_ERROR_INVALID_ARGUMENT;
    }
    if (poller->count == 0) {
        /* Native waits reject or mishandle an empty set; just sleep. */
        struct timespec duration = {(time_t)(timeout_ms / 1000u),
            (long)(timeout_ms % 1000u) * 1000000L};
        thrd_sleep(&duration, NULL);
        *out_count = 0;
        return CLM_NET_TIMEOUT;
    }
    ++poller->serial;
    size_t produced = 0;
    clm_net_status status = clm_net_poller_backend_wait(poller, out_events,
        capacity, timeout_ms, &produced);
    if (status == CLM_NET_SUCCESS && produced == 0) {
        status = CLM_NET_TIMEOUT; /* Only stale events were harvested. */
    }
    if (status == CLM_NET_SUCCESS || status == CLM_NET_TIMEOUT) {
        *out_count = produced;
    }
    return status;
}
