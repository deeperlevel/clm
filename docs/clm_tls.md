# CLM tls

> Language-neutral compatibility reference.

## Relationship to the canonical contract

This legacy-prefixed document is retained for existing links. The canonical
contract is [clm_tls](tls.md).
Implementations must follow that language-neutral behavior and dependency model.


> Language-neutral module overview and behavioral contract.

## Purpose

TLS session state and encrypted socket I/O.

## Behavior and constraints

TLS session state and encrypted socket I/O.

Encrypted input, decrypted application data, and pending output are distinct buffers.
Partial native handshakes and socket writes retain state until the next readiness event.

TLS 1.2+ client and server sessions over registered CLM TCP sockets.
Client sessions verify the peer certificate and hostname by default.
Sessions keep a reference to their socket; dispose the TLS session before
closing the underlying clm_net socket. A session serializes access to itself.

Two I/O modes exist per session:
- Blocking (the default, and what the *_open functions use): calls wait on the
socket through clm_net, with timeout_ms applied to each readiness wait.
- Nonblocking (clm_tls_set_nonblocking): calls never wait. When the socket cannot
make progress they return CLM_TLS_WANT_READ or CLM_TLS_WANT_WRITE; the caller
waits for that socket event (for example by parking on a poller) and repeats
the same call. Timeouts are then the caller's responsibility.
TLS can need the opposite event from the operation: a read may need to write and a
write may need to read, so always honour the event that is requested.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_net`](net.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_tls. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_net](net.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_tls when an application needs: TLS session state and encrypted socket I/O.

A conforming workflow is conceptually:

1. Configure or acquire the module's inputs.
2. Perform an operation defined by this module and inspect its outcome.
3. Observe ownership, lifetime, ordering, and concurrency rules while using
   dependent CLM resources.
4. Release or reset owned state through the documented lifecycle operation.

## Language-neutral boundaries

Implementations may choose native types, allocation, error reporting, callbacks,
threads, and platform integrations. They must preserve the documented data
meaning, distinguishable outcomes, lifetime rules, synchronization contract,
and dependency relationships. Implementation availability and platform
support must be reported rather than silently substituted.
<!-- CLM-DOC-NEUTRAL-SCOPE:END -->
