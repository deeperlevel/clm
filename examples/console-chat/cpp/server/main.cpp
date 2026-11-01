/*
 * CLM Console Chat Server (C11)
 *
 * Demonstrates how to COMPOSE existing CLM modules:
 *   clm_net        TCP listener, sockets, nonblocking I/O, readiness polling
 *   clm_async      Cooperative, nonblocking task steps (via async_pool)
 *   clm_async_pool Worker thread(s) for routing chat commands
 *   clm_channel    Thread-safe, bounded transfer of integer message IDs
 *   clm_bpa/string ID-based storage of connected usernames
 *   clm_clp        Command-line option parsing into an owned dictionary
 *
 * READING THIS EXAMPLE:
 * CLM exposes resources through clm_id values. A socket ID is an application
 * handle understood by clm_net, not a native Windows SOCKET or POSIX file
 * descriptor. Likewise, a string ID identifies storage managed by CLM rather
 * than a char* that the caller must free. The application chooses IDs when
 * creating resources and passes the same IDs to later module operations.
 * Passing an integer between threads does not, by itself, make the resource
 * behind that integer safe to access concurrently.
 *
 * There are three execution roles, with deliberately separate responsibilities:
 *   1. main() becomes the reactor: it owns peers, BPA strings, and network I/O.
 *   2. A console thread blocks on stdin and submits copied command packets.
 *   3. A CLM async-pool task consumes events and produces reactor actions.
 * The pool has two workers, but this example submits only one routing task;
 * worker count and number of independent application tasks are different ideas.
 *
 * Follow one chat message through the application:
 *   TCP bytes -> complete SEND line -> validated chat_packet
 *   -> mailbox slot ID on CHAT_TO_ROUTER -> server_route_step()
 *   -> mailbox slot ID on CHAT_TO_REACTOR -> server_apply()
 *   -> recipient's output buffer -> one or more clm_net_send() calls.
 * The protocol, mailbox, and output buffers are application code. CLM supplies
 * the reusable networking, scheduling, channel, string, and dictionary pieces.
 *
 * IMPORTANT OWNERSHIP RULES:
 * - Only the reactor thread may create/use/delete CLM string/BPA resources.
 *   The current BPA module is not thread-safe.
 * - Only the reactor thread touches socket I/O and the peer list.
 * - The router task transfers immutable values via a synchronized mailbox
 *   and CLM channels; it never accesses BPA or socket structures.
 * - Async task callbacks use channel try-operations, not waiting channel calls.
 *   Mutex acquisition and the overflow diagnostic can still block briefly;
 *   this teaching example is not a hard real-time scheduler.
 *
 * See ../chat_common.h for the shared protocol and its deliberate limitations.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

/* CLM's headers provide module APIs and, for these modules, declarations of
 * shared state. Defining the matching IMPLEMENTATION switches before the
 * first include emits that state in this translation unit. This executable
 * has just one .c file, so it is also the natural implementation owner.
 *
 * In a multi-file application, put each implementation definition in exactly
 * one .c file, not in a common header: missing definitions cause unresolved
 * symbols, while multiple definitions can cause linker errors. Dependencies
 * matter too: CLP uses the ID registry, so CLM_ID_IMPLEMENTATION is required
 * even though this server does not implement its own named-ID registry.
 * CLP itself supplies inline functions and needs no CLM_CLP_IMPLEMENTATION.
 * Header inclusion order matters because these switches must be visible when
 * the relevant header is first processed. */
#define CLM_BPA_IMPLEMENTATION
#define CLM_STRING_IMPLEMENTATION
#define CLM_ID_IMPLEMENTATION
#define CLM_NET_IMPLEMENTATION
#define CLM_ASYNC_IMPLEMENTATION
#define CLM_ASYNC_POOL_IMPLEMENTATION
#define CLM_CHANNEL_IMPLEMENTATION

#include "clm/net.cpp"             /* Includes base.hpp and string.hpp. */
#include "clm/async_pool.hpp"      /* Includes async.h. */
#include "clm/channel.hpp"
#include "clm/clp.hpp"
#include "../chat_common.hpp"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* One application-owned connection record, not a CLM resource in its own
 * right. Its lifetime is a slot in server.peers; CLM resources referenced by
 * its IDs must be released separately before that slot is reused.
 *
 * Connection state progresses from active/unregistered to registered, then
 * optionally closing_after_flush, and finally inactive after server_drop().
 * The closing state preserves a final NOTICE/KICKED response while forbidding
 * further input. It is not equivalent to closing the socket immediately. */
typedef struct chat_peer {
    bool active;
    bool registered;
    bool closing_after_flush;
    clm_id socket_id;
    clm_id username_id;  /* BPA-backed string; reactor thread owns lifetime. */
    /* A copied name can safely be placed in a packet; the CLM string remains
     * the authoritative record used by server_find(). No borrowed BPA pointer
     * is stored here or sent to the async worker. */
    char name[CHAT_NAME_SIZE];
    char incoming[CHAT_INPUT_CAPACITY];
    size_t input_length;
    /* The unsent byte range is [output_offset, output_length). These buffers
     * are plain application memory, not additional CLM strings or channels. */
    char outgoing[CHAT_OUTPUT_CAPACITY];
    size_t output_length;
    size_t output_offset;
} chat_peer;

/* All members except the explicitly synchronized ones belong to the reactor.
 * mailbox has its own mutex; console_gate protects submission during teardown;
 * stop_requested is atomic because producers set it while the reactor reads it.
 * stopping is reactor-only state, so it does not need to be atomic. */
