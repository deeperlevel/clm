# CLM Async Pool

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

A multiworker execution layer built on cooperative task schedulers. Workers run independent schedulers and tasks remain assigned to their worker.

## Capabilities and operations

- Configure worker count, per-worker task capacity, bounded pending queue and steps per cycle.
- Choose rejection or FIFO queueing when workers cannot accept new tasks.
- Spawn, wake, cancel, inspect, and release tasks; query worker admission state and pool statistics.
- Worker busy/available reflects admission, not simply CPU activity; blocking task code still blocks its worker.

## Worker pool behavior

Each worker owns a scheduler and can admit tasks until its configured task limit. A task assigned to one worker stays pinned there for its lifetime. A worker may be marked *busy* for admission while it continues executing previously assigned tasks.

- A fixed worker count and per-worker capacity are established at pool creation.
- When all workers cannot admit new work, the overflow policy either rejects the task or enqueues it in a bounded pending FIFO.
- Steps-per-cycle limits how much cooperative work is run before mailbox operations are processed.
- Task IDs, generation-safe wake tokens, task states, worker indices, admission state and pool-level statistics are available for observation.
- Terminal task records may be retained until explicitly released, independently of running capacity.

## Scaling boundary

Workers execute callbacks. An ordinary blocking callback still occupies its worker, even when scheduled through the pool. For many active idle connections, event-driven I/O that yields until readiness is more scalable than blocking read loops in each worker.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- Workers execute cooperative schedulers; submitting synchronous work to a worker does not make that work nonblocking.

## Related CLM modules

**Supporting modules:** [clm_async](clm_async.md).

## Example use cases

Handle multiple independent cooperative tasks across worker schedulers without giving every waiting task a dedicated thread.
