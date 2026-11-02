# CLM Bytes

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_base](clm_base.md), [clm_crypto](clm_crypto.md), [clm_file](clm_file.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

Binary-safe, owned, growable byte buffers and non-owning byte views, independent of text encoding.

## Capabilities and operations

- Create, reserve, append, insert, remove, read, consume and clear owned byte buffers.
- Use borrowed read-only views for slicing, prefix/suffix checks, searching and comparison.
- Apply optional capacity limits to bound growth; use explicit wiping when handling secrets.
- Borrowed pointers/views are invalidated if their owner changes; an ordinary clear does not securely erase bytes.

## Borrowed views versus owned buffers

A view is a read-only description of existing bytes and does not own memory. A buffer is a mutable owner that can grow or consume its contents. Its logical unread window can move forward without copying every remaining byte after each read.

## Resource limits

- Zero-length values are valid; binary zero bytes inside content have no special meaning.
- Operations that grow past a configured limit fail without increasing the logical buffer size.
- Reserving/preparing writable capacity must be paired with committing the number of bytes actually produced.
- Views taken from a mutable buffer may become invalid when that buffer is changed.
- Clearing a buffer is not secure wiping; explicit wipe paths exist for sensitive data.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- Buffer storage is owned; views are borrowed. Mutations may invalidate existing views.

## Example use cases

Accumulate socket input, consume processed frames and retain only the unread byte window.
