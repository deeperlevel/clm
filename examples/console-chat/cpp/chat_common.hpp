#pragma once

/*
 * Console Chat example â€” shared protocol and thread-safe ID mailbox.
 *
 * This file is an application example, not a new CLM module. It deliberately
 * keeps the on-the-wire protocol independent of the networking implementation
 * so C, Python, Go, Zig, and other CLM implementations can interoperate.
 * The in-process chat_packet struct is NOT the wire format: peers exchange the
 * text lines below, not raw C struct memory or CLM resource IDs. This avoids
 * dependencies on pointer width, enum representation, padding, or byte order.
 *
 * Protocol: one UTF-8, LF-terminated command per line (no embedded LF or NUL).
 * Client -> server: HELLO <name>, SEND <target> <message>, QUIT
 * Server -> client: ACCEPTED, MSG <sender> <message>, NOTICE <text>, KICKED
 * A target of '.' means the server console, NOT broadcast.
 * For example, HELLO alice registers a connection and SEND bob hello asks the
 * server to forward MSG alice hello to bob. Every actual wire command ends
 * with LF; a trailing CR is tolerated when decoding CRLF input.
 *
 * This header also adapts CLM's integer channels to application packet values:
 *   producer copies packet -> reserves mailbox slot -> sends slot index + 1
 *   consumer receives slot ID -> copies packet out -> releases mailbox slot.
 * The channel synchronizes the queue of IDs. A C11 mutex separately protects
 * the slot registry holding the values those IDs refer to. These are distinct
 * pieces of state with distinct responsibilities, not two copies of one queue.
 *
 * Functions are static inline so each executable can include these application
 * helpers without a separate common .c object. This header intentionally does
 * not define CLM_*_IMPLEMENTATION: each executable's implementation-owning .c
 * file must instantiate its required CLM module state before first inclusion.
 *
 * This tiny teaching protocol is NOT encrypted, authenticated, or suitable
 * for hostile Internet-facing deployments. All lines are size-limited.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <threads.h>
#include "clm/base.hpp"
#include "clm/channel.hpp"

/* These are example policy limits, not CLM-imposed limits. Name/text/line
 * sizes count bytes, including space for a C-string NUL where strings are
 * stored. CHAT_LINE_SIZE leaves room for protocol keywords, names, and a
 * message; a multibyte UTF-8 character can consume several of those bytes.
 * Incoming/outgoing capacities are per connection. The input allocation can
 * be larger than the maximum permitted single protocol line. */
#define CHAT_MAX_CLIENTS 64u
#define CHAT_NAME_SIZE 32u
#define CHAT_TEXT_SIZE 900u
#define CHAT_LINE_SIZE 1024u
#define CHAT_INPUT_CAPACITY 4096u
#define CHAT_OUTPUT_CAPACITY 16384u
/* Slots hold whole chat_packet values. Both routing directions share this
 * registry; exhausted slots can reject a send even if its channel has room. */
#define CHAT_MAILBOX_SLOTS 256u

/* Application-chosen resource identities. UINT64_C makes the constants have
 * an appropriate 64-bit integer representation, matching the clm_id model.
 * Reuse an ID consistently for operations on that resource; do not treat it
 * as a pointer or a native OS handle. Both executables may use these same
 * numeric constants because their CLM registries live in separate processes.
 *
 * CHAT_TO_ROUTER / CHAT_TO_REACTOR are channel resource IDs. The values sent
 * INSIDE those channels are different IDs: temporary mailbox slot numbers.
 * Pool, task, listener, and client socket constants identify their respective
 * module resources; no pointer to a chat_packet is transported in a channel. */
#define CHAT_TO_ROUTER UINT64_C(310001)
#define CHAT_TO_REACTOR UINT64_C(310002)
#define CHAT_ROUTER_POOL UINT64_C(310003)
#define CHAT_ROUTER_TASK UINT64_C(310004)
#define CHAT_CLIENT_CONSOLE UINT64_C(310005)
#define CHAT_LISTENER_ID UINT64_C(310006)
#define CHAT_CLIENT_SOCKET_ID UINT64_C(310007)

/* EVENT values describe requests from a producer; ACTION values describe
 * decisions ready for the reactor to apply. Keeping them separate makes the
 * worker/owner boundary visible even though today's routing policy is simple.
 * CLIENT values let the companion client use the same mailbox representation.
 * The server's ordinary shutdown path bypasses this queue via an atomic flag,
 * so a full message channel cannot prevent administrative termination. */
typedef enum chat_kind {
    CHAT_EVENT_SEND = 1,
    CHAT_EVENT_KICK,
    CHAT_EVENT_STOP,
    CHAT_ACTION_SEND,
    CHAT_ACTION_KICK,
    CHAT_ACTION_STOP,
    CHAT_CLIENT_INPUT,
    CHAT_CLIENT_EXIT
} chat_kind;

