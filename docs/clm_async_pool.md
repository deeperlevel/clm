# CLM Async Pool

## Purpose

`clm_async_pool` is an optional worker layer over [CLM Async](clm_async.md).
It assigns tasks to a fixed set of long-lived workers. In the C design, each
worker owns one `clm_async` scheduler and one operating-system thread. A worker
runs its task steps sequentially; different workers may run steps concurrently.

The pool is not required for using `clm_async`: applications may instead drive
one scheduler directly on their own thread. The pool does not turn a blocking
callback into nonblocking I/O and cannot forcibly stop a hung callback.

## Native runtime rule

In C and languages without an appropriate native runtime, implement the worker
pool with worker threads and per-worker schedulers as specified below. In a
language with native coroutines, `async`/`await`, goroutines, or executor/thread
pool facilities, `clm_async_pool` should **encapsulate those facilities**. It
should not create a competing async runtime. A binding may map pool creation,
submission, wakeup, cancellation, and status reporting to its native APIs while
preserving the observable CLM contract.

## Configuration

| Setting | Contract |
| --- | --- |
| `worker_count` | Positive, fixed number of workers for the pool lifetime. |
| `max_tasks_per_worker` | Positive admission limit covering ready, running, waiting, and reserved incoming tasks; terminal records are excluded. |
| `max_pending_tasks` | Capacity of the unassigned FIFO queue. Zero disables queueing. |
| `steps_per_cycle` | Maximum task steps a worker runs before checking its cross-thread mailbox again. |
| `overflow_policy` | Reject new submissions when no worker can accept them, or queue them up to the pending limit. |

Configuration is fixed at pool creation; resizing is outside the initial
contract.

## Dispatch and worker state

Workers have an **admission** state, not an indication of whether their thread
is currently executing code. A worker is unavailable for new assignments when
it has been manually disabled or has reached its task limit. Its existing
ready, running, and waiting tasks continue normally.

For new submissions, the dispatcher scans workers from a round-robin cursor,
reserving a capacity slot atomically before enqueueing the task to that worker's
mailbox. The cursor advances only after successful assignment. Existing
pool-level pending tasks have FIFO priority and are assigned as workers become
available. Once assigned, a task remains pinned to that worker for its
lifetime; work stealing is not part of this contract.

If no worker can accept a task, the pool either rejects it with a busy status
or queues it. A full bounded queue reports queue-full. Applications should
choose limits and overload behavior appropriate to their service; an
unbounded queue is not required or implied.

## Threading and callbacks

Pool control operations are thread-safe and nonblocking except destruction,
which waits for workers to stop and join. A step callback is never concurrent
with itself, but callbacks for tasks on different workers may run concurrently.
Shared context and application data must therefore be synchronized by the
application.

Wakeups from I/O or other threads go through the pool's thread-safe mailbox to
the owning worker; callers must not invoke that worker's scheduler directly
from another thread. Generation-bearing task tokens prevent delayed wakeups
from targeting reused task IDs.

Cleanup runs exactly once and never concurrently with that task's step. Its
executing thread is not fixed: cleanup may occur on a worker, the cancelling
thread, or the thread destroying the pool. Cleanup callbacks must be safe for
those contexts and must not destroy the pool from within a worker callback.
Pool destruction stops admission, cancels unfinished work, waits for current
steps to return, runs remaining cleanups, and joins workers. It must not be
called from one of its own workers.

## Blocking and failure isolation

A callback that blocks without yielding stalls its worker. Other workers
continue, but enough blocked workers can exhaust the pool. Cooperative
cancellation cannot interrupt a callback that is currently running. A pool
therefore provides bounded concurrency, not hard isolation; use process
isolation when work must be forcibly terminated or protected from memory
corruption.

## Operations and implementation

The C implementation in `clm/async_pool.h` covers pool creation/destruction,
submission, wakeup, cancellation, state and worker inspection, statistics, and
terminal task release. Define `CLM_ASYNC_IMPLEMENTATION` and
`CLM_ASYNC_POOL_IMPLEMENTATION` in exactly one C translation unit. The C pool
uses C11 threads, mutexes, and condition variables; platforms or language
bindings without that facility may provide an equivalent native backend.