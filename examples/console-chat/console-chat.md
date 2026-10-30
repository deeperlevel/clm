# CLM Console Chat — Language-Neutral Example Guide

**Example:** Console Chat  
**Category:** Networking, concurrency, and inter-task communication  
**Applications:** Chat server and chat client  
**Purpose:** Demonstrate how independent Cross Language Modules (CLM) compose into a functioning, multi-client TCP application.

## 1. Overview

Console Chat is a command-line application consisting of a TCP server and multiple named clients. A client can send a message to the server's console or address a message to another connected client. The server can also send private messages, remove individual clients, and shut down all connections.

This is **not an HTTP application or a new networking framework**. Its value is showing how CLM's existing networking, task scheduling, channel, and data-management modules cooperate while preserving clear ownership boundaries.

The reference example uses a deliberately simple text protocol, so ports to other languages can exchange messages with one another without sharing implementation code.

## 2. CLM modules and their responsibilities

| CLM module | Server | Client | How the example uses it |
|---|---|---|---|
| **`clm_base`** | Yes | Yes | Supplies common CLM identifiers, fundamental types, and result/status conventions used by the other modules. |
| **`clm_net`** | Yes | Yes | Creates TCP sockets, accepts and establishes connections, performs nonblocking sends and receives, polls for readiness, and closes sockets. |
| **`clm_async`** | Yes | No | Defines cooperative tasks whose steps return control to the scheduler instead of indefinitely blocking a worker. |
| **`clm_async_pool`** | Yes | No | Runs the server's message-routing task on a worker pool, separately from network I/O and interactive console input. |
| **`clm_channel`** | Yes | Yes | Provides bounded, thread-safe queues for passing identifiers between independent execution contexts. |
| **`clm_bpa`** | Yes | Indirectly | Underpins ID-based resource storage, notably server-owned usernames through `clm_string`. Its resources are not shared across workers without synchronization. |
| **`clm_string`** | Yes | Indirectly | Stores registered client names as CLM-managed strings, giving each username an explicit ID and lifetime. |
| **`clm_clp`** | Command-line integration | Command-line integration | Represents CLM's command-line parsing capability: server port options and client connection arguments. Ports can provide a small application-level adapter for positional arguments if required by their CLP implementation. |

**Supporting dependencies:** An implementation of `clm_clp` may use `clm_id` and a CLM dictionary to register and retrieve parsed option names and values. Those are parser implementation details; the chat protocol itself does not use a dictionary as its message router.

**Not required:** `clm_net_http`, `clm_crypto`, `clm_hash`, and the configuration-file format parsers. Console input/output in the initial C example uses its host language's standard I/O facilities; `clm_console` is a possible future substitution, not a module the example currently demonstrates.

## 3. Application responsibilities

### Server

The server listens for TCP connections, validates the name supplied by each client, and maintains a bounded registry of active peers. It receives complete chat commands, routes them to a named recipient or to its own console, and processes administrative commands.

It uses three conceptual execution roles:

1. **Network reactor:** Owns the listener, connected sockets, peer records, and CLM-managed usernames. It polls sockets and performs all socket I/O.
2. **Console reader:** Accepts administrative commands without blocking network progress.
3. **Asynchronous router:** Receives immutable message descriptions from a channel, decides what action is required, and sends the action back to the reactor. The reactor alone applies actions to live sockets.

The C reference creates a pool with two workers but schedules one routing task. **Worker count and task count are different things**; ports are not required to create one thread per client.

### Client

The client connects to the server, registers a username, reads messages from the connection, and accepts console commands. Interactive input must not prevent it from receiving a server-issued kick or detecting a disconnected socket.

The reference client uses a console-input execution context, a network loop, and a channel between them. It does **not** need `clm_async_pool` simply because the server uses it. Each application uses only the modules that solve its own problem.

## 4. Message flow

A message addressed to another client travels conceptually through:

```text
Sending client's console
        |
        v
Client network connection (clm_net)
        |
        v
Server network reactor (clm_net)
        |
        v
Validated, copied message packet
        |
        v
Inbound clm_channel -> asynchronous routing task
        |
        v
Outbound clm_channel -> server reactor
        |
        v
Recipient's bounded output buffer
        |
        v
Recipient connection (clm_net)
        |
        v
Recipient's console
```

`clm_channel` transports **resource/slot IDs**, not arbitrary packet objects. The reference example therefore pairs its channels with a small application-owned, synchronized mailbox holding packet copies. A channel ID identifies which mailbox slot to take; the consumer copies the packet and releases the slot. The mailbox is **example application code**, not an additional CLM module.

