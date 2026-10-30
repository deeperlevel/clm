/*
 * CLM Console Chat Client (C11)
 *
 * Start the server first, then run:  clm_chat_client 8080 deeper
 *
 * Console commands:
 *   send . Hello from deeper        Send a message to the server console.
 *   send alice Hello Alice!        Send a private message to a client.
 *   quit                           Ask the server to disconnect, then exit.
 *
 * WHAT THIS EXAMPLE COMPOSES:
 *   clm_net       TCP setup, socket I/O, nonblocking mode, readiness polling
 *   clm_channel   A bounded, thread-safe queue of integer message IDs
 *   clm_bpa       Storage used by the CLM ID registry and string module
 *   clm_string    String support used by CLP's ID-backed option dictionary
 *   clm_id        Shared ID-registry state required by CLP
 *   clm_clp       The intended command-line option parsing integration point
 *
 * CLM modules provide reusable mechanisms; this example supplies the chat
 * policy and protocol. A CLM clm_id is an application-visible integer handle,
 * not a pointer and not necessarily an operating-system handle. The socket
 * handle is understood by clm_net; the channel handle is understood by
 * clm_channel. The numeric value alone does not provide ownership, lifetime,
 * or thread-safety for the resource it identifies.
 *
 * There are two execution roles, and each owns different work:
 *   1. main() owns the socket, protocol state, incoming/outgoing byte buffers,
 *      and the loop that decides when to read, write, and shut down.
 *   2. A C11 thread owns blocking console input. It parses each line and
 *      publishes a copied chat_packet for main() to consume.
 * The console thread never touches the socket. This matters because fgets()
 * can wait indefinitely for a person to finish typing; if it ran in the network
 * loop, the client could not receive a KICKED response while waiting at the
 * prompt. Conversely, the network loop never waits for a new console command.
 *
 * FOLLOW ONE OUTGOING MESSAGE:
 *   fgets() -> local command parser -> copied chat_packet
 *   -> synchronized mailbox slot -> clm_channel carries the slot's integer ID
 *   -> main() receives and copies the packet -> SEND wire line
 *   -> bounded output byte queue -> one or more clm_net_send() calls.
 * The return trip is independent: clm_net_receive() yields arbitrary TCP byte
 * chunks, the client assembles LF-terminated lines, and the protocol parser
 * displays the resulting message. TCP does not preserve application lines.
 *
 * The channel does NOT carry a chat_packet struct or its text. CLM channels
 * carry clm_id values. chat_common.h implements a separate, mutex-protected
 * registry of packet slots: the producer copies a packet into a free slot and
 * queues that slot's ID; the consumer copies it back out and releases the slot.
 * The channel synchronizes its queue, while the mailbox mutex synchronizes the
 * packet storage. Neither primitive replaces the other.
 *
 * The wire protocol is plain, line-based text and is shared with the server;
 * see ../chat_common.h for its grammar, framing limits, and ownership rules.
 * It is a teaching protocol, NOT an authenticated, encrypted, or hostile-
 * network-safe chat protocol. HELLO proves no identity and TCP provides no
 * application-level delivery acknowledgement for the messages queued here.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

/* These headers use a single-header implementation pattern for their module
 * state: declarations are visible to users, while defining the corresponding
 * IMPLEMENTATION macro in exactly one .c file emits the shared definitions.
 * This small executable has one translation unit, so main.c owns those
 * definitions. In a multi-file program, put each definition in one dedicated
 * .c file; defining it in multiple files causes duplicate linker symbols.
 *
 * The ID implementation is needed even though this client does not build its
 * own named-ID registry: clm_clp uses ID-backed CLM storage internally. CLP's
 * parser functions are inline and need no CLM_CLP_IMPLEMENTATION definition.
 * Define these switches before the first include of each relevant header. */
#define CLM_BPA_IMPLEMENTATION
#define CLM_STRING_IMPLEMENTATION
#define CLM_NET_IMPLEMENTATION
#define CLM_CHANNEL_IMPLEMENTATION
#define CLM_ID_IMPLEMENTATION

