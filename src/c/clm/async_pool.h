#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

#include "async.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <threads.h>

/** Placement policy when no worker is accepting new tasks. */
typedef enum clm_async_overflow_policy
{
    CLM_ASYNC_OVERFLOW_REJECT = 0, /* Return BUSY, task not accepted. */
    CLM_ASYNC_OVERFLOW_QUEUE = 1 /* Put task into bounded pool FIFO queue. */
} clm_async_overflow_policy;

/** Worker ADMISSION state, not current CPU execution state.
 * BUSY means this worker is NOT ACCEPTING NEW TASKS (manually disabled or
 * automatically at its task limit). It continues all existing task work.
 */
typedef enum clm_async_worker_state
{
    CLM_ASYNC_WORKER_AVAILABLE = 0,
    CLM_ASYNC_WORKER_BUSY = 1,
    CLM_ASYNC_WORKER_STOPPING = 2,
    CLM_ASYNC_WORKER_STOPPED = 3
} clm_async_worker_state;

/** Initial, fixed configuration of a worker pool.
 * worker_count > 0: each worker owns ONE scheduler and ONE OS thread in C.
 * max_tasks_per_worker > 0: includes READY, RUNNING, WAITING and reserved
 * incoming tasks; excludes terminal task records retained for inspection.
 * max_pending_tasks: bounded unassigned FIFO capacity; 0 disables queueing.
 * steps_per_cycle > 0: max cooperative callbacks before processing mailbox.
 * overflow_policy: REJECT or QUEUE (QUEUE requires pending capacity).
 * All fields must be initialized; count changes require future API support.
 */
typedef struct clm_async_pool_config
{
    size_t worker_count;
    size_t max_tasks_per_worker;
    size_t max_pending_tasks;
    size_t steps_per_cycle;
    clm_async_overflow_policy overflow_policy;
} clm_async_pool_config;

/** Snapshot of one pool's task counts; fields never include released tasks.
 * pending = QUEUED (unassigned); ready/running/waiting are assigned;
 * terminal = COMPLETED/FAILED/CANCELLED retained until task_release.
 */
typedef struct clm_async_pool_stats
{
    size_t pending;
    size_t ready;
    size_t running;
    size_t waiting;
    size_t terminal;
} clm_async_pool_stats;

/** Sentinel returned in out_worker_index for a QUEUED/unassigned task. */
#define CLM_ASYNC_NO_WORKER ((size_t)-1)

typedef struct clm_async_pool_internal clm_async_pool_internal;
typedef struct clm_async_pool_internal_worker clm_async_pool_internal_worker;
typedef struct clm_async_pool_internal_task clm_async_pool_internal_task;
typedef struct clm_async_pool_internal_command clm_async_pool_internal_command;

typedef enum clm_async_pool_internal_command_type
{
    CLM_ASYNC_POOL_COMMAND_SPAWN,
    CLM_ASYNC_POOL_COMMAND_WAKE,
    CLM_ASYNC_POOL_COMMAND_CANCEL,
    CLM_ASYNC_POOL_COMMAND_RELEASE
} clm_async_pool_internal_command_type;

struct clm_async_pool_internal_task
{
    clm_async_pool_internal* pool;
    clm_id id;
    clm_id internal_id;
    clm_ulong generation;
    clm_id context_id;
    clm_async_step_fn step;
    clm_async_context_cleanup_fn cleanup;
    clm_async_task_state state;
    clm_async_task_token scheduler_token;
    size_t worker_index;
    size_t pending_commands;
    bool scheduler_token_valid;
    bool pending_wake;
    bool cancel_requested;
    bool cancel_dispatched;
    bool wake_queued;
    bool cancel_queued;
    bool cleanup_called;
    bool cleanup_in_progress;
    bool counted_active;
    bool in_pending_queue;
    clm_async_pool_internal_task* next;
    clm_async_pool_internal_task* pending_next;
};

