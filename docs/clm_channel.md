# CLM Channel

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_base](clm_base.md).
- **External libraries / platform facilities (C reference):** TinyCThread (optional C11-threads compatibility backend; `CLM_USE_TINYCTHREAD`).

## Purpose

Bounded concurrent message channels for exchanging integer IDs between producers and consumers.

## Capabilities and operations

- Create FIFO channels with positive buffering capacity or zero-capacity rendezvous semantics.
- Send/receive with immediate, blocking and timeout operations; close and inspect channel state.
- After close, existing buffered messages can still be received, and new sends are rejected.
- Channels transport ID values, not payload copies or ownership: protect referenced data separately.

## Delivery and ownership

- A successful send delivers one integer ID to exactly one receiver, not a broadcast to all listeners.
- A positive capacity buffers messages in FIFO order; capacity zero pairs a sender directly with a receiver.
- Blocking, nonblocking and timed send/receive operations are distinct.
- Close rejects new sends, wakes blocked operations and permits remaining queued messages to drain.
- Destroy discards queued IDs but never deletes the resources to which the IDs refer. Application code must first stop other users.

A channel does not make an ID-addressed payload safe for concurrent mutation. Synchronization and ownership transfer of referenced resources are separate concerns.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- For a zero-capacity channel, a sender and receiver rendezvous directly. A successful receive removes one FIFO message.

## Example use cases

Route chat-message IDs from a networking loop to a message processor.
