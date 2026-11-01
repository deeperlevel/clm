#pragma once

/*
 * =============================================================================
 * CLM â€” Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain â€” CC0 1.0 Universal
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

#include "base.hpp"

#include <stdbool.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>

/* --------------------------------------------------------------------------
 * clm_channel â€” bounded, multi-producer/multi-consumer message channels.
 *
 * This module is independent of clm_async, clm_async_pool, and clm_container.
 * Operations are linearized under a per-channel mutex. C11 condition
 * variables provide blocking and wakeups; no scheduler integration is implied.
 *
 * Implemented channel behavior:
 *
 * - Each channel has a caller-supplied clm_id and a fixed capacity >= 0.
 *   Positive capacity buffers FIFO values; zero capacity is an unbuffered
 *   rendezvous channel where send and receive must pair directly.
 * - The channel stores clm_id VALUES (resource references), NOT copies of
 *   their BPA payloads. Messages are FIFO; each successful receive consumes
 *   exactly one message. A message is delivered to one receiver, not broadcast.
 * - Multiple sender and receiver threads may operate concurrently; individual
 *   operations are linearizable with respect to the channel's FIFO order.
 *   Competing blocked senders and receivers are paired FIFO where possible.
 * - The channel does not assume ownership of referenced BPA resources. The
 *   application must keep a referenced resource alive until the receiver has
 *   finished using it, or enforce its own transfer/copy ownership protocol.
 *   Sending an ID does not make its referenced resource thread-safe.
 * - close(): permanently rejects new sends and wakes blocked operations.
 *   Buffered values remain available to receivers. After the buffer drains,
 *   receive returns CLOSED with out_id=0 and out_open=false, mirroring Go's
 *   zero-value/ok=false receive. A successful receive sets out_open=true.
 *   Repeated close returns CLOSED (Go would panic; C reports status).
 * - destroy(): releases the channel and discards any remaining queued IDs,
 *   but NEVER deletes the referenced BPA resources. The caller MUST first
 *   stop all concurrent channel users and waiters; destroy is not safe while
 *   another thread may enter or be blocked in a channel operation. Normally
 *   close, let participants exit, then destroy. No hidden forced thread-kill.
 * - send()/receive() block indefinitely, matching Go's default channel
 *   operations. _timeout variants use C11 TIME_UTC absolute deadlines; wall
 *   clock adjustments may affect timeout duration. Zero timeout is nonblocking.
 * - Blocking send/receive must NOT be used inside clm_async step callbacks:
 *   they would block the entire cooperative worker. Use try operations and
 *   a future race-safe channel-to-task notification adapter instead.
 * - Receive outputs are written on SUCCESS and CLOSED: CLOSED supplies the
 *   zero ID and out_open=false. Other failures, EMPTY/FULL, and TIMEOUT leave
 *   outputs unchanged. get_info() is a snapshot, not an admission guarantee.
 * -------------------------------------------------------------------------- */

#define CLM_CHANNEL_OPERATION_SUCCESS  0
#define CLM_CHANNEL_OPERATION_FAILURE -1
#define CLM_CHANNEL_OPERATION_EMPTY   -2
#define CLM_CHANNEL_OPERATION_FULL    -3
#define CLM_CHANNEL_OPERATION_CLOSED  -4
#define CLM_CHANNEL_OPERATION_TIMEOUT -5

/** Sentinel for a blocking send/receive with no timeout. */
#define CLM_CHANNEL_WAIT_FOREVER UINT64_MAX

/** Snapshot of a channel. count is the buffered message count and may change
 * immediately after this call. Unbuffered handoffs are not included. closed
 * does not imply count==0: closed buffered channels may still have data.
 */
typedef struct clm_channel_info {
    size_t capacity;
    size_t count;
    bool closed;
} clm_channel_info;

typedef struct clm_channel_internal_waiter {
    clm_id message_id;
    clm_id* out_id;
    bool* out_open;
    bool matched;
    bool closed;
    struct clm_channel_internal_waiter* next;
} clm_channel_internal_waiter;