struct clm_async_pool_internal_command
{
    clm_async_pool_internal_command_type type;
    clm_async_pool_internal_task* task;
    bool stack_owned;
    bool complete;
    clm_int result;
    clm_async_pool_internal_command* next;
};

struct clm_async_pool_internal_worker
{
    clm_async_pool_internal* pool;
    size_t index;
    clm_id scheduler_id;
    thrd_t thread;
    bool thread_created;
    bool started;
    bool exited;
    bool accepting;
    size_t active_count;
    clm_async_pool_internal_command* command_head;
    clm_async_pool_internal_command* command_tail;
};

struct clm_async_pool_internal
{
    clm_id id;
    clm_async_pool_config config;
    mtx_t mutex;
    cnd_t changed;
    clm_async_pool_internal_worker* workers;
    clm_async_pool_internal_task* tasks;
    clm_async_pool_internal_task* pending_head;
    clm_async_pool_internal_task* pending_tail;
    size_t pending_count;
    size_t started_count;
    size_t failed_start_count;
    size_t next_worker;
    clm_id next_internal_id;
    clm_ulong next_generation;
    bool stopping;
    bool destroying;
    bool initializing; /* Published to reserve its ID; APIs cannot acquire yet. */
    atomic_size_t api_references;
    clm_async_pool_internal* next;
};

extern clm_async_pool_internal* clm_async_pool_internal_pools;
extern atomic_flag clm_async_pool_internal_registry_lock;


void clm_async_pool_internal_lock_registry(void);

void clm_async_pool_internal_unlock_registry(void);

clm_async_pool_internal* clm_async_pool_internal_acquire(clm_id pool_id);

void clm_async_pool_internal_release(clm_async_pool_internal* pool);

clm_async_pool_internal_task* clm_async_pool_internal_find_task(
    const clm_async_pool_internal* pool,
    clm_id task_id
);

clm_async_pool_internal_task* clm_async_pool_internal_find_internal_task(
    const clm_async_pool_internal* pool,
    clm_id internal_id
);

clm_async_pool_internal_worker*
clm_async_pool_internal_find_worker(
    const clm_async_pool_internal* pool,
    size_t worker_index
);

clm_async_pool_internal_command*
clm_async_pool_internal_command_new(
    clm_async_pool_internal_command_type type,
    clm_async_pool_internal_task* task
);

void clm_async_pool_internal_enqueue_command_locked(
    clm_async_pool_internal_worker* worker,
    clm_async_pool_internal_command* command
);

clm_async_pool_internal_command*
clm_async_pool_internal_pop_command_locked(clm_async_pool_internal_worker* worker);

size_t clm_async_pool_internal_find_available_worker_locked(const clm_async_pool_internal* pool);

clm_int clm_async_pool_internal_assign_locked(
    clm_async_pool_internal* pool,
    clm_async_pool_internal_task* task,
    size_t worker_index
);

void clm_async_pool_internal_drain_pending_locked(clm_async_pool_internal* pool);

clm_int clm_async_pool_internal_queue_task_command_locked(
    clm_async_pool_internal_task* task,
    clm_async_pool_internal_command_type type
);

void clm_async_pool_internal_remove_pending_locked(
    clm_async_pool_internal* pool,
    const clm_async_pool_internal_task* target
);

void clm_async_pool_internal_finish_unassigned(
    clm_async_pool_internal_task* task,
    clm_async_task_state terminal_state
);

clm_async_step_result clm_async_pool_internal_step(
    clm_id internal_id,
    clm_id pool_id
);

void clm_async_pool_internal_task_state_changed(
    clm_id internal_id,
    clm_ulong generation,
    clm_async_task_state state,
    void* context
);

clm_async_step_result clm_async_pool_internal_task_step(
    clm_id internal_id,
    clm_id pool_id
);

void clm_async_pool_internal_state_observer(
    clm_id internal_id,
    clm_ulong generation,
    clm_async_task_state state,
    void* context
);

