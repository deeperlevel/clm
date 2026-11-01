# CLM Math Core

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Shared math result codes and storage-adapter contract for ID-addressed numeric arrays.

## Capabilities and operations

- Install a backend adapter to count, read and replace double-precision or signed 64-bit arrays by ID.
- Validate vector dimensions and storage shapes consistently.
- Distinguish storage, dimension, invalid-value, domain, singularity and allocation failures.
- Higher math modules use these adapters instead of requiring one global object-storage implementation.

## Numeric adapter

The shared adapter reads/writes numeric sequences by CLM resource ID. It provides double-precision and signed 64-bit integer array operations, plus element counts. A math implementation reads into working arrays and writes results back via the adapter.

Supported vector helpers require the expected dimension; unrelated object types and mismatched sizes produce storage/dimension errors. Adapters should account for input and output IDs referring to the same resource.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- This module defines the shared storage-adapter contract; the port may choose its own backing numeric storage.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md).

## Example use cases

Bind math operations to an ID-managed numeric-array store implemented in the host language.