typedef struct chat_server {
    chat_peer peers[CHAT_MAX_CLIENTS];
    clm_id next_socket_id;
    clm_id next_string_id;
    bool stopping;
    atomic_bool stop_requested;
    unsigned int shutdown_ticks;
    clm_id listener_id;
    chat_mailbox mailbox;
    mtx_t console_gate;
    bool console_stopped;
} chat_server;

/* This example has one process-lifetime server object, so the router accesses
 * it directly and ignores its context_id argument. The spawn call passes zero
 * as an unused context ID; it does not encode &server as an integer. A larger
 * application could resolve a context ID through its own synchronized registry. */
static chat_server server;

/** Submit a copied event without waiting for space in a channel.
 *
 * Ordinary commands take the mailbox/channel path. Shutdown takes a separate
 * atomic path so a full mailbox, full inbound channel, or full outbound channel
 * cannot discard the request. Multiple shutdown requests coalesce into true;
 * there is no need to count them or allocate one packet per request.
 *
 * The default C11 atomic operations used here are sequentially consistent.
 * They communicate a request only; they do not grant the producer permission
 * to close sockets or modify the reactor's peer table.
 * @param packet Complete value to transfer to the async worker.
 * @return true if queued or shutdown requested; false if ordinary intake is full.
 */
static bool server_submit(const chat_packet* packet)
{
    if (packet->kind == CHAT_EVENT_STOP) {
        atomic_store(&server.stop_requested, true);
        return true;
    }
    if (!chat_mailbox_send(&server.mailbox, CHAT_TO_ROUTER, packet)) {
        return false;
    }
    /* Publish before waking: a worker that runs immediately must already be
     * able to receive the slot ID and copy the fully initialized packet. */
    /* If the task is WAITING, wake it; if it has not reached WAIT yet, the
     * async implementation tracks pending wakes so no notification is lost. */
    (void)clm_async_pool_wake(CHAT_ROUTER_POOL, CHAT_ROUTER_TASK);
    return true;
}

/** Cooperative routing step: translate incoming events to network actions.
 *
 * The task is intentionally stackless: all state lives in the mailbox and
 * each invocation processes at most 16 messages. WAIT gives worker threads
 * back to the scheduler; a new producer event wakes the task explicitly.
 * WAIT does not put the operating-system worker to sleep inside this callback:
 * it tells the scheduler that this task needs an explicit wake before more
 * useful work is expected. YIELD means the task can be scheduled again without
 * waiting for a producer. Local variables do not survive either return, so any
 * state needed across invocations must live outside this C call stack.
 *
 * The callback performs no BPA calls, socket calls, waiting channel operations,
 * or thrd_sleep(). Its overflow fprintf() is an example diagnostic, not a
 * guarantee of nonblocking logging. Routing is intentionally simple: moving
 * policy into a worker demonstrates composition without sharing live sockets.
 * @param task_id Scheduler identity; unused because there is one router.
 * @param context_id Application context identity; unused in this example.
 * @return YIELD if a batch was processed; WAIT if its queue is empty.
 */
static clm_async_step_result server_route_step(clm_id task_id, clm_id context_id)
{
    (void)task_id;
    (void)context_id;
    for (unsigned int count = 0; count < 16u; ++count) {
        chat_packet packet;
        if (!chat_mailbox_receive(&server.mailbox, CHAT_TO_ROUTER, &packet)) {
            return CLM_ASYNC_STEP_WAIT;
        }
        switch (packet.kind) {
            case CHAT_EVENT_SEND:
                packet.kind = CHAT_ACTION_SEND;
                break;
            case CHAT_EVENT_KICK:
                packet.kind = CHAT_ACTION_KICK;
                break;
            case CHAT_EVENT_STOP:
                /* Defensive support for a stop packet placed directly on the
                 * input channel. Normal server_submit() calls bypass queues. */
                atomic_store(&server.stop_requested, true);
                continue;
            default:
                continue;
        }
        /* This queue is bounded too. A successful input submission therefore
         * does not promise final delivery: an ordinary action can be dropped
         * here. Production delivery guarantees require explicit retry/ack or
         * backpressure policy. The atomic stop path is not subject to this loss. */
        if (!chat_mailbox_send(&server.mailbox, CHAT_TO_REACTOR, &packet)) {
            fprintf(stderr, "[router] outbound command queue is full\n");
        }
    }
    return CLM_ASYNC_STEP_YIELD;
}

/** Find a named client. Called on the reactor thread only.
 * @param name Validated username.
 * @return Borrowed pointer to a live peer or NULL.
 * @note The returned peer is not retained or reference-counted. A later drop
 *       clears its contents, so callers must not cache it as a stable identity.
 */
static chat_peer* server_find(const char* name)
{
    for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
        chat_peer* peer = &server.peers[i];
        if (peer->active && peer->registered && !peer->closing_after_flush) {
            /* The CLM string is the authoritative username record. The
             * fixed char array in chat_peer is merely a snapshot for logs
             * and mailbox packets. This borrowed pointer remains valid
             * because only this reactor thread ever changes BPA resources. */
            clm_bpa_block_node* block = NULL;
            const char* stored_name = NULL;
            size_t stored_bytes = 0u;
            size_t character_count = 0u;
            /* get_block exposes existing string storage without making a copy.
             * Its byte length and character count are separate because UTF-8
             * characters may occupy multiple bytes. Usernames are ASCII here,
             * so strcmp is sufficient and comparison remains case-sensitive.
             * Do not free stored_name or block: their owner is CLM/BPA. */
            if (clm_string_get_block(peer->username_id, &block,
                    &stored_name, &stored_bytes, &character_count) ==
                    CLM_BPA_SUCCESS && strcmp(stored_name, name) == 0) {
                return peer;
            }
        }
    }
    return NULL;
}

