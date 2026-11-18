# CLM net_websocket

> Language-neutral compatibility reference.

## Relationship to the canonical contract

This legacy-prefixed document is retained for existing links. The canonical
contract is [clm_net_websocket](net_websocket.md).
Implementations must follow that language-neutral behavior and dependency model.


> Language-neutral module overview and behavioral contract.

## Purpose

WebSocket connection lifecycle and network transport.

## Behavior and constraints

WebSocket connection lifecycle and network transport.

HTTP upgrade, frame processing, and close-handshake state are separate phases. Outgoing
queues preserve partial writes across socket readiness events.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_net`](net.md)
- [`clm_tls`](tls.md)
- [`clm_types`](types.md)
- [`clm_websocket`](websocket.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_net_websocket. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.
- [clm_net](net.md) - required CLM capability.
- [clm_tls](tls.md) - required CLM capability.
- [clm_websocket](websocket.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_net_websocket when an application needs: WebSocket connection lifecycle and network transport.

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