#include "clm/net.h"
#include "clm/channel.h"
#include "clm/clp.h"
#include "../chat_common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* This is application state, not a CLM object. The process owns one instance
 * for its lifetime. main() is the only owner of network fields and protocol
 * buffers; the console thread may only read stdin and submit through mailbox.
 *
 * console_gate and console_stopped form a small shutdown handshake. A producer
 * holds the mutex while checking the flag and publishing; main() holds the same
 * mutex while setting the flag. Thus no new packet can be published after
 * teardown has declared the consumer stopped. The mailbox has its own mutex
 * because it also protects slot allocation and packet copies.
 *
 * The byte-buffer invariant is 0 <= output_offset <= output_length <=
 * CHAT_OUTPUT_CAPACITY. Bytes before output_offset were accepted by the local
 * socket API; [output_offset, output_length) are still pending. This is plain
 * application memory, not a CLM channel or CLM string. */
typedef struct chat_client {
    /* A chat_common.h adapter that copies full packet values into slots while
     * sending only each slot's integer ID through the CLM channel. */
    chat_mailbox mailbox;
    /* Protects the console producer's check-and-publish against shutdown. */
    mtx_t console_gate;
    bool console_stopped;
    /* Reactor-owned protocol state. finished means stop the main loop;
     * accepted gates SEND commands until the server confirms HELLO. */
    bool finished;
    bool accepted;
    bool quit_requested;
    /* Application-chosen CLM socket ID, not a native SOCKET or file handle. */
    clm_id socket_id;
    char username[CHAT_NAME_SIZE];
    /* Incoming TCP bytes are accumulated here until a complete LF line exists. */
    char incoming[CHAT_INPUT_CAPACITY];
    size_t input_length;
    /* Serialized wire bytes waiting for clm_net_send(). */
    char outgoing[CHAT_OUTPUT_CAPACITY];
    size_t output_length;
    size_t output_offset;
} chat_client;

static chat_client client;

/** Parse and validate this example's positional command-line arguments.
 *
 * Expected invocation: "client 8080 deeper" (port followed by username).
 * The host is deliberately not an argument: this demo connects to 127.0.0.1.
 * CLM's CLP module is called to show where option parsing belongs in the
 * eventual interface, but the current CLP contract does not accept these
 * positional arguments. Therefore chat_parse_port() and chat_valid_name()
 * enforce this example's actual syntax and value rules locally.
 *
 * The CLP result is intentionally unused: this call does not validate argc,
 * argv, the port, or the username for this positional interface. The returned
 * username is copied into caller-owned storage, so it does not borrow argv.
 * @param argc Conventional process argument count.
 * @param argv Conventional process argument vector.
 * @param port Receives the validated 1..65535 TCP port.
 * @param username Receives a validated username, capacity CHAT_NAME_SIZE.
 * @return true if the complete argument list matches the example syntax.
 */
static bool client_parse_arguments(
    int argc,
    char** argv,
    uint16_t* port,
    char username[CHAT_NAME_SIZE])
{
    const clm_int clp_status = clm_clp_parse_argv(argc,
        (const char* const*)argv, UINT64_C(320002));
    (void)clp_status;
    if (argc == 3 && chat_parse_port(argv[1], port) &&
        chat_valid_name(argv[2])) {
        snprintf(username, CHAT_NAME_SIZE, "%s", argv[2]);
        return true;
    }
    return false;
}

/** Read user commands and publish copied packets for the network thread.
 *
 * This function is the entry point for the one C11 console thread. fgets() is
 * deliberately isolated here because it blocks until a line arrives; the main
 * thread must remain available to receive server messages and process socket
 * readiness while a user is thinking. The console thread does not call any
 * clm_net function and never changes reactor-owned protocol state.
 *
 * The command parser returns pointers into the temporary line buffer. Those
 * pointers are copied into packet arrays before the line is reused. The packet
 * is then copied into chat_mailbox storage, and only a slot ID is queued on the
 * CLM channel. A successful mailbox send means the local consumer can retrieve
 * a copy; it does not mean the network sent it or the server received it.
 *
 * The mutex gate makes checking console_stopped and publishing one operation
 * with respect to shutdown. It is not a lock around stdin, and it does not turn
 * the whole input thread into a lock-free operation. EOF is converted into the
 * same exit packet as the explicit quit command so redirected input can end
 * the client cleanly.
 * @return C11 thread success status.
 */