/** Close a peer and release its CLM-owned username resource.
 * @param peer Peer owned by the reactor thread. Safe for repeated calls.
 * @note This closes the network socket after its outstanding nonblocking
 *       operations have returned; it does NOT close from inside a worker.
 */
static void server_drop(chat_peer* peer)
{
    if (!peer->active) {
        return;
    }
    if (peer->registered) {
        printf("[SERVER] %s left\n", peer->name);
        fflush(stdout);
        (void)clm_string_delete(peer->username_id);
    }
    /* Deleting a username does not close its socket, and closing a socket does
     * not delete its username: these are independent module resources. Only a
     * successfully registered peer owns a username that needs deleting. */
    (void)clm_net_socket_close(peer->socket_id);
    memset(peer, 0, sizeof(*peer));
}

/** Append a wire command to one client's bounded transmit buffer.
 * @return true when buffered; false if a slow client exceeded its queue.
 * @note Success means copied into application memory, not delivered to TCP or
 *       read by the recipient. This function can drop the peer on failure, so
 *       callers must recheck peer->active before further socket operations.
 */
static bool server_enqueue(chat_peer* peer, const char* line)
{
    if (!peer->active || !chat_queue_line(peer->outgoing,
            &peer->output_length, &peer->output_offset, line)) {
        /* A very slow client must not consume unbounded server memory. */
        server_drop(peer);
        return false;
    }
    return true;
}

/** Flush as many pending output bytes as the nonblocking socket accepts.
 * @param peer Live connection with an application-managed output queue.
 * @note A TCP send may be partial. WOULD_BLOCK is not a disconnection.
 *       Readiness is a hint, not a promise that an entire buffer will fit.
 */
static void server_flush(chat_peer* peer)
{
    while (peer->active && peer->output_offset < peer->output_length) {
        size_t sent = 0;
        clm_net_status status = clm_net_send(peer->socket_id,
            peer->outgoing + peer->output_offset,
            peer->output_length - peer->output_offset, &sent);
        if (status == CLM_NET_WOULD_BLOCK) {
            /* Preserve both indices. A future WRITE-ready notification will
             * retry exactly the remaining bytes instead of duplicating data. */
            return;
        }
        if (status != CLM_NET_SUCCESS || sent == 0u) {
            server_drop(peer);
            return;
        }
        peer->output_offset += sent;
    }
    if (peer->active && peer->output_offset == peer->output_length) {
        peer->output_offset = 0u;
        peer->output_length = 0u;
        if (peer->closing_after_flush) {
              /* "Flushed" means accepted by the local socket send operation,
               * not acknowledged or processed by the remote chat application. */
            server_drop(peer);
        }
    }
}

/** Validate, register and acknowledge a new client's HELLO line.
 * @param peer Connection which has not yet registered its name.
 * @param name Text after the literal "HELLO ".
 * @note HELLO is registration, not authentication. A client chooses its own
 *       name; this example neither proves identity nor reserves accounts.
 */
static void server_register(chat_peer* peer, const char* name)
{
    if (!chat_valid_name(name) || server_find(name) != NULL) {
        (void)server_enqueue(peer, "NOTICE Invalid or duplicate name\n");
        peer->closing_after_flush = true;
        return;
    }
    /* The caller chooses an unused string ID and CLM creates storage under it.
     * This small demo uses monotonic ranges for its sockets and strings rather
     * than asking clm_id to associate human-readable names with every resource.
     * It assumes these ranges do not wrap or collide during its lifetime.
     * A clm_id is a handle, not a type-safe distinction between resource kinds. */
    peer->username_id = server.next_string_id++;
    if (!clm_string_new(peer->username_id, name)) {
        (void)server_enqueue(peer, "NOTICE Server resource limit\n");
        peer->closing_after_flush = true;
        return;
    }
    /* name points into a temporary input line. clm_string_new() creates the
     * owned CLM record; this separate copy provides fixed-size packet/log data.
     * Marking registered only after creation makes cleanup ownership explicit. */
    memcpy(peer->name, name, strlen(name) + 1u);
    peer->registered = true;
    printf("[SERVER] %s joined\n", peer->name);
    fflush(stdout);
    (void)server_enqueue(peer, "ACCEPTED\n");
}

/** Handle exactly one complete, LF-free application protocol line.
 * @param peer Connection which delivered the line.
 * @param line NUL-terminated, no embedded NUL or newline.
 * @note Only the reactor calls this function, so no peer-list mutex is needed.
 */
