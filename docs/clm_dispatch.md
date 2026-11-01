# CLM Dispatch

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Dispatches commands, events, queries and exceptions through registered handlers indexed by message ID.

## Capabilities and operations

- Associate one handler with each message ID and declare dependency IDs.
- Dispatch categories share core handler invocation semantics.
- Validate dependencies explicitly; event dispatch is not automatically a multi-subscriber broadcast.
- Keep the message/handler lifetime under application control.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md).

## Example use cases

Associate one handler with each message ID and declare dependency IDs.