void clm_async_pool_internal_task_list_remove_locked(
    clm_async_pool_internal* pool,
    const clm_async_pool_internal_task* target
);

void clm_async_pool_internal_complete_spawn_failure(clm_async_pool_internal_task* task);

void clm_async_pool_internal_process_command(
    const clm_async_pool_internal_worker* worker,
    clm_async_pool_internal_command* command
);

clm_async_pool_internal_task*
clm_async_pool_internal_find_cancel_candidate_locked(const clm_async_pool_internal_worker* worker);

bool clm_async_pool_internal_has_cancel_candidate_locked(const clm_async_pool_internal_worker* worker);

int clm_async_pool_internal_worker_main(void* argument);

clm_int clm_async_pool_internal_register(clm_async_pool_internal* pool);

void clm_async_pool_internal_unregister(const clm_async_pool_internal* pool);

bool clm_async_pool_internal_is_worker_thread(const clm_async_pool_internal* pool);

void clm_async_pool_internal_free_unstarted_pool(clm_async_pool_internal* pool);

/* --------------------------------------------------------------------------
 * Background worker pool: these are thread-safe, nonblocking control APIs,
 * except destroy, which blocks until all worker threads have been joined.
 * Worker indices are zero-based and stable until pool destruction.
 * Each task executes on ONE worker after assignment (no work stealing).
 * A worker runs task steps sequentially on its own scheduler, while different
 * workers may execute steps concurrently.
 *
 * Dispatch: round-robin scan from the pool's cursor, skipping workers that
 * cannot admit more tasks. Reserve a slot atomically, then enqueue to worker
 * mailbox. Advance cursor only on successful assignment.
 * Existing pool-level pending tasks have FIFO priority over new submissions.
 * If all workers are busy, apply overflow_policy; queued tasks are QUEUED.
 * A worker becoming available drains pool pending FIFO in round-robin order.
 *
 * Pool task IDs are unique within the pool until released; worker scheduler
 * IDs are implementation-private. context_id remains caller-supplied.
 * On success cleanup is managed by the pool; on failed spawn caller retains
 * cleanup responsibility. Cleanup's executing thread is unspecified: it can
 * be the worker, cancelling thread, or destroying thread, but NEVER overlaps
 * that task's step. Callbacks must be thread-safe and not destroy the pool.
 * Output parameters are unchanged on failure.
 * -------------------------------------------------------------------------- */