static void server_process_line(chat_peer* peer, char* line)
{
    if (!peer->registered) {
        /* Registration is the only permitted first operation. Setting the
         * closing flag causes server_read() to discard any remaining commands
         * in the same receive chunk, not merely reject the first bad line. */
        if (strncmp(line, "HELLO ", 6u) == 0) {
            server_register(peer, line + 6);
        } else {
            (void)server_enqueue(peer, "NOTICE Send HELLO name first\n");
            peer->closing_after_flush = true;
        }
        return;
    }
    if (strcmp(line, "QUIT") == 0) {
        server_drop(peer);
        return;
    }
    if (strncmp(line, "SEND ", 5u) != 0) {
        (void)server_enqueue(peer, "NOTICE Expected SEND name message or QUIT\n");
        return;
    }

    /* Reuse the same parsing conventions as the console, after making the
     * first word lower-case for our tiny ASCII-only wire command grammar. */
    memcpy(line, "send ", 5u);
    const char* target = NULL;
    const char* message = NULL;
    if (!chat_parse_send(line, &target, &message)) {
        (void)server_enqueue(peer, "NOTICE Invalid message\n");
        return;
    }
    /* The application protocol specifies UTF-8 text, not arbitrary binary.
     * Validate before copying bytes into a worker-visible message packet. */
    size_t unicode_count = 0u;
    if (clm_string_measure(message, strlen(message),
            CLM_STRING_ENCODING_UTF8, &unicode_count) != CLM_BPA_SUCCESS) {
        (void)server_enqueue(peer, "NOTICE Message must be valid UTF-8\n");
        return;
    }
    /* target and message borrow slices of line, which is stack storage in
     * server_read(). Copy every needed byte into a self-contained value before
     * handing work to another thread. Never queue these temporary pointers. */
    chat_packet packet = {0};
    packet.kind = CHAT_EVENT_SEND;
    packet.source_socket = peer->socket_id;
    snprintf(packet.sender, sizeof(packet.sender), "%s", peer->name);
    snprintf(packet.target, sizeof(packet.target), "%s", target);
    snprintf(packet.text, sizeof(packet.text), "%s", message);
    if (!server_submit(&packet)) {
        (void)server_enqueue(peer, "NOTICE Server queue is busy\n");
    }
}

/** Read at most 8 KiB per polling cycle, preserving partial text lines.
 *
 * TCP is a byte stream, NOT a message transport: one recv() can contain half
 * a command, several commands, or any other combination. Lines remain in
 * peer->incoming until the LF delimiter arrives; oversized/binary lines are
 * rejected instead of being silently truncated or executed.
 *
 * A per-call byte budget makes network intake cooperative just like the async
 * task's per-call message budget. Remaining socket bytes stay in the kernel;
 * an incomplete line stays in peer->incoming. On the next polling cycle a
 * readable socket gets another turn. No unread portion of a received chunk is
 * discarded when the budget expires, because each receive is capped by the
 * remaining budget and its entire chunk is parsed before the next receive.
 */
static void server_read(chat_peer* peer)
{
    unsigned char chunk[2048];
    size_t read_budget = 8192u;
    while (peer->active && !peer->closing_after_flush && read_budget != 0u) {
        size_t received = 0;
        clm_net_status status = clm_net_receive(peer->socket_id,
            chunk, read_budget < sizeof(chunk) ? read_budget : sizeof(chunk),
            &received);
        if (status == CLM_NET_WOULD_BLOCK) {
            return;
        }
        if (status != CLM_NET_SUCCESS || received == 0u) {
            server_drop(peer);
            return;
        }
        read_budget -= received;
        for (size_t i = 0; i < received && peer->active; ++i) {
            const unsigned char c = chunk[i];
            if (c == 0u || (c < 32u && c != '\n' && c != '\r' && c != '\t')) {
                 /* Embedded NUL would make strlen/strcmp see a shorter command
                  * than the network supplied. Reject it instead of allowing two
                  * different interpretations of the same wire bytes. UTF-8
                  * validity of message text is checked separately above. */
                server_drop(peer);
                return;
            }
            if (c == '\n') {
                /* Remove CR from CRLF, if present. */
                size_t n = peer->input_length;
                if (n != 0u && peer->incoming[n - 1u] == '\r') {
                    --n;
                }
                peer->incoming[n] = '\0';
                char complete[CHAT_LINE_SIZE];
                /* Reset framing state before dispatching. The dispatcher may
                 * close and clear this peer, while complete remains valid for
                 * the duration of this call. Packet creation copies from it. */
                memcpy(complete, peer->incoming, n + 1u);
                peer->input_length = 0u;
                server_process_line(peer, complete);
                if (!peer->active || peer->closing_after_flush) {
                    /* Do not execute pipelined input after QUIT, rejection,
                     * or an enqueue failure that dropped the connection. */
                    return;
                }
            } else if (peer->input_length >= CHAT_LINE_SIZE - 1u) {
                server_drop(peer);
                return;
            } else {
                peer->incoming[peer->input_length++] = (char)c;
            }
        }
    }
}

/** Accept all ready connections; a fixed table caps total simultaneous peers.
 * @note The listening socket and accepted sockets are set nonblocking so the
 *       reactor can service every client without a thread per client.
 *       accept() readiness can become stale, so WOULD_BLOCK is a normal end
 *       to this loop. Each successful accept creates a separate socket resource
 *       under the new caller-supplied ID; the listener remains open.
 */
static void server_accept_ready(void)
{
    for (;;) {
        chat_peer* slot = NULL;
        for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
            if (!server.peers[i].active) {
                slot = &server.peers[i];
                break;
            }
        }
        if (slot == NULL) {
            return; /* Admission control; new peers remain queued in kernel. */
        }
        clm_id id = server.next_socket_id++;
        clm_net_status status = clm_net_accept(server.listener_id, id, NULL);
        if (status == CLM_NET_WOULD_BLOCK) {
            return;
        }
        if (status != CLM_NET_SUCCESS) {
            fprintf(stderr, "[SERVER] accept failed: %d\n", (int)status);
            return;
        }
        if (clm_net_socket_set_nonblocking(id, true) != CLM_NET_SUCCESS) {
              /* Configure accepted sockets explicitly rather than assuming they
               * inherit nonblocking mode consistently on every platform. */
            (void)clm_net_socket_close(id);
            continue;
        }
        memset(slot, 0, sizeof(*slot));
        slot->active = true;
        slot->socket_id = id;
    }
}

