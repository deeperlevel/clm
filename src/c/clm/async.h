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

#include "base.h"
#include <stdbool.h>
#include <stdatomic.h>
#include <stddef.h>

/* Return codes are shared by async.h and async_pool.h. */
#define CLM_ASYNC_OPERATION_SUCCESS 0
#define CLM_ASYNC_OPERATION_FAILURE -1
#define CLM_ASYNC_OPERATION_BUSY -2
#define CLM_ASYNC_OPERATION_QUEUE_FULL -3
#define CLM_ASYNC_OPERATION_NOT_FOUND -4
#define CLM_ASYNC_OPERATION_INVALID_STATE -5
#define CLM_ASYNC_OPERATION_STOPPING -6

/** Result of ONE invocation of a stackless task callback. */
typedef enum clm_async_step_result
{
    CLM_ASYNC_STEP_YIELD = 0, /* Run again at the back of the ready queue. */
    CLM_ASYNC_STEP_WAIT = 1, /* Wait for wakeup or a previously pending wake. */
    CLM_ASYNC_STEP_COMPLETE = 2, /* Terminal: completed successfully. */
    CLM_ASYNC_STEP_FAIL = 3 /* Terminal: failed. */
} clm_async_step_result;

/** Task state. QUEUED is reserved for pool tasks not yet assigned to a worker.
 * Existing state values (0 through 5) are retained for compatibility.
 */
typedef enum clm_async_task_state
{
    CLM_ASYNC_TASK_READY = 0,
    CLM_ASYNC_TASK_RUNNING = 1,
    CLM_ASYNC_TASK_WAITING = 2,
    CLM_ASYNC_TASK_COMPLETED = 3,
    CLM_ASYNC_TASK_FAILED = 4,
    CLM_ASYNC_TASK_CANCELLED = 5,
    CLM_ASYNC_TASK_QUEUED = 6
} clm_async_task_state;

/** Never classify task states by numeric ordering: QUEUED has value 6. 
 * @param state State value to inspect or update.
 * @return Whether the condition described above holds.
 */
bool clm_async_task_is_terminal(clm_async_task_state state);

/** Stable identification of ONE incarnation of a task ID.
 * Generation changes when an ID is reused, so delayed I/O completions cannot
 * accidentally wake a newly spawned task bearing the same numeric ID.
 * Tokens are scoped to the scheduler/pool passed to the token APIs.
 */
typedef struct clm_async_task_token
{
    clm_id task_id;
    clm_ulong generation;
} clm_async_task_token;

/** Called once per scheduled step; never executed concurrently for the same task.
 * The context ID is supplied by the caller, not allocated by the scheduler.
 * Save all continuation state in a caller-managed resource (e.g. BPA block).
 * Steps must be bounded and NONBLOCKING: a blocking step blocks its worker.
 * Pooled tasks remain pinned to their assigned worker for their lifetime.
 */
typedef clm_async_step_result (*clm_async_step_fn)(
    clm_id task_id,
    clm_id context_id);

/** Optional context cleanup, invoked exactly once after the task becomes
 * terminal or a scheduler/pool is destroyed. Never concurrent with its step.
 * On successful spawn the scheduler assumes responsibility for invoking it;
 * on failed spawn the caller remains responsible for context cleanup.
 * The callback must not destroy the scheduler/pool or block indefinitely.
 */
typedef void (*clm_async_context_cleanup_fn)(clm_id context_id);

typedef void (*clm_async_internal_state_fn)(
    clm_id task_id,
    clm_ulong generation,
    clm_async_task_state state,
    void* context);

typedef struct clm_async_internal_task
{
    clm_id id;
    clm_ulong generation;
    clm_id context_id;
    clm_async_step_fn step;
    clm_async_context_cleanup_fn cleanup;
    clm_async_internal_state_fn on_state;
    void* on_state_context;
    clm_async_task_state state;
    bool pending_wake;
    bool cancel_requested;
    bool cleanup_called;
    bool callback_active;
    struct clm_async_internal_task* next;
    struct clm_async_internal_task* ready_next;
} clm_async_internal_task;

typedef struct clm_async_internal_scheduler
{
    clm_id id;
    clm_ulong next_generation;
    bool running;
    bool destroying;
    clm_async_internal_task* tasks;
    clm_async_internal_task* ready_head;
    clm_async_internal_task* ready_tail;
    struct clm_async_internal_scheduler* next;
} clm_async_internal_scheduler;

extern clm_async_internal_scheduler* clm_async_internal_schedulers;
extern atomic_flag clm_async_internal_registry_lock;
extern _Atomic(clm_ulong) clm_async_internal_generation_sequence;


/** Process-wide monotonically increasing task incarnation, including pools.
 * Zero means the counter is exhausted, rather than silently reusing tokens.
 */
clm_ulong clm_async_internal_new_generation(void);

void clm_async_internal_lock_registry(void);

void clm_async_internal_unlock_registry(void);

clm_async_internal_scheduler* clm_async_internal_find_scheduler(clm_id scheduler_id);

clm_int clm_async_internal_scheduler_create_auto(clm_id* out_scheduler_id);

clm_async_internal_task* clm_async_internal_find_task(
    const clm_async_internal_scheduler* scheduler,
    clm_id task_id
);