/* A packet is self-contained: all text is stored inline, not through char*
 * fields borrowing console buffers or BPA blocks. Ordinary struct assignment
 * therefore copies the complete payload with no reference-counting or nested
 * free operations. Construct one locally, submit it, and immediately reuse
 * the local variable; the successful sender has published a separate copy.
 *
 * source_socket is meaningful only in the process owning that socket. It lets
 * the server route an error back to the originating connection without using
 * a username that might have been reassigned. It is never serialized to TCP. */
typedef struct chat_packet {
    chat_kind kind;
    clm_id source_socket;           /* 0 means a console-originated message. */
    char sender[CHAT_NAME_SIZE];
    char target[CHAT_NAME_SIZE];
    char text[CHAT_TEXT_SIZE];
} chat_packet;

/* occupied remains true from reservation until receipt, including the period
 * after reservation but before the slot ID is successfully enqueued. A sender
 * that fails to enqueue must roll back this reservation itself. */
typedef struct chat_mailbox_slot {
    bool occupied;
    chat_packet packet;
} chat_mailbox_slot;

/* One mutex protects occupancy AND packet contents for every slot. Channel
 * internals use their own synchronization; the application must not assume a
 * thread-safe channel automatically protects arbitrary memory elsewhere.
 * Never copy an initialized mailbox: copying mtx_t is not a valid mutex clone. */
typedef struct chat_mailbox {
    mtx_t mutex;
    chat_mailbox_slot slots[CHAT_MAILBOX_SLOTS];
} chat_mailbox;

/** Initialize the mailbox's slot registry and its synchronization primitive.
 *
 * Call once, before starting producers/consumers. Zeroing the slots marks each
 * one free, but does not create a usable mutex; mtx_init is required separately.
 * Initialization does not create channels. The executable creates the channels
 * under its chosen IDs and later passes those IDs to send/receive.
 * @param mailbox Application-owned mailbox, valid until mailbox_destroy().
 * @return true if the mutex was successfully created.
 */
static inline bool chat_mailbox_init(chat_mailbox* mailbox)
{
    if (mailbox == NULL) {
        return false;
    }
    memset(mailbox, 0, sizeof(*mailbox));
    return mtx_init(&mailbox->mutex, mtx_plain) == thrd_success;
}

/** Release synchronization resources after all producers/consumers have stopped.
 *
 * This does not close channels, stop async tasks, join threads, or drain packets.
 * The caller must establish that no thread can enter either mailbox helper
 * again before destroying this mutex. In the server that ordering is enforced
 * by the console submission gate and destruction of the async pool first.
 * @param mailbox A previously initialized, otherwise unused mailbox.
 */
static inline void chat_mailbox_destroy(chat_mailbox* mailbox)
{
    mtx_destroy(&mailbox->mutex);
}

/** Transfer a value into the mailbox and enqueue its integer slot ID.
 *
 * CLM channels carry clm_id values, not copied payloads. Using a synchronized
 * slot registry avoids passing C pointers disguised as IDs or sharing BPA's
 * non-thread-safe global storage across the reactor and async worker threads.
 * On failure, no mailbox slot remains allocated. The channel operation does not
 * wait for queue capacity, though acquiring a mutex may briefly wait for its
 * current holder; "try send" is not a claim that the whole helper is lock-free.
 *
 * Ownership here refers to the copy stored in a slot, not the caller's packet.
 * A successful return only means the receiving side can now retrieve that copy;
 * it does not mean a worker has processed it or a network recipient received it.
 * @param mailbox Synchronized, application-owned slot registry.
 * @param channel_id Existing buffered CLM channel.
 * @param packet Value copied to the slot; caller keeps its own copy.
 * @return true when ownership is transferred to the receiving side.
 */