/** Apply a completed routing action; all socket operations stay here.
 * @param packet Action value created by the async router.
 * @note Actions contain copied names rather than peer pointers. Resolve the
 *       recipient when applying an action because connections can disappear
 *       while the worker is preparing it. Name reuse can target a later
 *       connection with that name; the example has no session-generation IDs.
 */
static void server_apply(const chat_packet* packet)
{
    if (packet->kind == CHAT_ACTION_STOP) {
        /* Stop admission first, then drain existing peers' final responses.
         * The worker never performs these network operations itself. */
        server.stopping = true;
        (void)clm_net_socket_close(server.listener_id);
        for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
            chat_peer* peer = &server.peers[i];
            if (peer->active) {
                (void)server_enqueue(peer, "KICKED\n");
                peer->closing_after_flush = true;
            }
        }
        printf("[SERVER] Stopping; disconnecting all clients...\n");
        fflush(stdout);
        return;
    }
    if (packet->kind == CHAT_ACTION_KICK) {
        chat_peer* peer = server_find(packet->target);
        if (peer == NULL) {
            printf("[SERVER] Client '%s' not found\n", packet->target);
        } else {
            printf("[SERVER] Kicking %s\n", peer->name);
            (void)server_enqueue(peer, "KICKED\n");
            peer->closing_after_flush = true;
        }
        fflush(stdout);
        return;
    }
    if (packet->kind != CHAT_ACTION_SEND) {
        return;
    }
    if (strcmp(packet->target, ".") == 0) {
        /* The dot is an application-level destination, not a CLM socket ID and
         * not a wildcard. This branch writes only to the server's console. */
        printf("[%s -> server] %s\n", packet->sender, packet->text);
        fflush(stdout);
        return;
    }
    chat_peer* recipient = server_find(packet->target);
    if (recipient == NULL) {
        if (packet->source_socket != 0) {
              /* Reply to the original connection ID, not merely a matching
               * username that another connection might have registered later. */
            for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
                chat_peer* source = &server.peers[i];
                if (source->active && source->socket_id == packet->source_socket) {
                    (void)server_enqueue(source, "NOTICE Client not found\n");
                    break;
                }
            }
        } else {
            printf("[SERVER] Client '%s' not found\n", packet->target);
            fflush(stdout);
        }
        return;
    }
    char line[CHAT_LINE_SIZE];
    /* Serialize fields into the text protocol rather than sending the memory
     * layout of chat_packet. Struct padding, enums, and integer byte order are
     * not a portable network format. snprintf's return also detects truncation. */
    int n = snprintf(line, sizeof(line), "MSG %s %s\n",
        packet->sender, packet->text);
    if (n <= 0 || (size_t)n >= sizeof(line)) {
        return;
    }
    (void)server_enqueue(recipient, line);
}

/** Drain already-computed actions before handling any fresh network events.
 * @note This keeps the server's client list and sockets owned by one thread.
 */
static void server_drain_actions(void)
{
    /* Check the lossless control path before consuming ordinary queued work.
     * The producer only sets stop_requested; this reactor converts the request
     * into the same local stop action used by the rest of the implementation. */
    if (!server.stopping && atomic_load(&server.stop_requested)) {
        const chat_packet stop = {.kind = CHAT_ACTION_STOP};
        server_apply(&stop);
    }
    if (server.stopping) {
        return;
    }
    chat_packet packet;
    /* Limit routing work too, so a busy worker cannot keep the reactor from
     * polling sockets indefinitely. Shutdown intentionally abandons pending
     * ordinary actions; reliable shutdown is not a promise to deliver them. */
    for (unsigned int i = 0; i < 64u &&
        chat_mailbox_receive(&server.mailbox, CHAT_TO_REACTOR, &packet); ++i) {
        server_apply(&packet);
        if (server.stopping) {
            break;
        }
    }
}

/** Main reactor: one thread multiplexes all active TCP connections.
 * @return 0 for normal shutdown, nonzero on an unrecoverable poll failure.
 * @note Polling reports readiness; this example does not start one thread or
 *       one CLM async task per connection. All socket operations remain short
 *       nonblocking calls in this loop, with application-owned framing state.
 */