typedef struct clm_channel_internal {
    clm_id id;
    size_t capacity;
    size_t count;
    size_t head;
    size_t tail;
    bool closed;
    clm_id* buffer;
    mtx_t mutex;
    cnd_t changed;
    clm_channel_internal_waiter* send_head;
    clm_channel_internal_waiter* send_tail;
    clm_channel_internal_waiter* receive_head;
    clm_channel_internal_waiter* receive_tail;
    struct clm_channel_internal* next;
} clm_channel_internal;

extern clm_channel_internal* clm_channel_internal_channels;
extern atomic_flag clm_channel_internal_registry_lock;

#ifdef CLM_CHANNEL_IMPLEMENTATION
clm_channel_internal* clm_channel_internal_channels = NULL;
atomic_flag clm_channel_internal_registry_lock = ATOMIC_FLAG_INIT;
#endif

static inline void clm_channel_internal_lock_registry(void)
{
    while (atomic_flag_test_and_set_explicit(&clm_channel_internal_registry_lock,
        memory_order_acquire)) { }
}

static inline void clm_channel_internal_unlock_registry(void)
{
    atomic_flag_clear_explicit(&clm_channel_internal_registry_lock,
        memory_order_release);
}

static inline clm_channel_internal* clm_channel_internal_find(
    const clm_id channel_id)
{
    clm_channel_internal* found = NULL;
    clm_channel_internal_lock_registry();
    for (clm_channel_internal* channel = clm_channel_internal_channels;
        channel != NULL; channel = channel->next)
        if (channel->id == channel_id) {
            found = channel;
            break;
        }
    clm_channel_internal_unlock_registry();
    return found;
}

static inline void clm_channel_internal_push_waiter(
    clm_channel_internal_waiter** head,
    clm_channel_internal_waiter** tail,
    clm_channel_internal_waiter* waiter)
{
    waiter->next = NULL;
    if (*tail == NULL)
        *head = waiter;
    else
        (*tail)->next = waiter;
    *tail = waiter;
}

static inline clm_channel_internal_waiter* clm_channel_internal_pop_waiter(
    clm_channel_internal_waiter** head,
    clm_channel_internal_waiter** tail)
{
    clm_channel_internal_waiter* waiter = *head;
    if (waiter != NULL) {
        *head = waiter->next;
        if (*head == NULL)
            *tail = NULL;
        waiter->next = NULL;
    }
    return waiter;
}

static inline void clm_channel_internal_remove_waiter(
    clm_channel_internal_waiter** head,
    clm_channel_internal_waiter** tail,
    clm_channel_internal_waiter* target)
{
    clm_channel_internal_waiter* previous = NULL;
    for (clm_channel_internal_waiter* waiter = *head; waiter != NULL;
        waiter = waiter->next) {
        if (waiter == target) {
            if (previous == NULL)
                *head = waiter->next;
            else
                previous->next = waiter->next;
            if (*tail == waiter)
                *tail = previous;
            waiter->next = NULL;
            return;
        }
        previous = waiter;
    }
}

static inline void clm_channel_internal_buffer_push(
    clm_channel_internal* channel,
    const clm_id message_id)
{
    channel->buffer[channel->tail] = message_id;
    channel->tail = (channel->tail + 1) % channel->capacity;
    ++channel->count;
}

static inline clm_id clm_channel_internal_buffer_pop(
    clm_channel_internal* channel)
{
    const clm_id message_id = channel->buffer[channel->head];
    channel->head = (channel->head + 1) % channel->capacity;
    --channel->count;
    return message_id;
}

/** Moves oldest blocked senders into newly available buffer slots. */
static inline void clm_channel_internal_drain_senders(
    clm_channel_internal* channel)
{
    while (!channel->closed && channel->capacity != 0 &&
        channel->count < channel->capacity && channel->send_head != NULL) {
        clm_channel_internal_waiter* sender =
            clm_channel_internal_pop_waiter(&channel->send_head,
                &channel->send_tail);
        clm_channel_internal_buffer_push(channel, sender->message_id);
        sender->matched = true;
    }
    cnd_broadcast(&channel->changed);
}