void clm_async_internal_notify(const clm_async_internal_task* task);

void clm_async_internal_enqueue(
    clm_async_internal_scheduler* scheduler,
    clm_async_internal_task* task
);

void clm_async_internal_remove_ready(
    clm_async_internal_scheduler* scheduler,
    const clm_async_internal_task* task
);

void clm_async_internal_finish(
    clm_async_internal_task* task,
    clm_async_task_state state
);

clm_int clm_async_internal_spawn(
    clm_id scheduler_id,
    clm_id task_id,
    clm_async_step_fn step,
    clm_id context_id,
    clm_async_context_cleanup_fn cleanup,
    clm_async_internal_state_fn on_state,
    void* on_state_context
);

/* --------------------------------------------------------------------------
 * Standalone cooperative scheduler: no thread, pool, or event loop is created.
 * The application owns this scheduler and drives it with clm_async_run().
 * All methods are owner-thread-only and NOT thread-safe. The run/destroy
 * operations are nonreentrant. Callbacks MAY spawn, wake, or cancel tasks on
 * the same scheduler, but must not recursively run or destroy it.
 * async_pool.h is optional and may host schedulers on background workers;
 * async.h does not include or require that module. Cross-thread access to a
 * scheduler hosted by a pool goes through the pool's thread-safe mailbox.
 * Task IDs need only be unique among live/retained tasks in this scheduler.
 * Terminal task records remain queryable until released or scheduler destroyed.
 * Output parameters are unchanged when an operation fails.
 * -------------------------------------------------------------------------- */

/** Creates an empty scheduler identified by scheduler_id. 
 * @param scheduler_id CLM ID identifying the scheduler.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_scheduler_create(clm_id scheduler_id);

/** Cancels unfinished tasks, invokes outstanding cleanup callbacks, and
 * removes all records. Invalid from inside a step/cleanup callback.
 * @param scheduler_id CLM ID identifying the scheduler.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_scheduler_destroy(clm_id scheduler_id);

/** Adds a READY task to this scheduler. context_id is borrowed; cleanup is
 * optional. Task ID must not already be present (including terminal records).
 * @param scheduler_id CLM ID identifying the scheduler.
 * @param task_id CLM ID identifying the task.
 * @param step Input parameter described above.
 * @param context_id CLM ID identifying the context.
 * @param cleanup Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_spawn(
    clm_id scheduler_id,
    clm_id task_id,
    clm_async_step_fn step,
    clm_id context_id,
    clm_async_context_cleanup_fn cleanup
);

/** Runs 1..max_steps READY callbacks, FIFO. YIELD appends to the ready queue.
 * WAIT suspends until wake. Returns SUCCESS even if 0 steps were run.
 * max_steps must be >0. out_steps is optional. Must not call recursively.
 * @param scheduler_id CLM ID identifying the scheduler.
 * @param max_steps Input parameter described above.
 * @param out_steps Receives the steps.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_run(
    clm_id scheduler_id,
    size_t max_steps,
    size_t* out_steps
);

/** Signals a task without a generation check. WAITING -> READY.
 * If RUNNING or READY, remember one coalesced pending wake so a subsequent
 * WAIT transitions immediately to READY (avoids a lost wakeup).
 * For delayed or cross-thread completions prefer clm_async_wake_token().
 * COMPLETED/FAILED/CANCELLED and unknown task IDs cannot be woken.
 * @param scheduler_id CLM ID identifying the scheduler.
 * @param task_id CLM ID identifying the task.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_wake(
    clm_id scheduler_id,
    clm_id task_id
);

/** Retrieves a task token valid for this task's specific incarnation. 
 * @param scheduler_id CLM ID identifying the scheduler.
 * @param task_id CLM ID identifying the task.
 * @param out_token Receives the token.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_task_get_token(
    clm_id scheduler_id,
    clm_id task_id,
    clm_async_task_token* out_token
);

/** Wakes only if token.task_id AND token.generation still match an active task.
 * Return NOT_FOUND for an expired/stale token in a future implementation.
 * @param scheduler_id CLM ID identifying the scheduler.
 * @param token Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_wake_token(
    clm_id scheduler_id,
    clm_async_task_token token
);

/** Requests cancellation. READY/WAITING cancel immediately; a RUNNING task
 * completes its current step first and is then cancelled. Never force-kill a
 * thread or invoke cleanup concurrently with a running step.
 * @param scheduler_id CLM ID identifying the scheduler.
 * @param task_id CLM ID identifying the task.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_cancel(
    clm_id scheduler_id,
    clm_id task_id
);

/** Reads a task's state, including terminal states until release. 
 * @param scheduler_id CLM ID identifying the scheduler.
 * @param task_id CLM ID identifying the task.
 * @param out_state Receives the state.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_get_state(
    clm_id scheduler_id,
    clm_id task_id,
    clm_async_task_state* out_state
);

/** Removes only a terminal (COMPLETED, FAILED, CANCELLED) task record.
 * Cleanup already ran when the task became terminal. This does not delete
 * the caller's context when no cleanup callback was provided.
 * @param scheduler_id CLM ID identifying the scheduler.
 * @param task_id CLM ID identifying the task.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_async_task_release(
    clm_id scheduler_id,
    clm_id task_id
);