This separation makes it possible to teach channel semantics without converting memory pointers into IDs or sharing unsynchronized BPA memory with worker threads.

## 5. Console interface

### Starting the applications

- **Server:** An optional port argument; if omitted, listen on port **8080**.
- **Client:** A server port and a chosen name, such as `8080 deeper`.
- **Networking scope:** The reference implementation connects over the local loopback interface. A different address or remote-host argument would be an extension to the example.

### Server commands

| Command | Expected behavior |
|---|---|
| `send deeper Hello!` | Send a message from the server to client `deeper`. |
| `kick deeper` | Notify `deeper` of removal and disconnect that client. |
| `quit` | Stop accepting new clients, notify/disconnect current clients, and terminate the server. |

The server displays successful joins, departures, and messages explicitly addressed to it.

### Client commands

| Command | Expected behavior |
|---|---|
| `send . Hello server!` | Send a message to the server console; `.` is the special server destination. |
| `send alice Hello Alice!` | Send a private message to the connected client named `alice`. |
| `quit` | Disconnect from the server and exit. |

On connection, the client displays an acceptance message. It displays received messages with their sender. If kicked, it displays **“Server kicked you out”** and exits.

**Message scope:** This example implements **directed messages**, not automatic broadcasts to all clients. A recipient name identifies one destination.

## 6. Shared wire protocol

All implementations must agree on the byte-level protocol even if their concurrency and storage models differ. The reference application exchanges UTF-8 text lines terminated by LF (and tolerates CRLF framing).

```text
Client -> Server: HELLO <name>
Client -> Server: SEND <target> <message>
Client -> Server: QUIT

Server -> Client: ACCEPTED
Server -> Client: MSG <sender> <message>
Server -> Client: NOTICE <message>
Server -> Client: KICKED
```

Each line above represents one line of text **followed by a newline delimiter**. The displayed angle-bracketed terms are placeholders rather than literal protocol content.

The reference limits usernames to 1–31 ASCII letters, digits, underscores, or hyphens; messages to 899 bytes; complete lines to 1,023 bytes excluding LF; and simultaneous clients to 64. These limits are useful conformance targets when reimplementing this particular example.

TCP is a **byte stream**, not a message transport. A receiver may obtain half a line, several lines, or any other segmentation. It must accumulate incomplete input, handle multiple complete commands in one read, reject invalid framing, and support partial writes.

## 7. Ownership, concurrency, and backpressure

The example deliberately teaches several CLM design rules:

- **IDs identify resources; they are not automatic locks.** Sharing an ID does not make the underlying object thread-safe.
- **One owner handles mutable BPA state.** The reactor creates, reads, and deletes usernames; the routing worker only handles copied message values.
- **Cooperative tasks must return.** Blocking terminal reads belong outside an asynchronous worker step.
- **Sockets have independent lifetimes.** Removing a CLM string does not close a socket, and closing a socket does not delete its associated string.
- **Queues and output buffers are bounded.** A slow client or busy producer must not trigger unlimited memory growth. Overflow is handled explicitly, and ordinary messages are not guaranteed delivery under overload.
- **Shutdown is a control operation.** It must remain possible even if ordinary routing queues are full; closing and resource cleanup follow a deliberate order.

A successful local send or queue insertion is **not** proof that the recipient read the message. The sample does not implement durable delivery or acknowledgments.

## 8. Cross-language porting expectations

A conforming example port should preserve the observable behaviors rather than reproduce the reference language's exact threads, structs, or helper names. In particular, it should:

- Use the language's CLM networking implementation and shared wire protocol.
- Support several clients simultaneously without one blocking input source freezing all network progress.
- Retain the same command and notification semantics, including disconnect and kick behavior.
- Respect the ID-based resource contract wherever CLM-managed resources are used.
- Define who owns mutable state and how messages cross concurrency boundaries.
- Reject malformed, oversized, or incomplete commands safely.

A useful interoperability demonstration is a server written in one CLM language with clients written in other CLM languages, all speaking the same text protocol.

## 9. Scope and limitations

Console Chat is intentionally a **teaching example**. It is not a secure Internet messaging service: registration is not authentication; messages are not encrypted; there is no persistent message history, account system, delivery acknowledgment, or automatic broadcast. The initial reference restricts connections to loopback for this reason.

**Primary lesson:** CLM modules become more useful when composed carefully—networking provides transport, channels and async tasks coordinate work, while BPA and strings give resources explicit identities and lifetimes.