static inline bool chat_mailbox_send(
    chat_mailbox* mailbox,
    clm_id channel_id,
    const chat_packet* packet)
{
    size_t index = 0;
    if (mailbox == NULL || packet == NULL) {
        return false;
    }
    /* Reserve and initialize while holding the same mutex used by receivers.
     * No consumer can observe a partially copied packet through this registry.
     * A linear scan is deliberately simple and bounded by CHAT_MAILBOX_SLOTS;
     * a larger application could replace it with a synchronized free list. */
    mtx_lock(&mailbox->mutex);
    for (; index < CHAT_MAILBOX_SLOTS; ++index) {
        if (!mailbox->slots[index].occupied) {
            mailbox->slots[index].occupied = true;
            mailbox->slots[index].packet = *packet;
            break;
        }
    }
    mtx_unlock(&mailbox->mutex);
    /* Release the mailbox mutex before calling a channel operation. Keeping
     * these critical sections separate avoids holding the application lock
     * while entering another module's internal synchronization. */
    if (index == CHAT_MAILBOX_SLOTS) {
        return false;
    }

    /* Slot IDs start at 1 because zero is reserved for sentinels. These are
     * local indices, not BPA allocations or globally registered clm_id names.
     * The slot cannot be reused until its receiver copies it, or this sender
     * rolls back a failed enqueue. The packet was initialized BEFORE this
     * publication point, so a receiver can run as soon as try_send succeeds. */
    if (clm_channel_try_send(channel_id, (clm_id)(index + 1u)) !=
        CLM_CHANNEL_OPERATION_SUCCESS) {
        /* Full/closed/otherwise failed channel: no receiver acquired this ID,
         * so release the reservation. Omitting this rollback would leak a
         * mailbox slot on every failed send until all intake stopped working. */
        mtx_lock(&mailbox->mutex);
        mailbox->slots[index].occupied = false;
        mtx_unlock(&mailbox->mutex);
        return false;
    }
    return true;
}

/** Consume one queued packet (if any) without blocking an async worker.
 *
 * A channel operation status and its open output describe different things:
 * this helper needs both a successful operation and an actual delivered value,
 * not a closed-channel end condition. It collapses empty, closed, and invalid
 * slot cases to false because the example's callers only need "packet ready?".
 * Empty therefore means WAIT to the async task, not necessarily task failure.
 * Mutex acquisition can still briefly contend even though try_receive never
 * waits for a new channel value to arrive.
 * @param mailbox Shared mailbox whose slots are synchronized.
 * @param channel_id Channel holding the slot IDs.
 * @param out_packet Value destination, written only on successful receipt.
 * @return true if a complete packet has been consumed.
 */
static inline bool chat_mailbox_receive(
    chat_mailbox* mailbox,
    clm_id channel_id,
    chat_packet* out_packet)
{
    clm_id slot_id = 0;
    bool open = false;
    if (mailbox == NULL || out_packet == NULL ||
        clm_channel_try_receive(channel_id, &slot_id, &open) !=
            CLM_CHANNEL_OPERATION_SUCCESS ||
        !open || slot_id == 0 || slot_id > CHAT_MAILBOX_SLOTS) {
        return false;
    }
    const size_t index = (size_t)(slot_id - 1u);
    /* A successful channel receive consumes this queued ID exactly once.
     * Check its range before indexing, then use the registry's own mutex to
     * copy the payload and free the slot as one indivisible operation. */
    mtx_lock(&mailbox->mutex);
    const bool valid = mailbox->slots[index].occupied;
    if (valid) {
        *out_packet = mailbox->slots[index].packet;
        /* Free only after copying. A producer may reuse the slot immediately
         * after unlock, but out_packet is already an independent value. Clearing
         * the old bytes is unnecessary: occupied controls whether they are live. */
        mailbox->slots[index].occupied = false;
    }
    mtx_unlock(&mailbox->mutex);
    return valid;
}

/** Validate a portable chat username, avoiding protocol delimiter ambiguity.
 *
 * This is application validation rather than a limitation of CLM strings,
 * which support UTF-8. Restricting names to this ASCII subset avoids whitespace
 * delimiters, line breaks, and locale-dependent non-ASCII names in the protocol.
 * The server separately decides whether an otherwise valid name is already in
 * use; this helper does not query any CLM registry or reserve the name.
 * @param name NUL-terminated proposed name.
 * @return true for 1..31 ASCII letters, digits, underscore, or hyphen.
 */
static inline bool chat_valid_name(const char* name)
{
    const size_t length = name == NULL ? 0u : strlen(name);
    if (length == 0u || length >= CHAT_NAME_SIZE) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        /* ctype functions require EOF or a value representable as unsigned char;
         * casting before isalnum avoids undefined behavior from signed char.
         * The c < 128 test enforces ASCII even if the active locale is broader. */
        const unsigned char c = (unsigned char)name[i];
        if (!(isalnum(c) && c < 128) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

/** Parse a port number with strict decimal validation and no truncation.
 *
 * CLP retrieves strings; it does not decide which numeric domain a chat option
 * belongs to. Reject signs, spaces, suffixes, and port zero, which would request
 * an OS-assigned ephemeral port rather than the explicit listening port wanted
 * by this example. Leading zeros are accepted. Accumulate in unsigned int and
 * test each step against 65535 before the eventual uint16_t cast.
 * @param text Decimal text in the range 1..65535.
 * @param out_port Written only when parsing succeeds.
 * @return true when the value is valid.
 */
static inline bool chat_parse_port(const char* text, uint16_t* out_port)
{
    unsigned int value = 0;
    if (text == NULL || *text == '\0' || out_port == NULL) {
        return false;
    }
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return false;
        }
        value = value * 10u + (unsigned int)(*p - '0');
        if (value > 65535u) {
            return false;
        }
    }
    if (value == 0u) {
        return false;
    }
    *out_port = (uint16_t)value;
    return true;
}

