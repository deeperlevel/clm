# CLM Net

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md), [clm_string](clm_string.md)
- **External libraries / platform facilities (C reference):** Platform socket APIs (POSIX/WinSock), without imposing a cross-language socket library.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

**Current protocol composition:** Raw transport remains separate from HTTP server/client facades, WebSocket codecs and QUIC connection handling. `clm_net_quic` takes datagram I/O supplied by an external event loop rather than owning sockets.

## Purpose

TCP/UDP networking primitives for addresses, sockets and readiness-based I/O.

## Capabilities and operations

- Initialize networking; create, bind, listen, connect, accept, send and receive on sockets.
- Resolve addresses and inspect local/remote endpoints.
- Use nonblocking mode and poll/readiness operations for scalable event handling.
- Maintain explicit socket ownership and handle would-block, connect-in-progress, timeout and closure as distinct states.

## Detailed module contract

**Module:** `clm_net`  
**License:** CC0 1.0 Universal  
**Scope:** Portable low-level TCP and UDP networking for higher-level CLM services.

## 1. Purpose and boundaries

`clm_net` provides the essential transport primitives from which TCP-based HTTP servers, HTTP clients, WebSocket transports, and other network protocols can be implemented. It is **not** an HTTP server, an HTTP client, a WebSocket implementation, a TLS stack, a DNS cache, or an event-loop scheduler.

The interface is meant to be implemented *natively* in each CLM language. Bindings may use their operating system socket APIs. Other languages can use their corresponding standard or native networking facilities while preserving the behavior below.

The base socket layer uses shared IDs from [clm_base](clm_base.md) and exchanges bytes rather than owning BPA payloads. Optional text adapters may use [clm_string](clm_string.md). Network transport does not require the scheduling, channels or service-container modules.

## 2. Resource and ownership model

Each open socket is associated with a caller-chosen `clm_id`. Socket IDs are unique among currently registered sockets in the same process. Creating or accepting a socket transfers responsibility for closing that socket to the caller. Attempting to create or accept using an occupied ID fails without overwriting the existing socket.

The library owns the operating-system socket handle and its bookkeeping. A successful close frees that ownership. The library **never owns network payload buffers**. Buffers supplied for send are borrowed for the duration of the call; receive buffers and output structures are caller-owned.

A socket is not a message object: TCP is a **byte stream** with no preserved application-message boundaries. A single send may transfer fewer bytes than requested, and a single receive may return any available number of bytes. Applications must implement their own framing and retry/accumulation logic. UDP transports **datagrams**; each receive consumes one datagram, possibly truncated if the buffer is too small (the current base API does not report truncation separately).

### Thread-safety and closure

- The socket registry and socket lookup lifetime are synchronized; separate threads may operate on different sockets, and the operating system permits some simultaneous operations on the same socket.
- **Concurrent I/O on one socket must be coordinated by the caller.** Multiple readers of the same TCP socket consume from the same stream and cannot expect each call to receive a particular logical message.
- Closing stops new lookups of the socket ID and waits for operations already using that socket to finish. **Do not close while another thread may remain indefinitely blocked on accept/receive/poll.** First arrange for that operation to finish (for example, by nonblocking I/O, shutdown and/or application-level coordination). Close does not force-kill threads.
- Finalization is valid only after all sockets have been closed and all standalone resolution/initialization operations have finished.
- Socket IDs may be reused after close. A stale numeric ID can accidentally refer to a new socket with that ID; application code must manage ID lifetimes appropriately.

## 3. Common result model

| Result | Meaning |
|---|---|
| `CLM_NET_SUCCESS` | Operation completed successfully. |
| `CLM_NET_ERROR_INVALID_ARGUMENT` | Invalid input, type, address, or socket option. |
| `CLM_NET_ERROR_NOT_INITIALIZED` | Initialization is required before the operation. |
| `CLM_NET_ERROR_NOT_FOUND` | Socket ID is not registered or is closing. |
| `CLM_NET_ERROR_ALREADY_EXISTS` | Requested socket ID is in use. |
| `CLM_NET_ERROR_SYSTEM` | Native system call failed. |
| `CLM_NET_ERROR_RESOLUTION` | DNS/address resolution failed. |
| `CLM_NET_ERROR_BUFFER_TOO_SMALL` | A textual output or address-result array is insufficient. |
| `CLM_NET_ERROR_UNSUPPORTED` | Feature or native address family is unsupported. |
| `CLM_NET_ERROR_BUSY` | Resource cannot be finalized or closed in its current state. |
| `CLM_NET_WOULD_BLOCK` | Nonblocking operation cannot complete now; retry when ready. |
| `CLM_NET_IN_PROGRESS` | Nonblocking connection is still being established. |
| `CLM_NET_TIMEOUT` | Explicit readiness timeout expired. |
| `CLM_NET_CLOSED` | TCP peer performed orderly end-of-stream (receive returned zero). |

