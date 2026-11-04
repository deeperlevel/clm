# CLM Async

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

A cooperative scheduler for stackless tasks. Each task performs bounded work and returns a step result instead of holding a thread while it waits.

## Capabilities and operations

- Create and destroy schedulers; spawn task IDs with caller-managed continuation context.
- Run ready tasks cooperatively; yield, wait, complete or fail after each step.
- Wake, cancel, query and release tasks; use generation-bearing task tokens to avoid waking a reused ID.
- Keep steps nonblocking: a blocking callback stalls the scheduler thread.

## Task lifecycle

A task is identified by an ID and has a ready/running/waiting state or a terminal completed/failed/cancelled state. In pooled scheduling, a task may also be queued before assignment to a worker. **Do not infer terminal state from the numeric ordering of states.**

At each invocation, the task reports one of four step outcomes:

| Outcome | Scheduler behavior |
|---|---|
| Yield | Place the task back into the ready work queue. |
| Wait | Suspend until a wake or recorded pending wake. |
| Complete | Mark the task successfully terminal. |
| Fail | Mark the task failed and terminal. |

## Continuations and safe wakeups

This is a stackless scheduling contract: task callbacks do not preserve an implicit stack across yields. Keep continuation state in a resource the caller manages. Generation-bearing task tokens distinguish separate incarnations of a reused ID, avoiding accidental delivery of delayed wakeups to the wrong task.

**Nonblocking is behavioral, not merely threaded.** A task that performs blocking I/O defeats cooperative scheduling for the worker executing it.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- Step callbacks are stackless; the caller must preserve continuation state.

## Example use cases

Run a stateful nonblocking task until an external operation completes, waking it through a generation-safe token.