static int server_run(void)
{
    while (true) {
        server_drain_actions();
        size_t connected = 0;
        for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
            if (server.peers[i].active) {
                ++connected;
            }
        }
        if (server.stopping && connected == 0u) {
            break;
        }
        /* During shutdown give outgoing KICKED lines a bounded number of loop
         * turns before cleanup forces close, even if a client never reads.
         * This is a cycle budget, not an exact wall-clock deadline: each poll
         * may return earlier than its 40 ms timeout when a socket is ready. */
        if (server.stopping && server.shutdown_ticks++ >= 25u) {
            break;
        }

        clm_net_poll_entry entries[CHAT_MAX_CLIENTS + 1u];
        chat_peer* mapping[CHAT_MAX_CLIENTS + 1u];
        /* entries is CLM's poll input/output array; mapping is our parallel
         * lookup table from an array position to application peer state.
         * Rebuild both each cycle rather than retaining stale socket handles.
         * events asks for readiness; revents is filled with observed flags. */
        size_t count = 0u;
        if (!server.stopping) {
            entries[count] = (clm_net_poll_entry){
                server.listener_id, CLM_NET_EVENT_READ, 0u};
            mapping[count++] = NULL; /* NULL designates listening socket. */
        }
        for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
            chat_peer* peer = &server.peers[i];
            if (peer->active) {
                unsigned int flags = peer->closing_after_flush ? 0u :
                    CLM_NET_EVENT_READ;
                /* Watching WRITE on every connection would often make poll
                 * return immediately: idle TCP sockets are usually writable.
                 * Request it only while our application has bytes to flush.
                 * Closing peers retain WRITE interest but not READ interest. */
                if (peer->output_offset < peer->output_length) {
                    flags |= CLM_NET_EVENT_WRITE;
                }
                entries[count] = (clm_net_poll_entry){peer->socket_id, flags, 0u};
                mapping[count++] = peer;
            }
        }
        if (count != 0u) {
            size_t ready = 0u;
            /* Poll's timeout prevents an idle reactor from busy-spinning and
             * bounds its usual wait before checking worker/console requests.
             * The separate ready count is informational here; revents tells
             * us which entries need work. TIMEOUT is not a networking error. */
            clm_net_status status = clm_net_poll(entries, count, 40u, &ready);
            if (status != CLM_NET_SUCCESS && status != CLM_NET_TIMEOUT) {
                fprintf(stderr, "[SERVER] poll failed: %d\n", (int)status);
                return 1;
            }
            for (size_t i = 0; i < count; ++i) {
                if (entries[i].revents == 0u) {
                    continue;
                }
                chat_peer* peer = mapping[i];
                if (peer == NULL) {
                    server_accept_ready();
                    continue;
                }
                if (!peer->active) {
                    /* Earlier work in this cycle may have dropped a peer;
                     * never issue I/O using an inactive record's old mapping. */
                    continue;
                }
                if (entries[i].revents & (CLM_NET_EVENT_ERROR | CLM_NET_EVENT_HANGUP)) {
                    server_drop(peer);
                } else {
                    if (entries[i].revents & CLM_NET_EVENT_READ) {
                        server_read(peer);
                    }
                    if (peer->active && (entries[i].revents & CLM_NET_EVENT_WRITE)) {
                        /* server_read() can drop the peer, so test active
                        * again before acting on the earlier WRITE snapshot. */
                        server_flush(peer);
                    }
                }
            }
        } else {
            /* Only possible while shutdown is draining an empty peer set. */
            break;
        }
        /* Process replies quickly even if a socket hasn't changed readiness. */
        server_drain_actions();
    }
    return 0;
}

/** Read administrative commands on a separate C11 thread.
 *
 * fgets() is blocking, and must never run inside clm_async_pool's worker.
 * The thread is detached because standard C offers no portable way to cancel
 * a blocking terminal read on remote shutdown. It only writes through a
 * guarded submission gate; the reactor closes that gate before teardown.
 * Parsing is local to this thread. After submission, only the packet copy
 * belongs to the mailbox; line can immediately be reused by the next fgets().
 * This thread never looks up BPA names or reads/modifies the peer array.
 * @param opaque Unused C11 thread argument.
 */
static int server_console_main(void* opaque)
{
    (void)opaque;
    char line[CHAT_LINE_SIZE];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (strchr(line, '\n') == NULL && !feof(stdin)) {
              /* Drain the rest of an oversized console line. Otherwise its tail
               * could be mistaken for a second administrative command. */
            int c;
            while ((c = getchar()) != '\n' && c != EOF) { }
            puts("[SERVER] Command too long");
            continue;
        }
        chat_strip_newline(line);
        chat_packet packet = {0};
        if (strcmp(line, "quit") == 0) {
            packet.kind = CHAT_EVENT_STOP;
        } else if (strncmp(line, "kick ", 5u) == 0 &&
                chat_valid_name(line + 5)) {
            packet.kind = CHAT_EVENT_KICK;
            snprintf(packet.target, sizeof(packet.target), "%s", line + 5);
        } else {
            const char* target = NULL;
            const char* message = NULL;
            if (!chat_parse_send(line, &target, &message)) {
                puts("[SERVER] Commands: send <name> <message> | kick <name> | quit");
                continue;
            }
            packet.kind = CHAT_EVENT_SEND;
            snprintf(packet.sender, sizeof(packet.sender), "server");
            snprintf(packet.target, sizeof(packet.target), "%s", target);
            snprintf(packet.text, sizeof(packet.text), "%s", message);
        }
        /* The gate and mailbox mutex solve different problems. mailbox.mutex
         * protects slot data; console_gate prevents a detached producer from
         * entering CLM while main is destroying channels and the async pool.
         * Hold the gate across the stopped test AND publication. */
        mtx_lock(&server.console_gate);
        if (server.console_stopped) {
            mtx_unlock(&server.console_gate);
            break;
        }
        const bool sent = server_submit(&packet);
        mtx_unlock(&server.console_gate);
        if (!sent) {
            puts("[SERVER] Command queue full; retry");
        }
        if (packet.kind == CHAT_EVENT_STOP) {
              /* server_submit() handles STOP atomically, so this exit cannot
               * lose shutdown merely because ordinary message queues are full. */
            return 0;
        }
    }
    /* EOF means shutdown, useful for shell pipelines and automated tests.
     * A debugger that provides no stdin may therefore cause a normal immediate
     * exit. Run with an interactive terminal when administering it by hand. */
    chat_packet stop = {.kind = CHAT_EVENT_STOP};
    mtx_lock(&server.console_gate);
    if (!server.console_stopped) {
        (void)server_submit(&stop);
    }
    mtx_unlock(&server.console_gate);
    return 0;
}