Native error codes are platform-specific. They must not be used as portable status codes. The module API records the most recent native error on the calling thread, including address-resolution codes. A later successful call need not clear that recorded error; the caller should inspect it immediately following a native error status.

Unless documented otherwise, outputs are unchanged on a failure. Exceptions: `resolve()` writes the number of discovered addresses and may fill a partial array on `BUFFER_TOO_SMALL`; `receive()` writes a byte count of zero for `CLOSED`; `poll()` and `wait()` write zero ready results for `TIMEOUT`.

## 4. Addresses, families, and DNS

An address contains an IP family (IPv4 or IPv6), the TCP/UDP port in **host byte order**, its binary IP address, and an IPv6 scope ID. Port `0` selects an operating-system-assigned ephemeral port when binding. Numeric literals are parsed and formatted without DNS. Hostname resolution is a separate operation that yields one or more numeric addresses.

The IPv6 scope ID is carried separately. The current C text parser accepts plain IPv6 literals, **not** interface-name zone syntax such as `%en0`; the caller may set the numeric scope ID explicitly.

`resolve(host, port, transport)` uses transport-specific hints and returns the addresses in the native resolver's order. The number and order of DNS answers can vary; callers must not assume that IPv4 comes first or that hostname resolution is instantaneous. No DNS caching or cancellation is supplied. The reference implementation resolver is blocking.

**Cross-language invariant:** Do not treat a hostname as a numeric address. Resolution and numeric parsing are distinct operations.

Optional string-ID adapters accept CLM strings, which store validated UTF-8 internally. Sending a string transmits its UTF-8 bytes without a terminating NUL, encoding conversion, or message delimiter. It remains a byte send: TCP can send only part of the text, and applications must still frame their messages. Receive operations stay byte-oriented so arbitrary binary datagrams and incomplete TCP text remain representable.

## 5. Socket lifecycle and transport operations

### Setup

1. Initialize the networking subsystem (required to activate Winsock on Windows).
2. Create an IPv4 or IPv6 TCP/UDP socket with a unique ID.
3. Optionally configure reuse-address, nonblocking mode, TCP no-delay, or IPv6-only behavior.
4. Bind to a local address where needed.

### TCP server

1. Create, bind and listen on a TCP socket.
2. Wait for the listener to be readable if using nonblocking I/O.
3. Accept an incoming connection, assigning it a new socket ID.
4. Send and receive bytes on the accepted socket. The accepted socket's nonblocking mode should be configured explicitly; inheritance is platform-dependent.
5. Optionally shut down one half of the TCP connection.
6. Close the accepted socket and eventually the listener.

### TCP client

1. Resolve the hostname if necessary.
2. Create a TCP socket for the selected address family.
3. Connect. On a nonblocking socket, `IN_PROGRESS` is expected.
4. Wait for `WRITE` or `ERROR` readiness and use `connect_finish()` to inspect the connection result; writable readiness alone does **not** imply success.
5. Send and receive bytes until the protocol ends, then close.

### UDP

1. Create a UDP socket, optionally bind to a local endpoint.
2. `send_to()` sends one datagram to the specified endpoint.
3. `receive_from()` receives one datagram and may report the source endpoint.
4. Zero-byte UDP datagrams are valid: successful receive reports zero bytes but **not** `CLOSED`.
5. The send call accepts a null payload only when length is zero. Datagram sizes are limited by the transport and platform; a too-large datagram is not split into packets by this API.

**TCP and UDP do not provide encryption by themselves.** Higher-level protocol modules must add TLS/DTLS or other security where required.

## 6. Readiness polling

`poll(entries, timeout)` accepts an array of socket IDs and READ/WRITE interests. A successful call fills each entry's observed event flags and returns the number of entries with at least one event. Additional HANGUP and ERROR flags may be reported. No message is read or written by polling.

- `timeout = 0`: instantaneous check.
- Positive timeout: wait up to the specified number of milliseconds. This portable reference implementation accepts at most `INT_MAX` milliseconds per call; larger finite values are rejected, so applications can split unusually long waits.
- `CLM_NET_WAIT_FOREVER`: wait without a deadline.
- On timeout, all observed flags and ready count become zero.
- Readable TCP sockets may be ready because the peer has closed. The caller must receive to distinguish data from end-of-stream.
- Poll timeout is a per-call relative duration; platforms can round and schedule it approximately. A signal/interruption can cause a system error rather than an automatic retry.
- On POSIX the reference implementation uses `poll()`. On Windows it uses `select()` and is subject to `FD_SETSIZE` limitations. Windows' event flags do not provide the same hangup detail as POSIX.

