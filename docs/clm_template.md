# CLM Template

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md), [clm_bytes](clm_bytes.md), [clm_id](clm_id.md), [clm_string](clm_string.md), [clm_stringtify](clm_stringtify.md), [clm_tree](clm_tree.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

Compiles and renders lightweight text templates using named data contexts.

## Capabilities and operations

- Compile template text and reuse a parsed template for multiple renders.
- Bind string, numeric, Boolean, null and CLM tree values to context names.
- Render to byte buffers or text destinations, with measurable capacity requirements.
- Apply rendering limits and return errors on invalid template syntax or output overflows.

## Template and context lifecycle

- Compile reusable template text or use a one-shot render function.
- Construct a context containing named strings, integers, decimals, Booleans, nulls and tree-backed data.
- Optionally expose members of a tree object as top-level template names.
- Render to a mutable binary buffer or replace text in a designated string resource.
- Treat output-size limits, missing bindings, invalid syntax and other render errors distinctly.

Tree references are borrowed and must remain alive and unchanged throughout rendering. Explicitly dispose of compiled templates and contexts when finished.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Render a page from a compiled template with fields provided by a CLM tree.
