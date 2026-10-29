#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#include "base.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdlib.h>

/* Return codes are shared by async.h and async_pool.h. */
#define CLM_ASYNC_OPERATION_SUCCESS       0
#define CLM_ASYNC_OPERATION_FAILURE      -1
#define CLM_ASYNC_OPERATION_BUSY         -2
#define CLM_ASYNC_OPERATION_QUEUE_FULL   -3
#define CLM_ASYNC_OPERATION_NOT_FOUND    -4
#define CLM_ASYNC_OPERATION_INVALID_STATE -5
#define CLM_ASYNC_OPERATION_STOPPING     -6

/** Result of ONE invocation of a stackless task callback. */
typedef enum clm_async_step_result {
    CLM_ASYNC_STEP_YIELD = 0,    /* Run again at the back of the ready queue. */
    CLM_ASYNC_STEP_WAIT = 1,     /* Wait for wakeup or a previously pending wake. */
    CLM_ASYNC_STEP_COMPLETE = 2, /* Terminal: completed successfully. */
    CLM_ASYNC_STEP_FAIL = 3      /* Terminal: failed. */
} clm_async_step_result;

/** Task state. QUEUED is reserved for pool tasks not yet assigned to a worker.
 * Existing state values (0 through 5) are retained for compatibility.
 */
typedef enum clm_async_task_state {
    CLM_ASYNC_TASK_READY = 0,
    CLM_ASYNC_TASK_RUNNING = 1,
    CLM_ASYNC_TASK_WAITING = 2,
    CLM_ASYNC_TASK_COMPLETED = 3,
    CLM_ASYNC_TASK_FAILED = 4,
    CLM_ASYNC_TASK_CANCELLED = 5,
    CLM_ASYNC_TASK_QUEUED = 6
} clm_async_task_state;

/** Never classify task states by numeric ordering: QUEUED has value 6. */
static inline bool clm_async_task_is_terminal(const clm_async_task_state state)
{
    return state == CLM_ASYNC_TASK_COMPLETED ||
           state == CLM_ASYNC_TASK_FAILED ||
           state == CLM_ASYNC_TASK_CANCELLED;
}

/** Stable identification of ONE incarnation of a task ID.
 * Generation changes when an ID is reused, so delayed I/O completions cannot
 * accidentally wake a newly spawned task bearing the same numeric ID.
 * Tokens are scoped to the scheduler/pool passed to the token APIs.
 */