/** Parse the server port, defaulting to 8080.
 *
 * Normalize the positional port to a CLP option and release the resulting
 * dictionary after validating its string value as a TCP port.
 *
 * CLP parses option syntax, not the application's schema. This function still
 * decides which options and argument shapes are supported and converts the
 * retrieved string to uint16_t. Parsing runs before any worker starts, so all
 * BPA/dictionary/ID operations here are single-threaded.
 * @param argc/argv Process arguments; argv[0] is the executable name.
 * @param port Receives the default or successfully validated requested port.
 * @return true if all arguments were valid.
 */
static bool server_parse_arguments(int argc, char** argv, uint16_t* port)
{
    *port = 8080u;
    if (argc == 1) {
        return true;
    }
    const char* value = NULL;
    const char* key_name = "port";
    bool positional = false;
    if (argc == 2 && strncmp(argv[1], "--port=", 7u) == 0) {
        value = argv[1] + 7;
    } else if (argc == 2 && argv[1][0] != '-') {
        value = argv[1];
        positional = true;
    } else if (argc == 3 && (strcmp(argv[1], "--port") == 0 ||
            strcmp(argv[1], "-p") == 0)) {
        value = argv[2];
        key_name = argv[1] + (argv[1][1] == '-' ? 2 : 1);
    } else {
        return false;
    }
    const clm_id dictionary_id = UINT64_C(320001);
    /* parse_argv skips argv[0]; parse_tokens treats every supplied element as
     * input. CLP does not accept a bare positional port, so normalize that
     * convenience syntax to the two tokens "--port" and its value.
     * The dictionary ID must be unused: CLP does not overwrite existing data. */
    const char* tokens[] = {"--port", value};
    const clm_int status = positional ?
        clm_clp_parse_tokens(2, tokens, dictionary_id) :
        clm_clp_parse_argv(argc, (const char* const*)argv, dictionary_id);
    if (status != CLM_CLP_OPERATION_SUCCESS) {
        /* The CLP failure contract leaves no partial output dictionary, so
         * there is nothing at dictionary_id for us to destroy on this path. */
        return false;
    }
    const size_t capacity = strlen(value) + 1u;
    /* Dictionary keys are clm_id values, not C string keys. CLP registers the
     * option name in clm_id; resolve "port" or "p" to that key before lookup.
     * get_to copies bytes into caller-owned storage rather than exposing a
     * pointer into the dictionary, and includes room for the terminating NUL.
     * The original value's length also preserves support for leading zeros. */
    char* parsed_value = malloc(capacity);
    clm_id key_id;
    const bool valid = parsed_value != NULL &&
        clm_typed_dictionary_count(dictionary_id) == 1 &&
        clm_id_try_get(key_name, &key_id) == CLM_BPA_GET_SUCCESS &&
        clm_typed_dictionary_get_to(dictionary_id, key_id, parsed_value,
            capacity) == CLM_BPA_GET_SUCCESS &&
        chat_parse_port(parsed_value, port);
    free(parsed_value);
    /* A successful parse transfers ownership of this output dictionary to us,
     * including when application-level numeric validation subsequently fails.
     * Delete it on both paths. CLP's option-name registrations are separate
     * process-global ID resources; deleting the dictionary does not remove
     * those registrations. This example leaves them for process teardown. */
    (void)clm_typed_dictionary_delete(dictionary_id);
    return valid;
}

/** Create the module resources, run the reactor, and tear down in owner order.
 *
 * Startup failures return immediately because this executable is ending; the
 * operating system reclaims remaining process resources. Reusing this code in
 * a library or restartable service would require unwinding partially completed
 * startup too, not just the successful-run cleanup sequence below.
 */
