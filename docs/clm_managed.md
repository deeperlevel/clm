# CLM Managed

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** None.
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

A policy-driven coordinator for housekeeping and resource cleanup through configurable subsystem hooks.

## Capabilities and operations

- Configure manual, interval-based, threshold-based or hybrid collection policies.
- Choose caller-driven polling or a coordinator-owned worker mode where supported.
- Run configurable sweeps in ordered phases and retrieve counters/failure statistics.
- Hooks are supplied by the application: the coordinator does not implicitly discover and delete all resources.

## Collection policies

| Policy | When collection is due |
|---|---|
| Manual | Only after an explicit collection request. |
| Interval | When the configured time period elapses. |
| Threshold | When combined pending-item counts reach the configured threshold. |
| Hybrid | When either interval or threshold triggers. |

Collection can run through caller-initiated polling or an optional coordinator worker. Configurable hooks expose both a **pending count** and a **sweep** action. Collection statistics report cycles, disposed transients, containers and storages, failures and the last error.

## Hook contract

Hooks, not the coordinator, perform actual resource deletion. Hook ordering is controlled by the coordinator's subsystem phases, and an error can terminate the current cycle. Avoid re-entering the coordinator from a callback. This module does not automatically act as a garbage collector for arbitrary host-language objects.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Run housekeeping when pending resource counts exceed a configured threshold.