typedef struct clm_async_task_token {
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

typedef struct clm_async_internal_task {
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

typedef struct clm_async_internal_scheduler {
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

#ifdef CLM_ASYNC_IMPLEMENTATION
clm_async_internal_scheduler* clm_async_internal_schedulers = NULL;
atomic_flag clm_async_internal_registry_lock = ATOMIC_FLAG_INIT;
_Atomic(clm_ulong) clm_async_internal_generation_sequence = 1;
#endif

/** Process-wide monotonically increasing task incarnation, including pools.
 * Zero means the counter is exhausted, rather than silently reusing tokens.
 */
static inline clm_ulong clm_async_internal_new_generation(void)
{
    clm_ulong old = atomic_load_explicit(
        &clm_async_internal_generation_sequence, memory_order_relaxed);
    while (old != 0 && old != (clm_ulong)~(clm_ulong)0) {
        if (atomic_compare_exchange_weak_explicit(
            &clm_async_internal_generation_sequence, &old, old + 1,
            memory_order_relaxed, memory_order_relaxed))
            return old;
    }
    return 0;
}

static inline void clm_async_internal_lock_registry(void)
{
    while (atomic_flag_test_and_set_explicit(&clm_async_internal_registry_lock,
        memory_order_acquire)) { }
}

static inline void clm_async_internal_unlock_registry(void)
{
    atomic_flag_clear_explicit(&clm_async_internal_registry_lock,
        memory_order_release);
}

static inline clm_async_internal_scheduler* clm_async_internal_find_scheduler(
    const clm_id scheduler_id)
{
    clm_async_internal_scheduler* found = NULL;
    clm_async_internal_lock_registry();
    for (clm_async_internal_scheduler* item = clm_async_internal_schedulers;
        item != NULL; item = item->next)
        if (item->id == scheduler_id) {
            found = item;
            break;
        }
    clm_async_internal_unlock_registry();
    return found;
}

static inline clm_int clm_async_internal_scheduler_create_auto(
    clm_id* out_scheduler_id)
{
    if (out_scheduler_id == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_internal_lock_registry();
    for (clm_id candidate = ~(clm_id)0; candidate != 0; --candidate) {
        bool used = false;
        for (clm_async_internal_scheduler* item = clm_async_internal_schedulers;
            item != NULL; item = item->next)
            if (item->id == candidate) {
                used = true;
                break;
            }
        if (used)
            continue;
        clm_async_internal_scheduler* scheduler =
            (clm_async_internal_scheduler*)calloc(1, sizeof(*scheduler));
        if (scheduler == NULL) {
            clm_async_internal_unlock_registry();
            return CLM_ASYNC_OPERATION_FAILURE;
        }
        scheduler->id = candidate;
        scheduler->next_generation = 1;
        scheduler->next = clm_async_internal_schedulers;
        clm_async_internal_schedulers = scheduler;
        *out_scheduler_id = candidate;
        clm_async_internal_unlock_registry();
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    clm_async_internal_unlock_registry();
    return CLM_ASYNC_OPERATION_FAILURE;
}

static inline clm_async_internal_task* clm_async_internal_find_task(
    clm_async_internal_scheduler* scheduler,
    const clm_id task_id)
{
    for (clm_async_internal_task* task = scheduler->tasks; task != NULL;
        task = task->next)
        if (task->id == task_id)
            return task;
    return NULL;
}

static inline void clm_async_internal_notify(
    clm_async_internal_task* task)
{
    if (task->on_state != NULL)
        task->on_state(task->id, task->generation, task->state,
            task->on_state_context);
}

static inline void clm_async_internal_enqueue(
    clm_async_internal_scheduler* scheduler,
    clm_async_internal_task* task)
{
    task->ready_next = NULL;
    if (scheduler->ready_tail == NULL)
        scheduler->ready_head = task;
    else
        scheduler->ready_tail->ready_next = task;
    scheduler->ready_tail = task;
}

static inline void clm_async_internal_remove_ready(
    clm_async_internal_scheduler* scheduler,
    clm_async_internal_task* task)
{
    clm_async_internal_task* previous = NULL;
    for (clm_async_internal_task* current = scheduler->ready_head;
        current != NULL; current = current->ready_next) {
        if (current == task) {
            if (previous == NULL)
                scheduler->ready_head = current->ready_next;
            else
                previous->ready_next = current->ready_next;
            if (scheduler->ready_tail == current)
                scheduler->ready_tail = previous;
            current->ready_next = NULL;
            return;
        }
        previous = current;
    }
}

static inline void clm_async_internal_finish(
    clm_async_internal_task* task,
    const clm_async_task_state state)
{
    task->state = state;
    if (!task->cleanup_called) {
        task->cleanup_called = true;
        if (task->cleanup != NULL)
            task->cleanup(task->context_id);
    }
    clm_async_internal_notify(task);
}

static inline clm_int clm_async_internal_spawn(
    const clm_id scheduler_id,
    const clm_id task_id,
    const clm_async_step_fn step,
    const clm_id context_id,
    const clm_async_context_cleanup_fn cleanup,
    const clm_async_internal_state_fn on_state,
    void* on_state_context)
{
    clm_async_internal_scheduler* scheduler =
        clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (step == NULL || scheduler->destroying)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    if (clm_async_internal_find_task(scheduler, task_id) != NULL)
        return CLM_ASYNC_OPERATION_BUSY;
    const clm_ulong generation = clm_async_internal_new_generation();
    if (generation == 0)
        return CLM_ASYNC_OPERATION_FAILURE;
    clm_async_internal_task* task =
        (clm_async_internal_task*)calloc(1, sizeof(*task));
    if (task == NULL)
        return CLM_ASYNC_OPERATION_FAILURE;
    task->id = task_id;
    task->generation = generation;
    task->context_id = context_id;
    task->step = step;
    task->cleanup = cleanup;
    task->on_state = on_state;
    task->on_state_context = on_state_context;
    task->state = CLM_ASYNC_TASK_READY;
    task->next = scheduler->tasks;
    scheduler->tasks = task;
    clm_async_internal_enqueue(scheduler, task);
    clm_async_internal_notify(task);
    return CLM_ASYNC_OPERATION_SUCCESS;
}

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

/** Creates an empty scheduler identified by scheduler_id. */
static inline clm_int clm_async_scheduler_create(const clm_id scheduler_id)
{
    clm_async_internal_scheduler* scheduler =
        (clm_async_internal_scheduler*)calloc(1, sizeof(*scheduler));
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_FAILURE;
    scheduler->id = scheduler_id;
    scheduler->next_generation = 1;
    clm_async_internal_lock_registry();
    for (clm_async_internal_scheduler* item = clm_async_internal_schedulers;
        item != NULL; item = item->next)
        if (item->id == scheduler_id) {
            clm_async_internal_unlock_registry();
            free(scheduler);
            return CLM_ASYNC_OPERATION_BUSY;
        }
    scheduler->next = clm_async_internal_schedulers;
    clm_async_internal_schedulers = scheduler;
    clm_async_internal_unlock_registry();
    return CLM_ASYNC_OPERATION_SUCCESS;
}

/** Cancels unfinished tasks, invokes outstanding cleanup callbacks, and
 * removes all records. Invalid from inside a step/cleanup callback.
 */
static inline clm_int clm_async_scheduler_destroy(const clm_id scheduler_id)
{
    clm_async_internal_lock_registry();
    clm_async_internal_scheduler** link = &clm_async_internal_schedulers;
    while (*link != NULL && (*link)->id != scheduler_id)
        link = &(*link)->next;
    clm_async_internal_scheduler* scheduler = *link;
    if (scheduler == NULL) {
        clm_async_internal_unlock_registry();
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    }
    if (scheduler->running || scheduler->destroying) {
        clm_async_internal_unlock_registry();
        return CLM_ASYNC_OPERATION_BUSY;
    }
    scheduler->destroying = true;
    *link = scheduler->next;
    clm_async_internal_unlock_registry();
    clm_async_internal_task* task = scheduler->tasks;
    while (task != NULL) {
        clm_async_internal_task* next = task->next;
        if (!clm_async_task_is_terminal(task->state))
            clm_async_internal_finish(task, CLM_ASYNC_TASK_CANCELLED);
        free(task);
        task = next;
    }
    free(scheduler);
    return CLM_ASYNC_OPERATION_SUCCESS;
}

/** Adds a READY task to this scheduler. context_id is borrowed; cleanup is
 * optional. Task ID must not already be present (including terminal records).
 */
static inline clm_int clm_async_spawn(
    const clm_id scheduler_id,
    const clm_id task_id,
    const clm_async_step_fn step,
    const clm_id context_id,
    const clm_async_context_cleanup_fn cleanup)
{
    return clm_async_internal_spawn(scheduler_id, task_id, step, context_id,
        cleanup, NULL, NULL);
}

/** Runs 1..max_steps READY callbacks, FIFO. YIELD appends to the ready queue.
 * WAIT suspends until wake. Returns SUCCESS even if 0 steps were run.
 * max_steps must be >0. out_steps is optional. Must not call recursively.
 */
static inline clm_int clm_async_run(
    const clm_id scheduler_id,
    const size_t max_steps,
    size_t* out_steps)
{
    clm_async_internal_scheduler* scheduler =
        clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (max_steps == 0)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    if (scheduler->running || scheduler->destroying)
        return CLM_ASYNC_OPERATION_BUSY;
    scheduler->running = true;
    size_t steps = 0;
    while (steps < max_steps && scheduler->ready_head != NULL) {
        clm_async_internal_task* task = scheduler->ready_head;
        scheduler->ready_head = task->ready_next;
        if (scheduler->ready_head == NULL)
            scheduler->ready_tail = NULL;
        task->ready_next = NULL;
        if (task->state != CLM_ASYNC_TASK_READY)
            continue;
        task->state = CLM_ASYNC_TASK_RUNNING;
        task->callback_active = true;
        clm_async_internal_notify(task);
        const clm_async_step_result result = task->step(task->id,
            task->context_id);
        task->callback_active = false;
        ++steps;
        if (task->cancel_requested || result == CLM_ASYNC_STEP_FAIL) {
            clm_async_internal_finish(task, task->cancel_requested
                ? CLM_ASYNC_TASK_CANCELLED : CLM_ASYNC_TASK_FAILED);
        } else if (result == CLM_ASYNC_STEP_COMPLETE) {
            clm_async_internal_finish(task, CLM_ASYNC_TASK_COMPLETED);
        } else if (result == CLM_ASYNC_STEP_YIELD ||
            result == CLM_ASYNC_STEP_WAIT) {
            if (result == CLM_ASYNC_STEP_YIELD || task->pending_wake) {
                task->pending_wake = false;
                task->state = CLM_ASYNC_TASK_READY;
                clm_async_internal_enqueue(scheduler, task);
            } else {
                task->state = CLM_ASYNC_TASK_WAITING;
            }
            clm_async_internal_notify(task);
        } else {
            clm_async_internal_finish(task, CLM_ASYNC_TASK_FAILED);
        }
    }
    scheduler->running = false;
    if (out_steps != NULL)
        *out_steps = steps;
    return CLM_ASYNC_OPERATION_SUCCESS;
}

/** Signals a task without a generation check. WAITING -> READY.
 * If RUNNING or READY, remember one coalesced pending wake so a subsequent
 * WAIT transitions immediately to READY (avoids a lost wakeup).
 * For delayed or cross-thread completions prefer clm_async_wake_token().
 * COMPLETED/FAILED/CANCELLED and unknown task IDs cannot be woken.
 */
static inline clm_int clm_async_wake(
    const clm_id scheduler_id,
    const clm_id task_id)
{
    clm_async_internal_scheduler* scheduler =
        clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    clm_async_internal_task* task =
        clm_async_internal_find_task(scheduler, task_id);
    if (task == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task->state == CLM_ASYNC_TASK_WAITING) {
        task->pending_wake = false;
        task->state = CLM_ASYNC_TASK_READY;
        clm_async_internal_enqueue(scheduler, task);
        clm_async_internal_notify(task);
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    if (task->state == CLM_ASYNC_TASK_READY ||
        task->state == CLM_ASYNC_TASK_RUNNING) {
        task->pending_wake = true;
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    return CLM_ASYNC_OPERATION_INVALID_STATE;
}

/** Retrieves a task token valid for this task's specific incarnation. */
static inline clm_int clm_async_task_get_token(
    const clm_id scheduler_id,
    const clm_id task_id,
    clm_async_task_token* out_token)
{
    if (out_token == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_internal_scheduler* scheduler =
        clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    clm_async_internal_task* task =
        clm_async_internal_find_task(scheduler, task_id);
    if (task == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    const clm_async_task_token token = {task->id, task->generation};
    *out_token = token;
    return CLM_ASYNC_OPERATION_SUCCESS;
}

/** Wakes only if token.task_id AND token.generation still match an active task.
 * Return NOT_FOUND for an expired/stale token in a future implementation.
 */
static inline clm_int clm_async_wake_token(
    const clm_id scheduler_id,
    const clm_async_task_token token)
{
    clm_async_internal_scheduler* scheduler =
        clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    clm_async_internal_task* task =
        clm_async_internal_find_task(scheduler, token.task_id);
    if (task == NULL || task->generation != token.generation)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    return clm_async_wake(scheduler_id, token.task_id);
}

/** Requests cancellation. READY/WAITING cancel immediately; a RUNNING task
 * completes its current step first and is then cancelled. Never force-kill a
 * thread or invoke cleanup concurrently with a running step.
 */
static inline clm_int clm_async_cancel(
    const clm_id scheduler_id,
    const clm_id task_id)
{
    clm_async_internal_scheduler* scheduler =
        clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    clm_async_internal_task* task =
        clm_async_internal_find_task(scheduler, task_id);
    if (task == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task->state == CLM_ASYNC_TASK_RUNNING) {
        task->cancel_requested = true;
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    if (task->state == CLM_ASYNC_TASK_READY ||
        task->state == CLM_ASYNC_TASK_WAITING) {
        clm_async_internal_remove_ready(scheduler, task);
        clm_async_internal_finish(task, CLM_ASYNC_TASK_CANCELLED);
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    return CLM_ASYNC_OPERATION_INVALID_STATE;
}

/** Reads a task's state, including terminal states until release. */
static inline clm_int clm_async_get_state(
    const clm_id scheduler_id,
    const clm_id task_id,
    clm_async_task_state* out_state)
{
    if (out_state == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_internal_scheduler* scheduler =
        clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    clm_async_internal_task* task =
        clm_async_internal_find_task(scheduler, task_id);
    if (task == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    *out_state = task->state;
    return CLM_ASYNC_OPERATION_SUCCESS;
}

/** Removes only a terminal (COMPLETED, FAILED, CANCELLED) task record.
 * Cleanup already ran when the task became terminal. This does not delete
 * the caller's context when no cleanup callback was provided.
 */
static inline clm_int clm_async_task_release(
    const clm_id scheduler_id,
    const clm_id task_id)
{
    clm_async_internal_scheduler* scheduler =
        clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (scheduler->running || scheduler->destroying)
        return CLM_ASYNC_OPERATION_BUSY;
    clm_async_internal_task** link = &scheduler->tasks;
    while (*link != NULL && (*link)->id != task_id)
        link = &(*link)->next;
    clm_async_internal_task* task = *link;
    if (task == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task->callback_active || !clm_async_task_is_terminal(task->state))
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    *link = task->next;
    free(task);
    return CLM_ASYNC_OPERATION_SUCCESS;
}
