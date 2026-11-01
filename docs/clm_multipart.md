# CLM Multipart

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Parses and generates MIME multipart bodies with explicit boundary handling.

## Capabilities and operations

- Validate multipart boundary syntax and extract boundary values from Content-Type.
- Parse parts, headers, preamble and epilogue; generate a complete multipart body.
- Use zero-copy spans into the input; callers must keep source bytes alive while accessing parsed data.
- Require CRLF framing; reject malformed boundaries and header folding; return capacity errors explicitly.

## Parsing contract

Multipart boundaries are validated, including length and allowed characters. Parsing separates preamble, part headers, part bodies and epilogue into borrowed spans referencing the caller's input. The caller supplies capacity for part/header lists; exceeding that capacity is a distinct error.

The parser accepts CRLF multipart framing and rejects malformed delimiter lines and folded headers. Multipart body generation appends to a destination buffer while preserving the destination on failure.

**Keep the original body alive** until all parsed spans are no longer used.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_bytes](clm_bytes.md).

## Example use cases

Extract uploaded form parts from a request body while retaining the raw body until parsing is finished.