int main(int argc, char** argv)
{
    uint16_t port;
    if (!server_parse_arguments(argc, argv, &port)) {
        fprintf(stderr, "Usage: %s [--port 8080 | --port=8080 | 8080]\n", argv[0]);
        return EXIT_FAILURE;
    }
    /* Parse first: invalid options should not start workers or bind a port.
     * Atomic state is explicitly initialized before any other thread sees it;
     * zeroing the surrounding C struct is not a substitute for atomic_init. */
    memset(&server, 0, sizeof(server));
    atomic_init(&server.stop_requested, false);
    server.listener_id = CHAT_LISTENER_ID;
    server.next_socket_id = UINT64_C(100000);
    server.next_string_id = UINT64_C(200000);

    /* Resource order is intentional: initialize synchronization, then channels,
     * then the consumer task. No worker may observe an uninitialized mailbox or
     * publish into a channel that does not exist yet. */
    if (!chat_mailbox_init(&server.mailbox) ||
        mtx_init(&server.console_gate, mtx_plain) != thrd_success) {
        fprintf(stderr, "Failed to initialize synchronization\n");
        return EXIT_FAILURE;
    }
    /* Each channel buffers at most 128 integer IDs. Both directions share the
     * mailbox's 256 packet slots. Channel capacity counts handles, not bytes or
     * chat_packet payloads; the separate mailbox owns the actual copies. */
    if (clm_channel_create(CHAT_TO_ROUTER, 128u) != 0 ||
        clm_channel_create(CHAT_TO_REACTOR, 128u) != 0) {
        fprintf(stderr, "Failed to create CLM channels\n");
        return EXIT_FAILURE;
    }
    /* These are scheduler limits, not chat protocol limits. A pool owns worker
     * threads; spawn registers one cooperative task within it. steps_per_cycle
     * is a scheduler budget, distinct from the callback's 16-packet batch.
     * The reject policy makes overload explicit rather than creating more
     * workers indefinitely. No extra pending-task capacity is requested here. */
    clm_async_pool_config pool_config = {
        .worker_count = 2u,
        .max_tasks_per_worker = 8u,
        .max_pending_tasks = 0u,
        .steps_per_cycle = 16u,
        .overflow_policy = CLM_ASYNC_OVERFLOW_REJECT
    };
    /* The task ID identifies this scheduler task, and the zero context ID is
        * intentionally unused by our callback. The final NULL supplies no context
        * cleanup callback: this task owns no separate per-task context resource.
        * A WAIT result is resumed with clm_async_pool_wake(), using
     * the same pool/task IDs that were used when creating the task. */
    if (clm_async_pool_create(CHAT_ROUTER_POOL, &pool_config) != 0 ||
        clm_async_pool_spawn(CHAT_ROUTER_POOL, CHAT_ROUTER_TASK,
            server_route_step, 0, NULL) != 0) {
        fprintf(stderr, "Failed to start CLM async router\n");
        return EXIT_FAILURE;
    }
    /* Initialize the networking subsystem before creating resources (including
     * platform setup such as Winsock on Windows). CLM hides native socket types
     * behind IDs, but socket_create still needs the desired address family and
     * transport. This listener uses IPv4 and TCP, not datagram messaging. */
    if (clm_net_initialize() != CLM_NET_SUCCESS ||
        clm_net_socket_create(server.listener_id, CLM_NET_IPV4, CLM_NET_TCP) !=
            CLM_NET_SUCCESS) {
        fprintf(stderr, "Failed to initialize TCP networking\n");
        return EXIT_FAILURE;
    }
    /* Address reuse is a best-effort convenience for repeated local runs, not
     * permission to share one server port with arbitrary other listeners. */
    (void)clm_net_socket_set_reuse_address(server.listener_id, true);
    clm_net_address address;
    /* Loopback-only by default. The demonstration protocol has no transport
     * encryption or authentication, so don't expose it on public interfaces. */
    /* address_parse builds a CLM address value; bind associates the socket
     * with it; listen enables incoming TCP connections; nonblocking makes
     * accept/read/write suitable for the single-threaded reactor. The backlog
     * of 64 is a kernel admission setting, not allocation of 64 chat_peer slots. */
    if (clm_net_address_parse("127.0.0.1", port, &address) != CLM_NET_SUCCESS ||
        clm_net_bind(server.listener_id, &address) != CLM_NET_SUCCESS ||
        clm_net_listen(server.listener_id, 64) != CLM_NET_SUCCESS ||
        clm_net_socket_set_nonblocking(server.listener_id, true) != CLM_NET_SUCCESS) {
        fprintf(stderr, "Failed to bind/listen on port %u (native error: %d)\n",
            (unsigned int)port, clm_net_last_native_error());
        return EXIT_FAILURE;
    }

    printf("Ready... Listening on port %u\n", (unsigned int)port);
    puts("Commands: send <name> <message> | kick <name> | quit");
    fflush(stdout);

    /* A blocking terminal read gets its own OS thread. Giving it an async
     * callback would hold a pool worker inside fgets() instead of returning
     * WAIT/YIELD cooperatively. Detaching removes the need to join a thread
     * that might still be waiting for a human to press Enter. */
    thrd_t console_thread;
    if (thrd_create(&console_thread, server_console_main, NULL) != thrd_success) {
        fprintf(stderr, "Unable to start console reader thread\n");
        return EXIT_FAILURE;
    }
    (void)thrd_detach(console_thread);

    const int result = server_run();
    /* Prevent console producer from publishing while channels are destroyed.
     * If fgets() is blocked, it will not touch CLM state after the gate closes. */
    mtx_lock(&server.console_gate);
    server.console_stopped = true;
    mtx_unlock(&server.console_gate);

    /* Destruction follows use dependencies, not just numeric ID order:
     * stop the console producer above, stop/join the pool's workers here, then
     * release reactor-owned sockets/strings, networking, channels, and mailbox.
     * Destroying channels or mailbox first would race a worker's next receive.
     * Destroying the pool does not close our sockets or delete our usernames. */
    (void)clm_async_pool_destroy(CHAT_ROUTER_POOL);
    for (size_t i = 0; i < CHAT_MAX_CLIENTS; ++i) {
        server_drop(&server.peers[i]);
    }
    if (!server.stopping) {
        (void)clm_net_socket_close(server.listener_id);
    }
    (void)clm_net_finalize();
    /* close changes a channel's communication state; destroy releases its
     * resource. Neither frees the chat_packet copies in our application-owned
     * mailbox. Pending packets are plain inline values with no individually
     * allocated payloads, so this shutdown can discard them without per-packet
     * free calls once every producer/consumer has stopped. */
    (void)clm_channel_close(CHAT_TO_ROUTER);
    (void)clm_channel_close(CHAT_TO_REACTOR);
    (void)clm_channel_destroy(CHAT_TO_ROUTER);
    (void)clm_channel_destroy(CHAT_TO_REACTOR);
    chat_mailbox_destroy(&server.mailbox);
    /* The detached console thread might still be blocked in fgets(). Its
     * synchronization gate therefore remains allocated until process exit. */
    puts("[SERVER] Stopped");
    fflush(stdout);
    fflush(stderr);
    /* A detached console thread can still be blocked inside fgets(stdin).
     * On some C runtimes, ordinary exit() waits for stdio's internal lock,
     * potentially hanging. We have already released all CLM resources and
     * flushed diagnostics, so terminate the remaining console thread with
     * the process instead of waiting for non-cancellable C11 input. */
    _Exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