This is a **portable baseline**, not an epoll/kqueue/IOCP replacement. An application may use another event reactor for larger deployments without changing the basic socket abstraction.

## 7. Integration with cooperative scheduling

`clm_net` has **no dependency on an async scheduler**. It intentionally offers a nonblocking mode and explicit readiness checks so a higher-level reactor can connect them to `clm_async` or another runtime.

For example, a cooperative task can attempt a nonblocking receive, return `WAIT` if it encounters `WOULD_BLOCK`, and later be woken by an I/O reactor when the socket is readable. The reactor must handle registration, cancellation, and wakeup races; `clm_net` does not automatically wake tasks.

Do **not** call blocking connect, accept, receive, hostname resolution, or positive-timeout poll directly inside an `clm_async` step callback. Any of those can monopolize the cooperative worker. A separate blocking executor or an event-driven resolver may be necessary.

In languages with native asynchronous sockets, use those capabilities while matching the same transport contract instead of emulating POSIX operations literally.

## 8. API groups

| Group | C operations | Cross-language behavior |
|---|---|---|
| Lifecycle | `initialize`, `finalize` | Activate/release platform networking. |
| Addresses | `address_parse`, `address_format`, `resolve`; string-ID variants `address_parse_string`, `address_format_string`, `resolve_string` | Numeric IP handling and DNS. |
| Socket management | `socket_create`, `socket_close` | Open and release ID-addressed sockets. |
| Options | `socket_set_nonblocking`, `socket_set_reuse_address`, `socket_set_no_delay`, `socket_set_ipv6_only` | Configure transport behavior. |
| Servers | `bind`, `listen`, `accept` | Listen for TCP clients. |
| Clients | `connect`, `connect_finish` | Establish connection, including nonblocking completion. |
| Data | `send`, `receive`, `send_string` | Exchange raw TCP/connected-socket byte streams; optional sender accepts a CLM string ID. |
| Datagrams | `send_to`, `receive_from`, `send_to_string` | Send/receive UDP packets and numeric endpoints; optional sender accepts a CLM string ID. |
| Inspection | `socket_local_address`, `socket_peer_address`, `last_native_error` | Inspect native socket state. |
| Shutdown | `socket_shutdown` | Half-close TCP streams. |
| Readiness | `poll`, `wait` | Observe readiness, optionally blocking for a bounded time. |

## 9. Explicitly excluded high-level concerns

`clm_net` does **not** implement: HTTP parsing, HTTP/2 frames, WebSocket upgrade or framing, TLS, certificate validation, connection pooling, URL interpretation, proxy negotiation, automatic reconnect, service discovery, DNS caching, IP filtering, host-to-host authentication, or application framing. These belong to separate current CLM modules such as `clm_net_web`, `clm_net_web_client`, `clm_net_websocket`, `clm_tls`, `clm_url` and `clm_net_quic`, or to other explicitly scoped facilities.

HTTP server and client implementations should impose their own maximum header/body/message sizes, I/O deadlines, and security policies. These policies are not implied by opening a TCP socket.

## 10. Platform portability

Socket handling may use the networking facilities native to the host language and operating system. Name-resolution order, permission errors and availability of advanced socket features can differ among platforms. The shared contract distinguishes success, would-block, in-progress, timeout, closure and failure; exact system error details may remain implementation-specific.

## 11. Future extension points

The current module API is a portable baseline rather than a complete socket-option surface. The most useful additions for broader production reuse are:

- **UDP truncation reporting:** `receive_from()` currently cannot distinguish a complete datagram from one truncated to the caller's buffer capacity. Add an explicit truncation result/flag before building protocols that require reliable datagram-size validation.
- **Connection liveness and tuning:** optional keepalive controls and send/receive buffer-size queries/configuration are common needs for long-lived connections and throughput-sensitive services. Keep platform-specific option values out of the language-neutral contract.
- **Cancellation and deadlines:** blocking DNS and waits cannot currently be cancelled, and socket operations have no per-operation deadline. A reactor-facing cancellation/wakeup handle and a separately designed resolver would improve shutdown and cooperative-runtime integration.
- **Advanced transports:** multicast/broadcast controls and a pluggable native reactor (epoll/kqueue/IOCP) are useful for specialized deployments, but should remain optional extensions.
- **Higher-level adapters:** TLS, multi-address connection attempts, streaming backpressure, and HTTP/WebSocket parsers belong in separate modules.

These are extension candidates, not requirements for the current low-level TCP/UDP scope. Keep optional capabilities separate from the minimal portable socket contract.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Initialize networking; create, bind, listen, connect, accept, send and receive on sockets.