/** Creates a pool and starts config->worker_count workers.
 * Pool IDs must be unique among live pools; config must remain valid only
 * for this call (the implementation copies it).
 * @param pool_id CLM ID identifying the pool.
 * @param config Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_create(
    clm_id pool_id,
    const clm_async_pool_config* config
);

/** Stops admission, cancels unfinished tasks (including pool QUEUED tasks),
 * finishes already running callbacks, runs remaining cleanups, joins worker
 * threads, and destroys the pool. Blocking; never call from this pool's
 * callbacks or worker threads (to avoid self-join/deadlock).
 * @param pool_id CLM ID identifying the pool.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_destroy(clm_id pool_id);

/** Submits one task. Does NOT execute step synchronously.
 * Returns SUCCESS when accepted (assigned READY or unassigned QUEUED).
 * Returns BUSY if no eligible worker under REJECT, or QUEUE_FULL if QUEUE
 * is configured but pending capacity is exhausted.
 * A successful enqueue does not imply the callback has started.
 * @param pool_id CLM ID identifying the pool.
 * @param task_id CLM ID identifying the task.
 * @param step Input parameter described above.
 * @param context_id CLM ID identifying the context.
 * @param cleanup Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_spawn(
    clm_id pool_id,
    clm_id task_id,
    clm_async_step_fn step,
    clm_id context_id,
    clm_async_context_cleanup_fn cleanup
);

/** Thread-safe wake request. Assigned WAITING -> READY; RUNNING/READY gets
 * one coalesced pending wake. QUEUED tasks remember the wake until assigned.
 * Wake commands must be delivered to the OWNING worker's mailbox, not invoke
 * the owner-thread-only clm_async_wake() directly from another thread.
 * Late wakeups using a reused task_id are unsafe; prefer wake_token().
 * @param pool_id CLM ID identifying the pool.
 * @param task_id CLM ID identifying the task.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_wake(
    clm_id pool_id,
    clm_id task_id
);

/** Retrieves a task's generation-safe token. Valid even when QUEUED. 
 * @param pool_id CLM ID identifying the pool.
 * @param task_id CLM ID identifying the task.
 * @param out_token Receives the token.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_task_get_token(
    clm_id pool_id,
    clm_id task_id,
    clm_async_task_token* out_token
);

/** Preferred thread-safe wake for external I/O completion callbacks.
 * Must not affect a later incarnation of a reused task ID.
 * @param pool_id CLM ID identifying the pool.
 * @param token Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_wake_token(
    clm_id pool_id,
    clm_async_task_token token
);

/** Thread-safe cancellation request: QUEUED/READY/WAITING cancel without
 * another step; a RUNNING step is allowed to return before cancellation
 * becomes terminal. Cleanup occurs once, never concurrently with the step.
 * @param pool_id CLM ID identifying the pool.
 * @param task_id CLM ID identifying the task.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_cancel(
    clm_id pool_id,
    clm_id task_id
);

/** Reads a task's snapshot state. Includes QUEUED and terminal records. 
 * @param pool_id CLM ID identifying the pool.
 * @param task_id CLM ID identifying the task.
 * @param out_state Receives the state.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_get_state(
    clm_id pool_id,
    clm_id task_id,
    clm_async_task_state* out_state
);

/** Returns assigned zero-based worker index, or CLM_ASYNC_NO_WORKER if QUEUED.
 * Once assigned, a task's index never changes even when it is WAITING.
 * @param pool_id CLM ID identifying the pool.
 * @param task_id CLM ID identifying the task.
 * @param out_worker_index Receives the worker index.
 * @return Assigned zero-based worker index, or CLM_ASYNC_NO_WORKER if QUEUED. Once assigned, a task's index never changes even when it is WAITING.
 */
clm_int clm_async_pool_task_get_worker_index(
    clm_id pool_id,
    clm_id task_id,
    size_t* out_worker_index
);

/** Releases a terminal task record. Active / QUEUED tasks cannot be released.
 * Does not invoke cleanup a second time.
 * @param pool_id CLM ID identifying the pool.
 * @param task_id CLM ID identifying the task.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_task_release(
    clm_id pool_id,
    clm_id task_id
);

/** Manually enable/disable NEW assignments for a worker. Existing tasks
 * continue as normal. An enabled worker is still BUSY if its capacity is full.
 * worker_index is zero-based in [0, worker_count).
 * @param pool_id CLM ID identifying the pool.
 * @param worker_index Input parameter described above.
 * @param accepting Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_worker_set_accepting(
    clm_id pool_id,
    size_t worker_index,
    bool accepting
);

/** Reads the effective worker admission state (manual + capacity). 
 * @param pool_id CLM ID identifying the pool.
 * @param worker_index Input parameter described above.
 * @param out_state Receives the state.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_worker_get_state(
    clm_id pool_id,
    size_t worker_index,
    clm_async_worker_state* out_state
);

/** Reads the configured worker count. 
 * @param pool_id CLM ID identifying the pool.
 * @param out_worker_count Receives the worker count.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_worker_count(
    clm_id pool_id,
    size_t* out_worker_count
);

/** Reads aggregate state counts, for metrics and backpressure diagnostics. 
 * @param pool_id CLM ID identifying the pool.
 * @param out_stats Receives the stats.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_pool_get_stats(
    clm_id pool_id,
    clm_async_pool_stats* out_stats
);