static int client_console_main(void* opaque)
{
    (void)opaque;
    char line[CHAT_LINE_SIZE];
    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (strchr(line, '\n') == NULL && !feof(stdin)) {
            int c;
            /* fgets filled the buffer without reaching a line boundary. Drop
             * the rest of that physical line so its tail is not misread as a
             * second independent command on the next loop iteration. */
            while ((c = getchar()) != '\n' && c != EOF) { }
            puts("[CLIENT] Command too long");
            continue;
        }
        chat_strip_newline(line);
        chat_packet packet = {0};
        if (strcmp(line, "quit") == 0) {
            packet.kind = CHAT_CLIENT_EXIT;
        } else {
            const char* target = NULL;
            const char* message = NULL;
            if (!chat_parse_send(line, &target, &message)) {
                puts("[CLIENT] Commands: send <name|.> <message> | quit");
                continue;
            }
            packet.kind = CHAT_CLIENT_INPUT;
            snprintf(packet.target, sizeof(packet.target), "%s", target);
            snprintf(packet.text, sizeof(packet.text), "%s", message);
        }
        /* The caller owns packet and line storage; chat_mailbox_send makes
         * its own synchronized copy before this stack data can disappear. */
        mtx_lock(&client.console_gate);
        if (client.console_stopped) {
            mtx_unlock(&client.console_gate);
            return 0;
        }
        const bool ok = chat_mailbox_send(&client.mailbox,
            CHAT_CLIENT_CONSOLE, &packet);
        mtx_unlock(&client.console_gate);
        if (!ok) {
            puts("[CLIENT] Command queue full; retry");
        }
        if (packet.kind == CHAT_CLIENT_EXIT) {
            return 0;
        }
    }
    /* Input EOF acts like 'quit', useful for scripted/shell sessions. */
    chat_packet quit = {.kind = CHAT_CLIENT_EXIT};
    mtx_lock(&client.console_gate);
    if (!client.console_stopped) {
        (void)chat_mailbox_send(&client.mailbox, CHAT_CLIENT_CONSOLE, &quit);
    }
    mtx_unlock(&client.console_gate);
    return 0;
}

/** Send as much of the queued wire data as the socket will currently accept.
 *
 * A successful clm_net_send() may accept fewer bytes than requested. Advance
 * output_offset only by the reported count, then retry the remaining suffix.
 * WOULD_BLOCK is normal for a nonblocking socket: preserve both indices and
 * let a later WRITE-ready notification resume the exact unsent bytes. Returning
 * true on WOULD_BLOCK means "nothing fatal happened," not "the queue is empty."
 * Once all bytes have been accepted by the local socket API, reset both indices
 * so future lines can reuse the storage from the beginning.
 *
 * Socket acceptance is not remote delivery: it does not prove that TCP has
 * acknowledged the bytes or that the chat server parsed the line.
 * @return true when successful or temporarily blocked; false on fatal send.
 */
static bool client_flush(void)
{
    while (client.output_offset < client.output_length) {
        size_t sent = 0u;
        clm_net_status status = clm_net_send(client.socket_id,
            client.outgoing + client.output_offset,
            client.output_length - client.output_offset, &sent);
        if (status == CLM_NET_WOULD_BLOCK) {
            return true;
        }
        if (status != CLM_NET_SUCCESS || sent == 0u) {
            return false;
        }
        client.output_offset += sent;
    }
    client.output_length = 0u;
    client.output_offset = 0u;
    return true;
}

/** Interpret one complete server-to-client protocol line.
 *
 * Framing has already been handled by client_read(): line is a local mutable
 * NUL-terminated copy with the LF delimiter removed. This function performs
 * only application-level dispatch. ACCEPTED opens the gate for outgoing SEND
 * commands; KICKED ends the reactor loop; MSG and NOTICE are displayed. An
 * unknown line is reported rather than interpreted as a console command.
 *
 * For MSG, the first space after the sender separates sender from message.
 * Replacing it with NUL creates two C strings in the same temporary buffer,
 * without allocating another string or changing the received network buffer.
 * @param line Mutable LF-free line owned by this invocation.
 * @note Displaying a message does not execute it or treat it as a command.
 */