/** Trim a newline from console input without changing the actual message.
 *
 * Only the final LF and then final CR are removed. Interior whitespace and
 * trailing message spaces are preserved, unlike a generic trim operation.
 * This helper expects a complete C string, not an unterminated network chunk;
 * the server's socket reader performs its own incremental LF framing.
 * @param line Mutable line returned by fgets().
 */
static inline void chat_strip_newline(char* line)
{
    const size_t n = strlen(line);
    if (n != 0u && line[n - 1u] == '\n') {
        line[n - 1u] = '\0';
    }
    const size_t m = strlen(line);
    if (m != 0u && line[m - 1u] == '\r') {
        line[m - 1u] = '\0';
    }
}

/** Parse 'send <name-or-dot> <message>' without losing spaces in the message.
 *
 * This is a small application grammar, not shell tokenization or CLP parsing.
 * CLP is used for process options; interactive/wire commands have their own
 * stable protocol. Only the target separator is replaced with NUL. The message
 * remains one contiguous slice so spaces inside it do not become extra args.
 *
 * Returned pointers BORROW the caller's command buffer. They are valid only
 * until that storage is changed or goes out of scope, and must never be freed.
 * Copy both slices into a chat_packet before crossing a thread boundary.
 * Failure may still leave command modified; no rollback of the separator is
 * promised. This routine checks shape and byte length, not UTF-8 correctness;
 * the server validates incoming message encoding with clm_string_measure().
 * @param command Mutable NUL-terminated command line; modified in place.
 * @param out_target Pointer to target within command on success.
 * @param out_message Pointer to message within command on success.
 * @return true if both a target and a nonempty message were provided.
 */
static inline bool chat_parse_send(
    char* command,
    const char** out_target,
    const char** out_message)
{
    if (strncmp(command, "send ", 5u) != 0) {
        return false;
    }
    char* target = command + 5;
    while (*target == ' ') {
        ++target;
    }
    char* divider = strchr(target, ' ');
    if (divider == NULL) {
        return false;
    }
    *divider = '\0';
    /* target is now its own C string inside command, while message starts just
     * beyond its terminator. Leading separator spaces are skipped; spaces later
     * in the message are untouched. A lone dot is the only special target. */
    char* message = divider + 1;
    while (*message == ' ') {
        ++message;
    }
    if ((*target != '.' || target[1] != '\0') && !chat_valid_name(target)) {
        return false;
    }
    if (*message == '\0' || strlen(message) >= CHAT_TEXT_SIZE) {
        return false;
    }
    *out_target = target;
    *out_message = message;
    return true;
}

/** Append a framed wire line to a client's bounded outgoing byte queue.
 *
 * The caller must own the socket's transmit state. A partial send is handled
 * by advancing an offset, not by discarding the remainder. This helper only
 * appends; network transmission is performed separately.
 *
 * Buffer invariant: 0 <= *offset <= *length <= CHAT_OUTPUT_CAPACITY.
 * Bytes before offset were already accepted by the socket; bytes in
 * [offset, length) still need sending. This byte queue is not a CLM channel:
 * a channel holds integer IDs, whereas this helper stores serialized wire data.
 * The destination buffer need not have a terminating NUL because send uses an
 * explicit byte length. The input line does need a NUL so strlen can measure it;
 * that terminator is not appended and is not sent over TCP.
 * @param queue Buffer with fixed CHAT_OUTPUT_CAPACITY.
 * @param length Current valid byte count, updated on success.
 * @param offset Number already transmitted, updated after compaction.
 * @param line NUL-terminated line INCLUDING final '\n'.
 * @return true on success, false when queue capacity would be exceeded.
 */
static inline bool chat_queue_line(
    char queue[CHAT_OUTPUT_CAPACITY],
    size_t* length,
    size_t* offset,
    const char* line)
{
    size_t bytes = strlen(line);
    if (*offset > 0u) {
        /* Source and destination overlap, so use memmove rather than memcpy.
         * Compaction reclaims bytes already sent without changing the order of
         * pending bytes. Even a failed append may leave this compacted layout;
         * the existing pending content is preserved, not the original indices. */
        memmove(queue, queue + *offset, *length - *offset);
        *length -= *offset;
        *offset = 0u;
    }
    if (bytes > CHAT_OUTPUT_CAPACITY - *length) {
        /* Check remaining capacity before adding, avoiding size_t addition
         * overflow. A false result lets the owner apply its slow-client policy
         * rather than reallocating without limit or truncating a protocol line. */
        return false;
    }
    memcpy(queue + *length, line, bytes);
    *length += bytes;
    return true;
}