/** Delivers buffered messages to blocked receivers, then reports closed/drained. */
static inline void clm_channel_internal_drain_receivers(
    clm_channel_internal* channel)
{
    while (channel->receive_head != NULL && channel->count != 0) {
        clm_channel_internal_waiter* receiver =
            clm_channel_internal_pop_waiter(&channel->receive_head,
                &channel->receive_tail);
        *receiver->out_id = clm_channel_internal_buffer_pop(channel);
        *receiver->out_open = true;
        receiver->matched = true;
    }
    if (channel->closed && channel->count == 0) {
        clm_channel_internal_waiter* receiver;
        while ((receiver = clm_channel_internal_pop_waiter(
                &channel->receive_head, &channel->receive_tail)) != NULL) {
            *receiver->out_id = 0;
            *receiver->out_open = false;
            receiver->closed = true;
        }
    }
    cnd_broadcast(&channel->changed);
}

static inline clm_int clm_channel_internal_try_send_locked(
    clm_channel_internal* channel,
    const clm_id message_id)
{
    if (channel->closed)
        return CLM_CHANNEL_OPERATION_CLOSED;
    clm_channel_internal_waiter* receiver =
        clm_channel_internal_pop_waiter(&channel->receive_head,
            &channel->receive_tail);
    if (receiver != NULL) {
        *receiver->out_id = message_id;
        *receiver->out_open = true;
        receiver->matched = true;
        cnd_broadcast(&channel->changed);
        return CLM_CHANNEL_OPERATION_SUCCESS;
    }
    if (channel->capacity == 0 || channel->send_head != NULL ||
        channel->count == channel->capacity)
        return CLM_CHANNEL_OPERATION_FULL;
    clm_channel_internal_buffer_push(channel, message_id);
    clm_channel_internal_drain_receivers(channel);
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

static inline clm_int clm_channel_internal_try_receive_locked(
    clm_channel_internal* channel,
    clm_id* out_id,
    bool* out_open)
{
    if (channel->count != 0) {
        *out_id = clm_channel_internal_buffer_pop(channel);
        *out_open = true;
        clm_channel_internal_drain_senders(channel);
        clm_channel_internal_drain_receivers(channel);
        return CLM_CHANNEL_OPERATION_SUCCESS;
    }
    clm_channel_internal_waiter* sender =
        clm_channel_internal_pop_waiter(&channel->send_head,
            &channel->send_tail);
    if (sender != NULL) {
        *out_id = sender->message_id;
        *out_open = true;
        sender->matched = true;
        cnd_broadcast(&channel->changed);
        return CLM_CHANNEL_OPERATION_SUCCESS;
    }
    if (channel->closed) {
        *out_id = 0;
        *out_open = false;
        return CLM_CHANNEL_OPERATION_CLOSED;
    }
    return CLM_CHANNEL_OPERATION_EMPTY;
}

static inline clm_int clm_channel_internal_deadline(
    const uint64_t timeout_ms,
    struct timespec* deadline)
{
    if (deadline == NULL || timespec_get(deadline, TIME_UTC) != TIME_UTC)
        return CLM_CHANNEL_OPERATION_FAILURE;
    uint64_t additional_seconds = timeout_ms / 1000;
    const long additional_nanoseconds =
        (long)(timeout_ms % 1000) * 1000000L;
    long nanoseconds = deadline->tv_nsec + additional_nanoseconds;
    if (nanoseconds >= 1000000000L) {
        ++additional_seconds;
        nanoseconds -= 1000000000L;
    }
    const uint64_t max_time = sizeof(time_t) >= sizeof(int64_t)
        ? (uint64_t)INT64_MAX
        : (UINT64_C(1) << (sizeof(time_t) * CHAR_BIT - 1)) - 1;
    const uint64_t current_seconds = (uint64_t)deadline->tv_sec;
    if (additional_seconds > max_time ||
        current_seconds > max_time - additional_seconds)
        return CLM_CHANNEL_OPERATION_FAILURE;
    deadline->tv_sec = (time_t)(current_seconds + additional_seconds);
    deadline->tv_nsec = nanoseconds;
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

/** Creates an OPEN, empty channel with fixed capacity.
 * capacity==0 creates an unbuffered rendezvous channel.
 * Duplicate live channel IDs and allocation failures return FAILURE.
 * @return SUCCESS or FAILURE.
 */
static inline clm_int clm_channel_create(
    const clm_id channel_id,
    const size_t capacity)
{
    if (capacity > (size_t)-1 / sizeof(clm_id))
        return CLM_CHANNEL_OPERATION_FAILURE;
    clm_channel_internal* channel =
        (clm_channel_internal*)calloc(1, sizeof(*channel));
    if (channel == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    if (capacity != 0) {
        channel->buffer = (clm_id*)calloc(capacity, sizeof(clm_id));
        if (channel->buffer == NULL) {
            free(channel);
            return CLM_CHANNEL_OPERATION_FAILURE;
        }
    }
    if (mtx_init(&channel->mutex, mtx_plain) != thrd_success) {
        free(channel->buffer);
        free(channel);
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    if (cnd_init(&channel->changed) != thrd_success) {
        mtx_destroy(&channel->mutex);
        free(channel->buffer);
        free(channel);
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    channel->id = channel_id;
    channel->capacity = capacity;
    clm_channel_internal_lock_registry();
    for (clm_channel_internal* current = clm_channel_internal_channels;
        current != NULL; current = current->next)
        if (current->id == channel_id) {
            clm_channel_internal_unlock_registry();
            cnd_destroy(&channel->changed);
            mtx_destroy(&channel->mutex);
            free(channel->buffer);
            free(channel);
            return CLM_CHANNEL_OPERATION_FAILURE;
        }
    channel->next = clm_channel_internal_channels;
    clm_channel_internal_channels = channel;
    clm_channel_internal_unlock_registry();
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

/** Permanently closes the channel for sending, without deleting it.
 * Receivers may still drain already queued messages in FIFO order.
 * Blocked senders wake and return CLOSED. Blocked receivers can drain pending
 * messages; after the queue empties, they wake/return CLOSED.
 * Closing an already closed, still-existing channel returns CLOSED.
 * @return SUCCESS, CLOSED (already closed), or FAILURE (unknown channel).
 */
static inline clm_int clm_channel_close(const clm_id channel_id)
{
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    mtx_lock(&channel->mutex);
    if (channel->closed) {
        mtx_unlock(&channel->mutex);
        return CLM_CHANNEL_OPERATION_CLOSED;
    }
    channel->closed = true;
    clm_channel_internal_waiter* sender;
    while ((sender = clm_channel_internal_pop_waiter(&channel->send_head,
            &channel->send_tail)) != NULL)
        sender->closed = true;
    clm_channel_internal_drain_receivers(channel);
    cnd_broadcast(&channel->changed);
    mtx_unlock(&channel->mutex);
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

/** Destroys a channel and its internal queue/locks/condition variables.
 * Caller MUST guarantee no other thread can concurrently access or wait on
 * this channel (normally close and join/wait for users first).
 * Any unread message IDs are discarded, but referenced resources are not
 * destroyed or otherwise modified. Destroy does not call BPA cleanup.
 * @return SUCCESS or FAILURE (unknown channel / invalid state).
 */
static inline clm_int clm_channel_destroy(const clm_id channel_id)
{
    clm_channel_internal_lock_registry();
    clm_channel_internal** link = &clm_channel_internal_channels;
    while (*link != NULL && (*link)->id != channel_id)
        link = &(*link)->next;
    clm_channel_internal* channel = *link;
    if (channel == NULL) {
        clm_channel_internal_unlock_registry();
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    mtx_lock(&channel->mutex);
    if (channel->send_head != NULL || channel->receive_head != NULL) {
        mtx_unlock(&channel->mutex);
        clm_channel_internal_unlock_registry();
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    *link = channel->next;
    mtx_unlock(&channel->mutex);
    clm_channel_internal_unlock_registry();
    cnd_destroy(&channel->changed);
    mtx_destroy(&channel->mutex);
    free(channel->buffer);
    free(channel);
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

/** Immediately sends message_id if it can complete without waiting.
 * Buffered channels enqueue when space is available. An unbuffered channel
 * succeeds only when a receiver is already waiting. The ID is passed by
 * VALUE; the channel does not copy or own the referenced data.
 * @return SUCCESS, FULL (would block), CLOSED, or FAILURE (invalid ID).
 */
static inline clm_int clm_channel_try_send(
    const clm_id channel_id,
    const clm_id message_id)
{
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    mtx_lock(&channel->mutex);
    clm_int status = clm_channel_internal_try_send_locked(channel, message_id);
    mtx_unlock(&channel->mutex);
    return status;
}

/** Immediately receives a message if available.
 * For an unbuffered channel, succeeds only when a sender is waiting. On
 * SUCCESS, writes the message ID and sets *out_open=true. On CLOSED, writes
 * zero and sets *out_open=false, matching Go's receive result after close.
 * @return SUCCESS, EMPTY (open but no sender/value), CLOSED, or FAILURE.
 */
static inline clm_int clm_channel_try_receive(
    const clm_id channel_id,
    clm_id* out_id,
    bool* out_open)
{
    if (out_id == NULL || out_open == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    mtx_lock(&channel->mutex);
    clm_int status = clm_channel_internal_try_receive_locked(channel,
        out_id, out_open);
    mtx_unlock(&channel->mutex);
    return status;
}

static inline clm_int clm_channel_send_timeout(
    const clm_id channel_id,
    const clm_id message_id,
    const uint64_t timeout_ms);
static inline clm_int clm_channel_receive_timeout(
    const clm_id channel_id,
    clm_id* out_id,
    bool* out_open,
    const uint64_t timeout_ms);

/** Sends an ID, blocking until accepted by a receiver/buffer or the channel
 * is closed. For capacity zero, this pairs with one receive operation.
 * @return SUCCESS, CLOSED (including close while blocked), or FAILURE.
 * Do NOT call from cooperative scheduler step functions.
 */
static inline clm_int clm_channel_send(
    const clm_id channel_id,
    const clm_id message_id)
{
    return clm_channel_send_timeout(channel_id, message_id,
        CLM_CHANNEL_WAIT_FOREVER);
}

/** Receives a message, blocking until a sender/value is available or the
 * channel is closed and drained. On SUCCESS, sets *out_open=true. On CLOSED,
 * writes zero to *out_id and sets *out_open=false.
 * @return SUCCESS, CLOSED, or FAILURE (invalid channel/output).
 * Do NOT call from cooperative scheduler step functions.
 */
static inline clm_int clm_channel_receive(
    const clm_id channel_id,
    clm_id* out_id,
    bool* out_open)
{
    return clm_channel_receive_timeout(channel_id, out_id, out_open,
        CLM_CHANNEL_WAIT_FOREVER);
}

/** Sends an ID with an elapsed-time deadline. timeout_ms==0 is nonblocking;
 * CLM_CHANNEL_WAIT_FOREVER is equivalent to clm_channel_send.
 * @return SUCCESS, FULL, TIMEOUT, CLOSED, or FAILURE.
 */
static inline clm_int clm_channel_send_timeout(
    const clm_id channel_id,
    const clm_id message_id,
    const uint64_t timeout_ms)
{
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    struct timespec deadline;
    const bool timed = timeout_ms != CLM_CHANNEL_WAIT_FOREVER && timeout_ms != 0;
    if (timed && clm_channel_internal_deadline(timeout_ms, &deadline) !=
        CLM_CHANNEL_OPERATION_SUCCESS)
        return CLM_CHANNEL_OPERATION_FAILURE;
    mtx_lock(&channel->mutex);
    clm_int status = clm_channel_internal_try_send_locked(channel, message_id);
    if (status != CLM_CHANNEL_OPERATION_FULL || timeout_ms == 0) {
        mtx_unlock(&channel->mutex);
        return status;
    }
    clm_channel_internal_waiter waiter = {0};
    waiter.message_id = message_id;
    clm_channel_internal_push_waiter(&channel->send_head,
        &channel->send_tail, &waiter);
    for (;;) {
        if (waiter.matched) {
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_SUCCESS;
        }
        if (waiter.closed) {
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_CLOSED;
        }
        const int wait_status = timed
            ? cnd_timedwait(&channel->changed, &channel->mutex, &deadline)
            : cnd_wait(&channel->changed, &channel->mutex);
        if (wait_status == thrd_timedout && !waiter.matched && !waiter.closed) {
            clm_channel_internal_remove_waiter(&channel->send_head,
                &channel->send_tail, &waiter);
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_TIMEOUT;
        }
        if (wait_status != thrd_success && wait_status != thrd_timedout) {
            if (!waiter.matched && !waiter.closed)
                clm_channel_internal_remove_waiter(&channel->send_head,
                    &channel->send_tail, &waiter);
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_FAILURE;
        }
    }
}

/** Receives with an elapsed-time deadline. timeout_ms==0 is nonblocking;
 * CLM_CHANNEL_WAIT_FOREVER is equivalent to clm_channel_receive.
 * As with receive(), CLOSED writes zero to out_id and false to out_open.
 * @return SUCCESS, EMPTY, TIMEOUT, CLOSED, or FAILURE.
 */
static inline clm_int clm_channel_receive_timeout(
    const clm_id channel_id,
    clm_id* out_id,
    bool* out_open,
    const uint64_t timeout_ms)
{
    if (out_id == NULL || out_open == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    struct timespec deadline;
    const bool timed = timeout_ms != CLM_CHANNEL_WAIT_FOREVER && timeout_ms != 0;
    if (timed && clm_channel_internal_deadline(timeout_ms, &deadline) !=
        CLM_CHANNEL_OPERATION_SUCCESS)
        return CLM_CHANNEL_OPERATION_FAILURE;
    mtx_lock(&channel->mutex);
    clm_int status = clm_channel_internal_try_receive_locked(channel, out_id,
        out_open);
    if (status != CLM_CHANNEL_OPERATION_EMPTY || timeout_ms == 0) {
        mtx_unlock(&channel->mutex);
        return status;
    }
    clm_channel_internal_waiter waiter = {0};
    waiter.out_id = out_id;
    waiter.out_open = out_open;
    clm_channel_internal_push_waiter(&channel->receive_head,
        &channel->receive_tail, &waiter);
    for (;;) {
        if (waiter.matched) {
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_SUCCESS;
        }
        if (waiter.closed) {
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_CLOSED;
        }
        const int wait_status = timed
            ? cnd_timedwait(&channel->changed, &channel->mutex, &deadline)
            : cnd_wait(&channel->changed, &channel->mutex);
        if (wait_status == thrd_timedout && !waiter.matched && !waiter.closed) {
            clm_channel_internal_remove_waiter(&channel->receive_head,
                &channel->receive_tail, &waiter);
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_TIMEOUT;
        }
        if (wait_status != thrd_success && wait_status != thrd_timedout) {
            if (!waiter.matched && !waiter.closed)
                clm_channel_internal_remove_waiter(&channel->receive_head,
                    &channel->receive_tail, &waiter);
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_FAILURE;
        }
    }
}

/** Copies a thread-safe snapshot of capacity, count, and closed state.
 * The snapshot is informational, not a substitute for checking send/receive
 * results: another thread may change the channel immediately afterward.
 * @return SUCCESS or FAILURE (unknown channel / NULL out_info).
 */
static inline clm_int clm_channel_get_info(
    const clm_id channel_id,
    clm_channel_info* out_info)
{
    if (out_info == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL)
        return CLM_CHANNEL_OPERATION_FAILURE;
    mtx_lock(&channel->mutex);
    const clm_channel_info info = {
        channel->capacity, channel->count, channel->closed
    };
    mtx_unlock(&channel->mutex);
    *out_info = info;
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