static void client_process_line(char* line)
{
    if (strcmp(line, "ACCEPTED") == 0) {
        client.accepted = true;
        printf("[CLIENT] Accepted to the server as %s\n", client.username);
    } else if (strcmp(line, "KICKED") == 0) {
        puts("[CLIENT] Server kicked you out");
        client.finished = true;
    } else if (strncmp(line, "MSG ", 4u) == 0) {
        char* space = strchr(line + 4, ' ');
        if (space != NULL) {
            *space = '\0';
            printf("[%s] %s\n", line + 4, space + 1);
        }
    } else if (strncmp(line, "NOTICE ", 7u) == 0) {
        printf("[SERVER] %s\n", line + 7);
    } else {
        printf("[CLIENT] Unknown server message: %s\n", line);
    }
    fflush(stdout);
}

/** Read available TCP bytes and assemble complete protocol lines.
 *
 * TCP is a byte stream, not a line or message transport. A receive may contain
 * part of one line, several lines, or a line plus the start of the next. The
 * persistent incoming buffer therefore stores only the unfinished suffix;
 * each LF completes one line, which is copied to a temporary C string for
 * protocol handling before the accumulator is reused.
 *
 * The loop keeps receiving until the nonblocking socket says WOULD_BLOCK, so
 * all currently available data gets a chance to progress in this reactor turn.
 * A CR immediately before LF is stripped to accept CRLF peers as well as LF.
 * Embedded NUL is rejected because C string operations would otherwise see a
 * shorter line than the bytes actually received. A line that exceeds the
 * protocol's fixed bound is rejected instead of truncated into another command.
 * A KICKED line returns promptly after setting finished, even though stdin may
 * still be blocked in the other thread.
 * @return true when reading can continue; false on close, fatal error, or bad framing.
 */
static bool client_read(void)
{
    unsigned char bytes[2048];
    for (;;) {
        size_t count = 0u;
        clm_net_status status = clm_net_receive(client.socket_id,
            bytes, sizeof(bytes), &count);
        if (status == CLM_NET_WOULD_BLOCK) {
            return true;
        }
        if (status != CLM_NET_SUCCESS || count == 0u) {
            if (!client.quit_requested && !client.finished) {
                puts("[CLIENT] Disconnected from server");
            }
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            const unsigned char c = bytes[i];
            if (c == 0u) {
                return false;
            }
            if (c == '\n') {
                size_t n = client.input_length;
                if (n != 0u && client.incoming[n - 1u] == '\r') {
                    --n;
                }
                client.incoming[n] = '\0';
                char line[CHAT_LINE_SIZE];
                memcpy(line, client.incoming, n + 1u);
                client.input_length = 0u;
                client_process_line(line);
                if (client.finished) {
                    return true;
                }
            } else if (client.input_length >= CHAT_LINE_SIZE - 1u) {
                return false;
            } else {
                client.incoming[client.input_length++] = (char)c;
            }
        }
    }
}

/** Convert queued console packets into bounded outgoing protocol lines.
 *
 * This is the handoff point between application command values and the wire
 * protocol. The console thread never formats into the socket buffer directly;
 * main() receives each packet, checks client state, and serializes it here.
 * Processing is capped at 32 packets per call so a continuously busy producer
 * cannot keep the network loop from polling the socket indefinitely.
 *
 * The server must first answer HELLO with ACCEPTED. Commands received earlier
 * are consumed but not sent, which keeps protocol order simple rather than
 * buffering arbitrary user input before registration. chat_queue_line() copies
 * a complete LF-terminated line into the application byte queue; a later call
 * to client_flush() performs the actual socket I/O and handles partial sends.
 * @note Queue success is local buffering only, not server receipt or processing.
 */
static void client_drain_console(void)
{
    chat_packet packet;
    for (unsigned int i = 0; i < 32u &&
        chat_mailbox_receive(&client.mailbox, CHAT_CLIENT_CONSOLE, &packet); ++i) {
        if (packet.kind == CHAT_CLIENT_EXIT) {
            client.quit_requested = true;
            (void)chat_queue_line(client.outgoing,
                &client.output_length, &client.output_offset, "QUIT\n");
            break;
        }
        if (packet.kind == CHAT_CLIENT_INPUT) {
            if (!client.accepted) {
                puts("[CLIENT] Waiting for server acceptance");
                continue;
            }
            char frame[CHAT_LINE_SIZE];
            int n = snprintf(frame, sizeof(frame), "SEND %s %s\n",
                packet.target, packet.text);
            if (n <= 0 || (size_t)n >= sizeof(frame) ||
                !chat_queue_line(client.outgoing,
                    &client.output_length, &client.output_offset, frame)) {
                puts("[CLIENT] Outgoing buffer full; message not queued");
            }
        }
    }
}

