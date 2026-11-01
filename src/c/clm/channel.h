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

#include "base.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <threads.h>
#include <time.h>

/* --------------------------------------------------------------------------
 * clm_channel — bounded, multi-producer/multi-consumer message channels.
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

#define CLM_CHANNEL_OPERATION_SUCCESS 0
#define CLM_CHANNEL_OPERATION_FAILURE -1
#define CLM_CHANNEL_OPERATION_EMPTY -2
#define CLM_CHANNEL_OPERATION_FULL -3
#define CLM_CHANNEL_OPERATION_CLOSED -4
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

void clm_channel_internal_lock_registry(void);

void clm_channel_internal_unlock_registry(void);

clm_channel_internal* clm_channel_internal_find(clm_id channel_id);

void clm_channel_internal_push_waiter(
    clm_channel_internal_waiter** head,
    clm_channel_internal_waiter** tail,
    clm_channel_internal_waiter* waiter
);

clm_channel_internal_waiter* clm_channel_internal_pop_waiter(
    clm_channel_internal_waiter** head,
    clm_channel_internal_waiter** tail
);

void clm_channel_internal_remove_waiter(
    clm_channel_internal_waiter** head,
    clm_channel_internal_waiter** tail,
    const clm_channel_internal_waiter* target
);

void clm_channel_internal_buffer_push(clm_channel_internal* channel, clm_id message_id);

clm_id clm_channel_internal_buffer_pop(clm_channel_internal* channel);

/** Moves oldest blocked senders into newly available buffer slots. */
void clm_channel_internal_drain_senders(clm_channel_internal* channel);

/** Delivers buffered messages to blocked receivers, then reports closed/drained. */
void clm_channel_internal_drain_receivers(clm_channel_internal* channel);

clm_int clm_channel_internal_try_send_locked(clm_channel_internal* channel, clm_id message_id);

clm_int clm_channel_internal_try_receive_locked(
    clm_channel_internal* channel,
    clm_id* out_id,
    bool* out_open
);

clm_int clm_channel_internal_deadline(
    uint64_t timeout_ms,
    struct timespec* deadline
);

/** Creates an OPEN, empty channel with fixed capacity.
 * capacity==0 creates an unbuffered rendezvous channel.
 * Duplicate live channel IDs and allocation failures return FAILURE.
 * @return SUCCESS or FAILURE.
 * @param channel_id CLM ID identifying the channel.
 * @param capacity Available capacity.
 */
clm_int clm_channel_create(
    clm_id channel_id,
    size_t capacity
);

/** Permanently closes the channel for sending, without deleting it.
 * Receivers may still drain already queued messages in FIFO order.
 * Blocked senders wake and return CLOSED. Blocked receivers can drain pending
 * messages; after the queue empties, they wake/return CLOSED.
 * Closing an already closed, still-existing channel returns CLOSED.
 * @return SUCCESS, CLOSED (already closed), or FAILURE (unknown channel).
 * @param channel_id CLM ID identifying the channel.
 */
clm_int clm_channel_close(clm_id channel_id);

/** Destroys a channel and its internal queue/locks/condition variables.
 * Caller MUST guarantee no other thread can concurrently access or wait on
 * this channel (normally close and join/wait for users first).
 * Any unread message IDs are discarded, but referenced resources are not
 * destroyed or otherwise modified. Destroy does not call BPA cleanup.
 * @return SUCCESS or FAILURE (unknown channel / invalid state).
 * @param channel_id CLM ID identifying the channel.
 */
clm_int clm_channel_destroy(clm_id channel_id);

/** Immediately sends message_id if it can complete without waiting.
 * Buffered channels enqueue when space is available. An unbuffered channel
 * succeeds only when a receiver is already waiting. The ID is passed by
 * VALUE; the channel does not copy or own the referenced data.
 * @return SUCCESS, FULL (would block), CLOSED, or FAILURE (invalid ID).
 * @param channel_id CLM ID identifying the channel.
 * @param message_id CLM ID identifying the message.
 */
clm_int clm_channel_try_send(clm_id channel_id, clm_id message_id);

/** Immediately receives a message if available.
 * For an unbuffered channel, succeeds only when a sender is waiting. On
 * SUCCESS, writes the message ID and sets *out_open=true. On CLOSED, writes
 * zero and sets *out_open=false, matching Go's receive result after close.
 * @return SUCCESS, EMPTY (open but no sender/value), CLOSED, or FAILURE.
 * @param channel_id CLM ID identifying the channel.
 * @param out_id Receives the id.
 * @param out_open Receives the open.
 */
clm_int clm_channel_try_receive(clm_id channel_id, clm_id* out_id, bool* out_open);

/** Sends an ID with an elapsed-time deadline. timeout_ms==0 is nonblocking;
 * CLM_CHANNEL_WAIT_FOREVER is equivalent to clm_channel_send.
 * @return SUCCESS, FULL, TIMEOUT, CLOSED, or FAILURE.
 * @param channel_id CLM ID identifying the channel.
 * @param message_id CLM ID identifying the message.
 * @param timeout_ms Input parameter described above.
 */
clm_int clm_channel_send_timeout(clm_id channel_id, clm_id message_id, uint64_t timeout_ms);

/** Receives with an elapsed-time deadline. timeout_ms==0 is nonblocking;
 * CLM_CHANNEL_WAIT_FOREVER is equivalent to clm_channel_receive.
 * As with receive(), CLOSED writes zero to out_id and false to out_open.
 * @return SUCCESS, EMPTY, TIMEOUT, CLOSED, or FAILURE.
 * @param channel_id CLM ID identifying the channel.
 * @param out_id Receives the id.
 * @param out_open Receives the open.
 * @param timeout_ms Input parameter described above.
 */
clm_int clm_channel_receive_timeout(clm_id channel_id, clm_id* out_id, bool* out_open, uint64_t timeout_ms);

/** Sends an ID, blocking until accepted by a receiver/buffer or the channel
 * is closed. For capacity zero, this pairs with one receive operation.
 * @return SUCCESS, CLOSED (including close while blocked), or FAILURE.
 * Do NOT call from cooperative scheduler step functions.
 * @param channel_id CLM ID identifying the channel.
 * @param message_id CLM ID identifying the message.
 */
clm_int clm_channel_send(clm_id channel_id, clm_id message_id);

/** Receives a message, blocking until a sender/value is available or the
 * channel is closed and drained. On SUCCESS, sets *out_open=true. On CLOSED,
 * writes zero to *out_id and sets *out_open=false.
 * @return SUCCESS, CLOSED, or FAILURE (invalid channel/output).
 * Do NOT call from cooperative scheduler step functions.
 * @param channel_id CLM ID identifying the channel.
 * @param out_id Receives the id.
 * @param out_open Receives the open.
 */
clm_int clm_channel_receive(clm_id channel_id, clm_id* out_id, bool* out_open);

/** Copies a thread-safe snapshot of capacity, count, and closed state.
 * The snapshot is informational, not a substitute for checking send/receive
 * results: another thread may change the channel immediately afterward.
 * @return SUCCESS or FAILURE (unknown channel / NULL out_info).
 * @param channel_id CLM ID identifying the channel.
 * @param out_info Receives the info.
 */
clm_int clm_channel_get_info(clm_id channel_id, clm_channel_info* out_info);