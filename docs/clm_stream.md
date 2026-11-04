# CLM Stream

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md), [clm_bytes](clm_bytes.md), [clm_file](clm_file.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Reusable source/sink adapters and incremental data pumps for transferring binary data.

## Capabilities and operations

- Read from memory, byte buffers, files, sockets or concatenated sources.
- Write to buffers, files, sockets, discard sinks and chunked-transfer wrappers.
- Advance transfers in bounded pump steps; report which side is waiting for readiness.
- Treat would-block distinctly from end-of-stream and failure; endpoints borrow the resources they wrap.

## Source and sink contracts

A source produces bytes and eventually reports end-of-input; a sink accepts bytes, possibly only part of what is offered. Either side may report **would block**, which means to retry when ready—not end-of-stream.

- Sources: memory, byte buffer, file, network socket and concatenated sources.
- Sinks: mutable byte buffer, file, socket, discard target and HTTP-style chunked wrapper.
- A pump transfers in bounded steps and reports whether to wait for reading or writing readiness.
- Endpoint wrappers borrow referenced buffers, sockets and file resources. Keep those resources alive for the entire transfer.

When using this with [clm_net](clm_net.md), readiness loops can avoid holding a thread while the network is idle.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Copy data incrementally from a file source to a socket sink, returning to the reactor on would-block.
