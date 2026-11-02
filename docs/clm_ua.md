# CLM Ua

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_base](clm_base.md), [clm_string](clm_string.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

Heuristic parsing of HTTP User-Agent strings.

## Capabilities and operations

- Detect likely browser and operating-system families and versions.
- Classify desktop, tablet and mobile clients and recognize common crawlers.
- Prioritize specific browser tokens over generic compatibility tokens.
- Results are advisory heuristics, not trustworthy security or identity evidence.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Detect likely browser and operating-system families and versions.