/** Initialize TCP, connect, then run the client's socket reactor.
 *
 * The sequence is: initialize the platform networking layer, parse the fixed
 * loopback address, create the application-ID socket, connect, and switch the
 * connected socket to nonblocking mode. Connect is intentionally blocking to
 * keep this teaching example small; after it succeeds, no socket operation in
 * the polling loop should wait for peer data or transmit-buffer space.
 *
 * The client sends HELLO before accepting user SEND commands. The console
 * thread is started only after the connection is ready. The main thread then
 * alternates between draining a bounded batch of console packets, polling for
 * READ/WRITE readiness, reading complete lines, and flushing pending output.
 * Polling is the bridge between CLM's readiness notification and application
 * policy: the poll result tells us which operation is worth attempting, while
 * client_read() and client_flush() still handle closure, partial progress, and
 * WOULD_BLOCK safely.
 *
 * A 50 ms poll timeout is a responsiveness/idle-wakeup choice for this example;
 * it is not a protocol timeout and does not prove the server is alive. QUIT
 * stops the loop only after the queued QUIT bytes have left the local buffer.
 * Cleanup first closes the console publication gate, then releases the socket
 * and CLM networking state. The channel and mailbox are owned by main() and
 * are closed/destroyed by main() after this function returns.
 * @param port Parsed and validated remote TCP port.
 * @return EXIT_SUCCESS for a clean exit, EXIT_FAILURE for network errors.
 */
static int client_run(uint16_t port)
{
    clm_net_address address;
    if (clm_net_initialize() != CLM_NET_SUCCESS ||
        clm_net_address_parse("127.0.0.1", port, &address) != CLM_NET_SUCCESS ||
        clm_net_socket_create(client.socket_id, CLM_NET_IPV4, CLM_NET_TCP) !=
            CLM_NET_SUCCESS) {
        fprintf(stderr, "[CLIENT] Unable to initialize TCP\n");
        return EXIT_FAILURE;
    }
    /* Connecting to localhost in blocking mode keeps setup simple. Once
     * connected, nonblocking mode ensures the interactive reactor can continue
     * servicing input and output without waiting for one socket operation. */
    if (clm_net_connect(client.socket_id, &address) != CLM_NET_SUCCESS ||
        clm_net_socket_set_nonblocking(client.socket_id, true) != CLM_NET_SUCCESS) {
        fprintf(stderr, "[CLIENT] Connection failed (native error: %d)\n",
            clm_net_last_native_error());
        (void)clm_net_socket_close(client.socket_id);
        (void)clm_net_finalize();
        return EXIT_FAILURE;
    }

    char greeting[CHAT_LINE_SIZE];
    snprintf(greeting, sizeof(greeting), "HELLO %s\n", client.username);
    if (!chat_queue_line(client.outgoing, &client.output_length,
            &client.output_offset, greeting)) {
        return EXIT_FAILURE;
    }
    printf("[CLIENT] Connected to 127.0.0.1:%u; requesting name '%s'\n",
        (unsigned int)port, client.username);
    puts("Commands: send <name|.> <message> | quit");
    fflush(stdout);

    /* Only the input function is placed on another thread. Detaching avoids a
     * join that could wait forever: ISO C11 provides no portable cancellation
     * for a thread blocked in fgets(). The shutdown gate prevents it from
     * publishing after the reactor has finished; process exit handles the
     * still-blocked stdin call in main() after all CLM resources are released. */
    thrd_t console_thread;
    if (thrd_create(&console_thread, client_console_main, NULL) != thrd_success) {
        fprintf(stderr, "[CLIENT] Unable to start console reader\n");
        return EXIT_FAILURE;
    }
    (void)thrd_detach(console_thread);

    int exit_code = EXIT_SUCCESS;
    /* This is the reactor: it owns every socket operation and all network
     * protocol state. Each iteration gives console traffic a bounded turn,
     * asks CLM which socket operations may progress, and then performs only
     * the operations reported ready. */
    while (!client.finished) {
        client_drain_console();
        /* Always watch for peer data. Watch for writability only while bytes
         * remain queued; otherwise many sockets are continuously writable and
         * polling WRITE with no pending work would cause needless wakeups. */
        clm_net_poll_entry entry = {
            client.socket_id,
            CLM_NET_EVENT_READ | (client.output_length > client.output_offset
                ? CLM_NET_EVENT_WRITE : 0u),
            0u
        };
        size_t ready = 0u;
        clm_net_status status = clm_net_poll(&entry, 1u, 50u, &ready);
        if (status != CLM_NET_SUCCESS && status != CLM_NET_TIMEOUT) {
            fprintf(stderr, "[CLIENT] Socket polling failed: %d\n", (int)status);
            exit_code = EXIT_FAILURE;
            break;
        }
        /* Read before writing so inbound control messages, especially KICKED,
         * are handled promptly even when there is also outgoing data. */
        if (entry.revents & CLM_NET_EVENT_READ) {
            if (!client_read()) {
                break;
            }
        }
        if (entry.revents & (CLM_NET_EVENT_ERROR | CLM_NET_EVENT_HANGUP)) {
            if (!client.finished && !client.quit_requested) {
                puts("[CLIENT] Connection closed");
            }
            break;
        }
        if (entry.revents & CLM_NET_EVENT_WRITE) {
            if (!client_flush()) {
                exit_code = EXIT_FAILURE;
                break;
            }
        }
        if (client.quit_requested && client.output_length == 0u) {
            puts("[CLIENT] Disconnected");
            break;
        }
    }

    /* Gate prevents a detached console thread from publishing after cleanup
     * begins. Its blocked fgets() is not portably cancellable by ISO C11, so we
     * do not wait for it here. Returning from main() will end that thread as the
     * process exits, after this function has released its networking resources. */
    mtx_lock(&client.console_gate);
    client.console_stopped = true;
    mtx_unlock(&client.console_gate);
    (void)clm_net_socket_close(client.socket_id);
    (void)clm_net_finalize();
    return exit_code;
}

