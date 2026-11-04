# CLM Template

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md), [clm_bytes](clm_bytes.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

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
