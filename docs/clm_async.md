# CLM Async

## Purpose

`clm_async` defines CLM's portable task and cooperative-scheduler contract for
asynchronous work. Its core is a **single-threaded, stackless scheduler**:
applications create a scheduler, submit resumable tasks, and explicitly drive
ready work. The core does not create threads, perform I/O, or start a background
event loop.

## Native runtime rule

`clm_async` is a cross-language abstraction, not a requirement to implement a
second coroutine runtime in every language.

- In C and other languages without native async/coroutine support, implement
  the documented task state machine and scheduler operations.
- In languages with built-in coroutines, `async`/`await`, or goroutines, the
  CLM module should **encapsulate the language's native runtime** and map its
  operations to the CLM contract. It should not create a competing scheduler
  or duplicate native coroutine machinery.
- Bindings may expose idiomatic language syntax. The shared contract is about
  task states, wakeup, cancellation, completion, and cleanup, not identical
  syntax or calling conventions.

## Task model

A task is a resumable computation identified by a scheduler-local task ID. A
stackless task callback performs one bounded step and stores its continuation
state in a caller-owned context. It returns one of:

| Step result | Meaning |
| --- | --- |
| `YIELD` | Requeue the task at the end of the ready queue. |
| `WAIT` | Suspend until an external event wakes the task. |
| `COMPLETE` | Finish successfully and enter a terminal state. |
| `FAIL` | Finish unsuccessfully and enter a terminal state. |

The scheduler does not save and restore an ordinary C call stack. A task must
return from each step and explicitly preserve the state needed for its next
step. A blocking or unbounded step blocks the scheduler and every task assigned
to it. Async I/O integrations must notify the scheduler when the awaited
operation is ready; `WAIT` does not itself monitor files, sockets, timers, or
other resources.

## Scheduler behavior

- A scheduler belongs to one owner thread. Calls are not thread-safe unless a
  binding explicitly provides an external synchronization adapter.
- `run(max_steps)` executes at most the requested number of ready steps in
  FIFO order. Bounded runs let the caller interleave scheduler work with other
  application work.
- Waking a running or ready task records one coalesced pending wake. If the
  callback then returns `WAIT`, the pending wake prevents a lost notification.
- Task IDs cannot be reused while a task record is retained. Generation-bearing
  task tokens protect delayed wakeups from accidentally targeting a later task
  that reused the same numeric ID.
- Cancellation is cooperative. A running callback is allowed to return before
  cancellation takes effect; the scheduler never forcibly terminates a thread.
- An optional context cleanup callback runs once after completion, failure,
  cancellation, or scheduler destruction. If no cleanup callback is supplied,
  the caller remains responsible for the context.
- Terminal records remain inspectable until explicitly released or the
  scheduler is destroyed.

## Operations

The C API in `clm/async.h` declares scheduler creation/destruction, task
submission, bounded execution, wakeup, cancellation, state inspection, and
terminal-record release. Its status constants distinguish success, generic
failure, busy/not-found/invalid-state conditions, queue-full, and stopping.

The C header implements the cooperative scheduler. Define
`CLM_ASYNC_IMPLEMENTATION` in exactly one C translation unit to provide its
shared registry storage. The C binding requires C11 atomics. Other bindings may
map the same operations to their native async runtime instead of using this
implementation.

## Relationship to the worker pool

The scheduler can run by itself on the application's current thread. The
optional [CLM Async Pool](clm_async_pool.md) hosts schedulers on background
workers and adds cross-thread submission and wakeup. `clm_async` does not depend
on the pool.