int main(int argc, char** argv)
{
    /* Validate before initializing resources so a usage error has no CLM or
     * operating-system state to clean up. The parsed username is copied into
     * the long-lived client record; argv remains owned by the C runtime. */
    uint16_t port = 0u;
    char username[CHAT_NAME_SIZE];
    if (!client_parse_arguments(argc, argv, &port, username)) {
        fprintf(stderr, "Usage: %s <port> <name>\n", argv[0]);
        fprintf(stderr, "Example: %s 8080 deeper\n", argv[0]);
        return EXIT_FAILURE;
    }
    memset(&client, 0, sizeof(client));
    client.socket_id = CHAT_CLIENT_SOCKET_ID;
    snprintf(client.username, sizeof(client.username), "%s", username);
    /* A mailbox and a channel are separate resources. The mailbox stores copied
     * chat_packet values under a mutex; the CLM channel stores integer slot
     * IDs under its own synchronization. Initialize both before starting the
     * producer thread. The short-circuit expression stops at the first failed
     * step, so this teaching example reports initialization failure together. */
    if (!chat_mailbox_init(&client.mailbox) ||
        mtx_init(&client.console_gate, mtx_plain) != thrd_success ||
        clm_channel_create(CHAT_CLIENT_CONSOLE, 64u) !=
            CLM_CHANNEL_OPERATION_SUCCESS) {
        fprintf(stderr, "[CLIENT] Unable to initialize CLM command channel\n");
        return EXIT_FAILURE;
    }
    /* client_run() has stopped accepting console publications before it
     * returns. Close the queue, destroy it only after users are done, then
     * release the mailbox mutex and slot registry. Closing a CLM channel does
     * not free mailbox packets; destroying the mailbox does not destroy CLM. */
    const int exit_code = client_run(port);
    (void)clm_channel_close(CHAT_CLIENT_CONSOLE);
    (void)clm_channel_destroy(CHAT_CLIENT_CONSOLE);
    chat_mailbox_destroy(&client.mailbox);
    /* fgets() may hold the C runtime's stdin lock while blocked. On some
     * runtimes, returning normally from main() can make exit-time stream flush
     * wait for that lock, even though this console thread is intentionally
     * detached. At this point CLM's channel, mailbox, and network resources are
     * already released, so flush our explicit output and terminate immediately
     * rather than joining or trying to cancel a portable, uncancellable read. */
    fflush(stdout);
    fflush(stderr);
    _Exit(exit_code);
}
