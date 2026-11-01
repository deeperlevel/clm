#pragma once

/*
 * =============================================================================
 * CLM â€” Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain â€” CC0 1.0 Universal
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

#include "async.hpp"
#include <stdbool.h>
#include <stddef.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

/** Placement policy when no worker is accepting new tasks. */
typedef enum clm_async_overflow_policy {
    CLM_ASYNC_OVERFLOW_REJECT = 0, /* Return BUSY, task not accepted. */
    CLM_ASYNC_OVERFLOW_QUEUE = 1   /* Put task into bounded pool FIFO queue. */
} clm_async_overflow_policy;

/** Worker ADMISSION state, not current CPU execution state.
 * BUSY means this worker is NOT ACCEPTING NEW TASKS (manually disabled or
 * automatically at its task limit). It continues all existing task work.
 */
typedef enum clm_async_worker_state {
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
typedef struct clm_async_pool_config {
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
typedef struct clm_async_pool_stats {
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

typedef enum clm_async_pool_internal_command_type {
    CLM_ASYNC_POOL_COMMAND_SPAWN,
    CLM_ASYNC_POOL_COMMAND_WAKE,
    CLM_ASYNC_POOL_COMMAND_CANCEL,
    CLM_ASYNC_POOL_COMMAND_RELEASE
} clm_async_pool_internal_command_type;

struct clm_async_pool_internal_task {
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

struct clm_async_pool_internal_command {
    clm_async_pool_internal_command_type type;
    clm_async_pool_internal_task* task;
    bool stack_owned;
    bool complete;
    clm_int result;
    clm_async_pool_internal_command* next;
};

struct clm_async_pool_internal_worker {
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

struct clm_async_pool_internal {
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

#ifdef CLM_ASYNC_POOL_IMPLEMENTATION
clm_async_pool_internal* clm_async_pool_internal_pools = NULL;
atomic_flag clm_async_pool_internal_registry_lock = ATOMIC_FLAG_INIT;
#endif

static inline void clm_async_pool_internal_lock_registry(void)
{
    while (atomic_flag_test_and_set_explicit(
        &clm_async_pool_internal_registry_lock, memory_order_acquire)) { }
}

static inline void clm_async_pool_internal_unlock_registry(void)
{
    atomic_flag_clear_explicit(&clm_async_pool_internal_registry_lock,
        memory_order_release);
}

static inline clm_async_pool_internal* clm_async_pool_internal_acquire(
    const clm_id pool_id)
{
    clm_async_pool_internal* found = NULL;
    clm_async_pool_internal_lock_registry();
    for (clm_async_pool_internal* pool = clm_async_pool_internal_pools;
        pool != NULL; pool = pool->next)
        if (pool->id == pool_id && !pool->initializing) {
            atomic_fetch_add_explicit(&pool->api_references, 1,
                memory_order_relaxed);
            found = pool;
            break;
        }
    clm_async_pool_internal_unlock_registry();
    return found;
}

static inline void clm_async_pool_internal_release(
    clm_async_pool_internal* pool)
{
    /* Hold the same mutex used by destroy's refcount wait before decreasing
     * the count: destroy must not free this mutex before we're finished.
     */
    mtx_lock(&pool->mutex);
    atomic_fetch_sub_explicit(&pool->api_references, 1, memory_order_release);
    cnd_broadcast(&pool->changed);
    mtx_unlock(&pool->mutex);
}

static inline clm_async_pool_internal_task* clm_async_pool_internal_find_task(
    clm_async_pool_internal* pool,
    const clm_id task_id)
{
    for (clm_async_pool_internal_task* task = pool->tasks; task != NULL;
        task = task->next)
        if (task->id == task_id)
            return task;
    return NULL;
}

static inline clm_async_pool_internal_task* clm_async_pool_internal_find_internal_task(
    clm_async_pool_internal* pool,
    const clm_id internal_id)
{
    for (clm_async_pool_internal_task* task = pool->tasks; task != NULL;
        task = task->next)
        if (task->internal_id == internal_id)
            return task;
    return NULL;
}

static inline clm_async_pool_internal_worker*
clm_async_pool_internal_find_worker(
    clm_async_pool_internal* pool,
    const size_t worker_index)
{
    return worker_index < pool->config.worker_count
        ? &pool->workers[worker_index] : NULL;
}

static inline clm_async_pool_internal_command*
clm_async_pool_internal_command_new(
    const clm_async_pool_internal_command_type type,
    clm_async_pool_internal_task* task)
{
    clm_async_pool_internal_command* command =
        (clm_async_pool_internal_command*)calloc(1, sizeof(*command));
    if (command != NULL) {
        command->type = type;
        command->task = task;
    }
    return command;
}

static inline void clm_async_pool_internal_enqueue_command_locked(
    clm_async_pool_internal_worker* worker,
    clm_async_pool_internal_command* command)
{
    command->next = NULL;
    if (worker->command_tail == NULL)
        worker->command_head = command;
    else
        worker->command_tail->next = command;
    worker->command_tail = command;
    if (command->task != NULL)
        ++command->task->pending_commands;
    cnd_broadcast(&worker->pool->changed);
}

static inline clm_async_pool_internal_command*
clm_async_pool_internal_pop_command_locked(
    clm_async_pool_internal_worker* worker)
{
    clm_async_pool_internal_command* command = worker->command_head;
    if (command != NULL) {
        worker->command_head = command->next;
        if (worker->command_head == NULL)
            worker->command_tail = NULL;
        command->next = NULL;
    }
    return command;
}

static inline size_t clm_async_pool_internal_find_available_worker_locked(
    clm_async_pool_internal* pool)
{
    for (size_t offset = 0; offset < pool->config.worker_count; ++offset) {
        const size_t index = (pool->next_worker + offset) %
            pool->config.worker_count;
        clm_async_pool_internal_worker* worker = &pool->workers[index];
        if (worker->started && !worker->exited && worker->accepting &&
            worker->active_count < pool->config.max_tasks_per_worker)
            return index;
    }
    return CLM_ASYNC_NO_WORKER;
}

static inline clm_int clm_async_pool_internal_assign_locked(
    clm_async_pool_internal* pool,
    clm_async_pool_internal_task* task,
    const size_t worker_index)
{
    clm_async_pool_internal_worker* worker =
        clm_async_pool_internal_find_worker(pool, worker_index);
    clm_async_pool_internal_command* command =
        clm_async_pool_internal_command_new(CLM_ASYNC_POOL_COMMAND_SPAWN, task);
    if (worker == NULL || command == NULL) {
        free(command);
        return CLM_ASYNC_OPERATION_FAILURE;
    }
    task->worker_index = worker_index;
    task->state = CLM_ASYNC_TASK_READY;
    task->counted_active = true;
    ++worker->active_count;
    pool->next_worker = (worker_index + 1) % pool->config.worker_count;
    clm_async_pool_internal_enqueue_command_locked(worker, command);
    return CLM_ASYNC_OPERATION_SUCCESS;
}

static inline void clm_async_pool_internal_drain_pending_locked(
    clm_async_pool_internal* pool)
{
    while (!pool->stopping && pool->pending_head != NULL) {
        const size_t worker_index =
            clm_async_pool_internal_find_available_worker_locked(pool);
        if (worker_index == CLM_ASYNC_NO_WORKER)
            return;
        clm_async_pool_internal_task* task = pool->pending_head;
        if (clm_async_pool_internal_assign_locked(pool, task, worker_index) !=
            CLM_ASYNC_OPERATION_SUCCESS)
            return;
        pool->pending_head = task->pending_next;
        if (pool->pending_head == NULL)
            pool->pending_tail = NULL;
        --pool->pending_count;
        task->pending_next = NULL;
        task->in_pending_queue = false;
    }
}

static inline clm_int clm_async_pool_internal_queue_task_command_locked(
    clm_async_pool_internal_task* task,
    const clm_async_pool_internal_command_type type)
{
    clm_async_pool_internal_worker* worker =
        clm_async_pool_internal_find_worker(task->pool, task->worker_index);
    if (worker == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if ((type == CLM_ASYNC_POOL_COMMAND_WAKE && task->wake_queued) ||
        (type == CLM_ASYNC_POOL_COMMAND_CANCEL && task->cancel_queued))
        return CLM_ASYNC_OPERATION_SUCCESS;
    clm_async_pool_internal_command* command =
        clm_async_pool_internal_command_new(type, task);
    if (command == NULL)
        return CLM_ASYNC_OPERATION_FAILURE;
    if (type == CLM_ASYNC_POOL_COMMAND_WAKE)
        task->wake_queued = true;
    if (type == CLM_ASYNC_POOL_COMMAND_CANCEL)
        task->cancel_queued = true;
    clm_async_pool_internal_enqueue_command_locked(worker, command);
    return CLM_ASYNC_OPERATION_SUCCESS;
}

static inline void clm_async_pool_internal_remove_pending_locked(
    clm_async_pool_internal* pool,
    clm_async_pool_internal_task* target)
{
    clm_async_pool_internal_task* previous = NULL;
    for (clm_async_pool_internal_task* task = pool->pending_head;
        task != NULL; task = task->pending_next) {
        if (task == target) {
            if (previous == NULL)
                pool->pending_head = task->pending_next;
            else
                previous->pending_next = task->pending_next;
            if (pool->pending_tail == task)
                pool->pending_tail = previous;
            task->pending_next = NULL;
            task->in_pending_queue = false;
            --pool->pending_count;
            return;
        }
        previous = task;
    }
}

static inline void clm_async_pool_internal_finish_unassigned(
    clm_async_pool_internal_task* task,
    const clm_async_task_state terminal_state)
{
    task->state = terminal_state;
    if (!task->cleanup_called) {
        task->cleanup_called = true;
        if (task->cleanup != NULL)
            task->cleanup(task->context_id);
    }
}

static inline clm_async_step_result clm_async_pool_internal_step(
    const clm_id internal_id,
    const clm_id pool_id);
static inline void clm_async_pool_internal_task_state_changed(
    const clm_id internal_id,
    const clm_ulong generation,
    const clm_async_task_state state,
    void* context);

static inline clm_async_step_result clm_async_pool_internal_task_step(
    const clm_id internal_id,
    const clm_id pool_id)
{
    return clm_async_pool_internal_step(internal_id, pool_id);
}

static inline void clm_async_pool_internal_state_observer(
    const clm_id internal_id,
    const clm_ulong generation,
    const clm_async_task_state state,
    void* context)
{
    clm_async_pool_internal_task* task =
        (clm_async_pool_internal_task*)context;
    if (task == NULL || task->internal_id != internal_id)
        return;
    clm_async_pool_internal_task_state_changed(internal_id, generation, state,
        task);
}

static inline void clm_async_pool_internal_task_list_remove_locked(
    clm_async_pool_internal* pool,
    clm_async_pool_internal_task* target)
{
    clm_async_pool_internal_task** link = &pool->tasks;
    while (*link != NULL && *link != target)
        link = &(*link)->next;
    if (*link == target)
        *link = target->next;
}

static inline void clm_async_pool_internal_complete_spawn_failure(
    clm_async_pool_internal_task* task)
{
    clm_async_pool_internal_task_state_changed(task->internal_id, 0,
        CLM_ASYNC_TASK_FAILED, task);
}

static inline void clm_async_pool_internal_process_command(
    clm_async_pool_internal_worker* worker,
    clm_async_pool_internal_command* command)
{
    clm_async_pool_internal* pool = worker->pool;
    clm_async_pool_internal_task* task = command->task;
    clm_int result = CLM_ASYNC_OPERATION_SUCCESS;
    if (command->type == CLM_ASYNC_POOL_COMMAND_SPAWN) {
        result = clm_async_internal_spawn(worker->scheduler_id,
            task->internal_id, clm_async_pool_internal_task_step, pool->id,
            NULL,
            clm_async_pool_internal_state_observer, task);
        if (result == CLM_ASYNC_OPERATION_SUCCESS) {
            clm_async_task_token token;
            result = clm_async_task_get_token(worker->scheduler_id,
                task->internal_id, &token);
            if (result == CLM_ASYNC_OPERATION_SUCCESS) {
                mtx_lock(&pool->mutex);
                task->scheduler_token = token;
                task->scheduler_token_valid = true;
                const bool wake_pending = task->pending_wake;
                task->pending_wake = false;
                mtx_unlock(&pool->mutex);
                if (wake_pending)
                    (void)clm_async_wake_token(worker->scheduler_id, token);
            }
        } else {
            clm_async_pool_internal_complete_spawn_failure(task);
        }
    } else if (command->type == CLM_ASYNC_POOL_COMMAND_WAKE) {
        mtx_lock(&pool->mutex);
        task->wake_queued = false;
        const bool token_valid = task->scheduler_token_valid;
        const clm_async_task_token token = task->scheduler_token;
        mtx_unlock(&pool->mutex);
        result = token_valid
            ? clm_async_wake_token(worker->scheduler_id, token)
            : CLM_ASYNC_OPERATION_NOT_FOUND;
    } else if (command->type == CLM_ASYNC_POOL_COMMAND_CANCEL) {
        mtx_lock(&pool->mutex);
        task->cancel_queued = false;
        const clm_id internal_id = task->internal_id;
        mtx_unlock(&pool->mutex);
        result = clm_async_cancel(worker->scheduler_id, internal_id);
    } else if (command->type == CLM_ASYNC_POOL_COMMAND_RELEASE) {
        result = clm_async_task_release(worker->scheduler_id,
            task->internal_id);
        mtx_lock(&pool->mutex);
        if (result == CLM_ASYNC_OPERATION_SUCCESS)
            clm_async_pool_internal_task_list_remove_locked(pool, task);
        command->result = result;
        command->complete = true;
        if (task->pending_commands != 0)
            --task->pending_commands;
        cnd_broadcast(&pool->changed);
        mtx_unlock(&pool->mutex);
        if (result == CLM_ASYNC_OPERATION_SUCCESS)
            free(task);
        return;
    }
    mtx_lock(&pool->mutex);
    if (command->type == CLM_ASYNC_POOL_COMMAND_SPAWN &&
        result != CLM_ASYNC_OPERATION_SUCCESS)
        task->state = CLM_ASYNC_TASK_FAILED;
    if (task->pending_commands != 0)
        --task->pending_commands;
    if (!command->stack_owned)
        free(command);
    clm_async_pool_internal_drain_pending_locked(pool);
    cnd_broadcast(&pool->changed);
    mtx_unlock(&pool->mutex);
}

static inline clm_async_pool_internal_task*
clm_async_pool_internal_find_cancel_candidate_locked(
    clm_async_pool_internal_worker* worker)
{
    for (clm_async_pool_internal_task* task = worker->pool->tasks;
        task != NULL; task = task->next)
        if (task->worker_index == worker->index && task->counted_active &&
            !task->cancel_dispatched) {
            task->cancel_dispatched = true;
            return task;
        }
    return NULL;
}

static inline bool clm_async_pool_internal_has_cancel_candidate_locked(
    clm_async_pool_internal_worker* worker)
{
    for (clm_async_pool_internal_task* task = worker->pool->tasks;
        task != NULL; task = task->next)
        if (task->worker_index == worker->index && task->counted_active &&
            !task->cancel_dispatched)
            return true;
    return false;
}

static inline int clm_async_pool_internal_worker_main(void* argument)
{
    clm_async_pool_internal_worker* worker =
        (clm_async_pool_internal_worker*)argument;
    clm_async_pool_internal* pool = worker->pool;
    if (clm_async_internal_scheduler_create_auto(&worker->scheduler_id) !=
        CLM_ASYNC_OPERATION_SUCCESS) {
        mtx_lock(&pool->mutex);
        ++pool->failed_start_count;
        cnd_broadcast(&pool->changed);
        mtx_unlock(&pool->mutex);
        return -1;
    }
    mtx_lock(&pool->mutex);
    worker->started = true;
    ++pool->started_count;
    cnd_broadcast(&pool->changed);
    mtx_unlock(&pool->mutex);
    for (;;) {
        clm_async_pool_internal_command* command = NULL;
        clm_async_pool_internal_task* cancel_task = NULL;
        mtx_lock(&pool->mutex);
        command = clm_async_pool_internal_pop_command_locked(worker);
        if (command == NULL && pool->stopping)
            cancel_task = clm_async_pool_internal_find_cancel_candidate_locked(worker);
        if (command == NULL && cancel_task == NULL && pool->stopping &&
            worker->active_count == 0) {
            mtx_unlock(&pool->mutex);
            break;
        }
        mtx_unlock(&pool->mutex);
        if (command != NULL) {
            clm_async_pool_internal_process_command(worker, command);
            continue;
        }
        if (cancel_task != NULL) {
            (void)clm_async_cancel(worker->scheduler_id,
                cancel_task->internal_id);
            continue;
        }
        size_t steps = 0;
        const clm_int run_status = clm_async_run(worker->scheduler_id,
            pool->config.steps_per_cycle, &steps);
        if (run_status != CLM_ASYNC_OPERATION_SUCCESS) {
            mtx_lock(&pool->mutex);
            pool->stopping = true;
            cnd_broadcast(&pool->changed);
            mtx_unlock(&pool->mutex);
            continue;
        }
        if (steps == 0) {
            mtx_lock(&pool->mutex);
            if (worker->command_head == NULL &&
                !(pool->stopping && worker->active_count == 0) &&
                !(pool->stopping &&
                clm_async_pool_internal_has_cancel_candidate_locked(worker)))
                cnd_wait(&pool->changed, &pool->mutex);
            mtx_unlock(&pool->mutex);
        }
    }
    (void)clm_async_scheduler_destroy(worker->scheduler_id);
    mtx_lock(&pool->mutex);
    worker->exited = true;
    cnd_broadcast(&pool->changed);
    mtx_unlock(&pool->mutex);
    return 0;
}

static inline clm_int clm_async_pool_internal_register(
    clm_async_pool_internal* pool)
{
    clm_async_pool_internal_lock_registry();
    for (clm_async_pool_internal* item = clm_async_pool_internal_pools;
        item != NULL; item = item->next)
        if (item->id == pool->id) {
            clm_async_pool_internal_unlock_registry();
            return CLM_ASYNC_OPERATION_BUSY;
        }
    pool->next = clm_async_pool_internal_pools;
    clm_async_pool_internal_pools = pool;
    clm_async_pool_internal_unlock_registry();
    return CLM_ASYNC_OPERATION_SUCCESS;
}

static inline void clm_async_pool_internal_unregister(
    clm_async_pool_internal* pool)
{
    clm_async_pool_internal_lock_registry();
    clm_async_pool_internal** link = &clm_async_pool_internal_pools;
    while (*link != NULL && *link != pool)
        link = &(*link)->next;
    if (*link == pool)
        *link = pool->next;
    clm_async_pool_internal_unlock_registry();
}

static inline bool clm_async_pool_internal_is_worker_thread(
    clm_async_pool_internal* pool)
{
    const thrd_t current = thrd_current();
    for (size_t index = 0; index < pool->config.worker_count; ++index)
        if (pool->workers[index].thread_created &&
            thrd_equal(current, pool->workers[index].thread))
            return true;
    return false;
}

static inline void clm_async_pool_internal_free_unstarted_pool(
    clm_async_pool_internal* pool)
{
    if (pool == NULL)
        return;
    clm_async_pool_internal_unregister(pool);
    mtx_lock(&pool->mutex);
    while (atomic_load_explicit(&pool->api_references,
            memory_order_acquire) != 0)
        cnd_wait(&pool->changed, &pool->mutex);
    mtx_unlock(&pool->mutex);
    for (size_t index = 0; index < pool->config.worker_count; ++index) {
        clm_async_pool_internal_command* command =
            pool->workers != NULL ? pool->workers[index].command_head : NULL;
        while (command != NULL) {
            clm_async_pool_internal_command* next = command->next;
            if (!command->stack_owned)
                free(command);
            command = next;
        }
    }
    free(pool->workers);
    cnd_destroy(&pool->changed);
    mtx_destroy(&pool->mutex);
    free(pool);
}

static inline void clm_async_pool_internal_task_state_changed(
    const clm_id internal_id,
    const clm_ulong generation,
    const clm_async_task_state state,
    void* context)
{
    (void)internal_id;
    (void)generation;
    clm_async_pool_internal_task* task =
        (clm_async_pool_internal_task*)context;
    clm_async_pool_internal* pool = task->pool;
    clm_async_context_cleanup_fn cleanup = NULL;
    clm_id context_id = 0;
    mtx_lock(&pool->mutex);
    const bool was_terminal = clm_async_task_is_terminal(task->state);
    task->state = state;
    if (!was_terminal && clm_async_task_is_terminal(state) &&
        !task->cleanup_called) {
        task->cleanup_called = true;
        cleanup = task->cleanup;
        context_id = task->context_id;
        task->cleanup_in_progress = (cleanup != NULL);
    }
    if (!was_terminal && clm_async_task_is_terminal(state) &&
        task->counted_active) {
        clm_async_pool_internal_worker* worker =
            clm_async_pool_internal_find_worker(pool, task->worker_index);
        if (worker != NULL && worker->active_count != 0)
            --worker->active_count;
        task->counted_active = false;
        clm_async_pool_internal_drain_pending_locked(pool);
    }
    cnd_broadcast(&pool->changed);
    mtx_unlock(&pool->mutex);
    if (cleanup != NULL) {
        cleanup(context_id);
        mtx_lock(&pool->mutex);
        task->cleanup_in_progress = false;
        cnd_broadcast(&pool->changed);
        mtx_unlock(&pool->mutex);
    }
}

static inline clm_async_step_result clm_async_pool_internal_step(
    const clm_id internal_id,
    const clm_id pool_id)
{
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_STEP_FAIL;
    mtx_lock(&pool->mutex);
    clm_async_pool_internal_task* task =
        clm_async_pool_internal_find_internal_task(pool, internal_id);
    if (task == NULL) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        return CLM_ASYNC_STEP_FAIL;
    }
    const clm_async_step_fn step = task->step;
    const clm_id public_id = task->id;
    const clm_id context_id = task->context_id;
    const size_t worker_index = task->worker_index;
    bool cancel_requested = task->cancel_requested;
    mtx_unlock(&pool->mutex);

    /* The scheduler is currently executing this wrapper on its owner thread.
     * Mark scheduler cancellation directly so the current step becomes
     * CANCELLED, even if a CANCEL mailbox command has not been processed yet.
     * Never call the user's callback after cancellation has been requested.
     */
    clm_async_step_result result = CLM_ASYNC_STEP_YIELD;
    if (!cancel_requested) {
        result = step(public_id, context_id);
        mtx_lock(&pool->mutex);
        cancel_requested = task->cancel_requested;
        mtx_unlock(&pool->mutex);
    }
    if (cancel_requested) {
        clm_async_pool_internal_worker* worker =
            clm_async_pool_internal_find_worker(pool, worker_index);
        if (worker != NULL)
            (void)clm_async_cancel(worker->scheduler_id, internal_id);
    }
    clm_async_pool_internal_release(pool);
    return result;
}

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
 */
static inline clm_int clm_async_pool_create(
    const clm_id pool_id,
    const clm_async_pool_config* config)
{
    if (config == NULL || config->worker_count == 0 ||
        config->max_tasks_per_worker == 0 || config->steps_per_cycle == 0 ||
        config->worker_count > (size_t)-1 /
            sizeof(clm_async_pool_internal_worker) ||
        (config->overflow_policy != CLM_ASYNC_OVERFLOW_REJECT &&
         config->overflow_policy != CLM_ASYNC_OVERFLOW_QUEUE) ||
        (config->overflow_policy == CLM_ASYNC_OVERFLOW_QUEUE &&
         config->max_pending_tasks == 0))
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_pool_internal* pool =
        (clm_async_pool_internal*)calloc(1, sizeof(*pool));
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_FAILURE;
    pool->id = pool_id;
    pool->config = *config;
    pool->next_internal_id = 1;
    pool->next_generation = 1;
    pool->initializing = true;
    atomic_init(&pool->api_references, 0);
    if (mtx_init(&pool->mutex, mtx_plain) != thrd_success) {
        free(pool);
        return CLM_ASYNC_OPERATION_FAILURE;
    }
    if (cnd_init(&pool->changed) != thrd_success) {
        mtx_destroy(&pool->mutex);
        free(pool);
        return CLM_ASYNC_OPERATION_FAILURE;
    }
    pool->workers = (clm_async_pool_internal_worker*)calloc(
        config->worker_count, sizeof(*pool->workers));
    if (pool->workers == NULL) {
        cnd_destroy(&pool->changed);
        mtx_destroy(&pool->mutex);
        free(pool);
        return CLM_ASYNC_OPERATION_FAILURE;
    }
    for (size_t index = 0; index < config->worker_count; ++index) {
        pool->workers[index].pool = pool;
        pool->workers[index].index = index;
        pool->workers[index].accepting = true;
    }
    const clm_int register_status = clm_async_pool_internal_register(pool);
    if (register_status != CLM_ASYNC_OPERATION_SUCCESS) {
        clm_async_pool_internal_free_unstarted_pool(pool);
        return register_status;
    }
    for (size_t index = 0; index < config->worker_count; ++index) {
        if (thrd_create(&pool->workers[index].thread,
            clm_async_pool_internal_worker_main, &pool->workers[index]) !=
            thrd_success) {
            mtx_lock(&pool->mutex);
            pool->stopping = true;
            cnd_broadcast(&pool->changed);
            mtx_unlock(&pool->mutex);
            for (size_t join_index = 0; join_index < index; ++join_index)
                (void)thrd_join(pool->workers[join_index].thread, NULL);
            clm_async_pool_internal_free_unstarted_pool(pool);
            return CLM_ASYNC_OPERATION_FAILURE;
        }
        pool->workers[index].thread_created = true;
    }
    mtx_lock(&pool->mutex);
    while (pool->started_count + pool->failed_start_count < config->worker_count)
        cnd_wait(&pool->changed, &pool->mutex);
    const bool startup_failed = pool->failed_start_count != 0;
    if (startup_failed) {
        pool->stopping = true;
        cnd_broadcast(&pool->changed);
    }
    mtx_unlock(&pool->mutex);
    if (startup_failed) {
        for (size_t index = 0; index < config->worker_count; ++index)
            (void)thrd_join(pool->workers[index].thread, NULL);
        clm_async_pool_internal_free_unstarted_pool(pool);
        return CLM_ASYNC_OPERATION_FAILURE;
    }
    /* Publish fully started pool atomically through the registry lock. */
    clm_async_pool_internal_lock_registry();
    pool->initializing = false;
    clm_async_pool_internal_unlock_registry();
    return CLM_ASYNC_OPERATION_SUCCESS;
}

/** Stops admission, cancels unfinished tasks (including pool QUEUED tasks),
 * finishes already running callbacks, runs remaining cleanups, joins worker
 * threads, and destroys the pool. Blocking; never call from this pool's
 * callbacks or worker threads (to avoid self-join/deadlock).
 */
static inline clm_int clm_async_pool_destroy(const clm_id pool_id)
{
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (clm_async_pool_internal_is_worker_thread(pool)) {
        clm_async_pool_internal_release(pool);
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    }
    mtx_lock(&pool->mutex);
    if (pool->destroying) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        return CLM_ASYNC_OPERATION_BUSY;
    }
    pool->destroying = true;
    pool->stopping = true;
    clm_async_pool_internal_task* cleanup_list = NULL;
    for (clm_async_pool_internal_task* task = pool->tasks; task != NULL;
        task = task->next) {
        if (task->in_pending_queue) {
            clm_async_pool_internal_remove_pending_locked(pool, task);
            task->state = CLM_ASYNC_TASK_CANCELLED;
            task->cleanup_called = true;
            task->pending_next = cleanup_list;
            cleanup_list = task;
        } else if (!clm_async_task_is_terminal(task->state)) {
            task->cancel_requested = true;
        }
    }
    cnd_broadcast(&pool->changed);
    mtx_unlock(&pool->mutex);
    while (cleanup_list != NULL) {
        clm_async_pool_internal_task* task = cleanup_list;
        cleanup_list = task->pending_next;
        task->pending_next = NULL;
        if (task->cleanup != NULL)
            task->cleanup(task->context_id);
    }
    for (size_t index = 0; index < pool->config.worker_count; ++index)
        if (pool->workers[index].thread_created)
            (void)thrd_join(pool->workers[index].thread, NULL);
    clm_async_pool_internal_unregister(pool);
    mtx_lock(&pool->mutex);
    while (atomic_load_explicit(&pool->api_references, memory_order_acquire) > 1)
        cnd_wait(&pool->changed, &pool->mutex);
    mtx_unlock(&pool->mutex);
    atomic_fetch_sub_explicit(&pool->api_references, 1, memory_order_release);
    clm_async_pool_internal_task* task = pool->tasks;
    while (task != NULL) {
        clm_async_pool_internal_task* next = task->next;
        free(task);
        task = next;
    }
    for (size_t index = 0; index < pool->config.worker_count; ++index) {
        clm_async_pool_internal_command* command = pool->workers[index].command_head;
        while (command != NULL) {
            clm_async_pool_internal_command* next = command->next;
            if (!command->stack_owned)
                free(command);
            command = next;
        }
    }
    free(pool->workers);
    cnd_destroy(&pool->changed);
    mtx_destroy(&pool->mutex);
    free(pool);
    return CLM_ASYNC_OPERATION_SUCCESS;
}

/** Submits one task. Does NOT execute step synchronously.
 * Returns SUCCESS when accepted (assigned READY or unassigned QUEUED).
 * Returns BUSY if no eligible worker under REJECT, or QUEUE_FULL if QUEUE
 * is configured but pending capacity is exhausted.
 * A successful enqueue does not imply the callback has started.
 */
static inline clm_int clm_async_pool_spawn(
    const clm_id pool_id,
    const clm_id task_id,
    const clm_async_step_fn step,
    const clm_id context_id,
    const clm_async_context_cleanup_fn cleanup)
{
    if (step == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_pool_internal_task* task =
        (clm_async_pool_internal_task*)calloc(1, sizeof(*task));
    if (task == NULL)
        return CLM_ASYNC_OPERATION_FAILURE;
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL) {
        free(task);
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    }
    mtx_lock(&pool->mutex);
    if (pool->stopping) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        free(task);
        return CLM_ASYNC_OPERATION_STOPPING;
    }
    if (clm_async_pool_internal_find_task(pool, task_id) != NULL) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        free(task);
        return CLM_ASYNC_OPERATION_BUSY;
    }
    if (pool->next_internal_id == 0) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        free(task);
        return CLM_ASYNC_OPERATION_FAILURE;
    }
    const clm_ulong generation = clm_async_internal_new_generation();
    if (generation == 0) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        free(task);
        return CLM_ASYNC_OPERATION_FAILURE;
    }
    task->pool = pool;
    task->id = task_id;
    task->generation = generation;
    task->internal_id = pool->next_internal_id++;
    task->context_id = context_id;
    task->step = step;
    task->cleanup = cleanup;
    task->worker_index = CLM_ASYNC_NO_WORKER;
    task->state = CLM_ASYNC_TASK_QUEUED;
    task->next = pool->tasks;
    pool->tasks = task;
    /* Older pending tasks have admission priority over new submissions. */
    clm_async_pool_internal_drain_pending_locked(pool);
    const size_t worker_index = (pool->pending_head == NULL)
        ? clm_async_pool_internal_find_available_worker_locked(pool)
        : CLM_ASYNC_NO_WORKER;
    clm_int status = CLM_ASYNC_OPERATION_SUCCESS;
    if (worker_index != CLM_ASYNC_NO_WORKER) {
        status = clm_async_pool_internal_assign_locked(pool, task, worker_index);
        if (status != CLM_ASYNC_OPERATION_SUCCESS)
            clm_async_pool_internal_task_list_remove_locked(pool, task);
    } else if (pool->config.overflow_policy == CLM_ASYNC_OVERFLOW_REJECT) {
        status = CLM_ASYNC_OPERATION_BUSY;
        clm_async_pool_internal_task_list_remove_locked(pool, task);
    } else if (pool->pending_count >= pool->config.max_pending_tasks) {
        status = CLM_ASYNC_OPERATION_QUEUE_FULL;
        clm_async_pool_internal_task_list_remove_locked(pool, task);
    } else {
        task->in_pending_queue = true;
        if (pool->pending_tail == NULL)
            pool->pending_head = task;
        else
            pool->pending_tail->pending_next = task;
        pool->pending_tail = task;
        ++pool->pending_count;
    }
    cnd_broadcast(&pool->changed);
    mtx_unlock(&pool->mutex);
    clm_async_pool_internal_release(pool);
    if (status != CLM_ASYNC_OPERATION_SUCCESS)
        free(task);
    return status;
}

/** Thread-safe wake request. Assigned WAITING -> READY; RUNNING/READY gets
 * one coalesced pending wake. QUEUED tasks remember the wake until assigned.
 * Wake commands must be delivered to the OWNING worker's mailbox, not invoke
 * the owner-thread-only clm_async_wake() directly from another thread.
 * Late wakeups using a reused task_id are unsafe; prefer wake_token().
 */
static inline clm_int clm_async_pool_wake(
    const clm_id pool_id,
    const clm_id task_id)
{
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    mtx_lock(&pool->mutex);
    clm_async_pool_internal_task* task =
        clm_async_pool_internal_find_task(pool, task_id);
    clm_int status = CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task != NULL) {
        if (clm_async_task_is_terminal(task->state)) {
            status = CLM_ASYNC_OPERATION_INVALID_STATE;
        } else if (task->in_pending_queue || !task->scheduler_token_valid) {
            task->pending_wake = true;
            status = CLM_ASYNC_OPERATION_SUCCESS;
        } else {
            status = clm_async_pool_internal_queue_task_command_locked(task,
                CLM_ASYNC_POOL_COMMAND_WAKE);
        }
    }
    mtx_unlock(&pool->mutex);
    clm_async_pool_internal_release(pool);
    return status;
}

/** Retrieves a task's generation-safe token. Valid even when QUEUED. */
static inline clm_int clm_async_pool_task_get_token(
    const clm_id pool_id,
    const clm_id task_id,
    clm_async_task_token* out_token)
{
    if (out_token == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    mtx_lock(&pool->mutex);
    clm_async_pool_internal_task* task =
        clm_async_pool_internal_find_task(pool, task_id);
    clm_int status = CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task != NULL) {
        const clm_async_task_token token = {task->id, task->generation};
        *out_token = token;
        status = CLM_ASYNC_OPERATION_SUCCESS;
    }
    mtx_unlock(&pool->mutex);
    clm_async_pool_internal_release(pool);
    return status;
}

/** Preferred thread-safe wake for external I/O completion callbacks.
 * Must not affect a later incarnation of a reused task ID.
 */
static inline clm_int clm_async_pool_wake_token(
    const clm_id pool_id,
    const clm_async_task_token token)
{
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    mtx_lock(&pool->mutex);
    clm_async_pool_internal_task* task =
        clm_async_pool_internal_find_task(pool, token.task_id);
    clm_int status = CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task != NULL) {
        if (task->generation != token.generation)
            status = CLM_ASYNC_OPERATION_NOT_FOUND;
        else if (clm_async_task_is_terminal(task->state))
            status = CLM_ASYNC_OPERATION_INVALID_STATE;
        else if (task->in_pending_queue || !task->scheduler_token_valid) {
            task->pending_wake = true;
            status = CLM_ASYNC_OPERATION_SUCCESS;
        } else {
            status = clm_async_pool_internal_queue_task_command_locked(task,
                CLM_ASYNC_POOL_COMMAND_WAKE);
        }
    }
    mtx_unlock(&pool->mutex);
    clm_async_pool_internal_release(pool);
    return status;
}

/** Thread-safe cancellation request: QUEUED/READY/WAITING cancel without
 * another step; a RUNNING step is allowed to return before cancellation
 * becomes terminal. Cleanup occurs once, never concurrently with the step.
 */
static inline clm_int clm_async_pool_cancel(
    const clm_id pool_id,
    const clm_id task_id)
{
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    clm_async_pool_internal_task* cleanup_task = NULL;
    clm_async_context_cleanup_fn cleanup_callback = NULL;
    clm_id cleanup_context_id = 0;
    mtx_lock(&pool->mutex);
    clm_async_pool_internal_task* task =
        clm_async_pool_internal_find_task(pool, task_id);
    clm_int status = CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task != NULL) {
        if (clm_async_task_is_terminal(task->state)) {
            status = CLM_ASYNC_OPERATION_INVALID_STATE;
        } else if (task->in_pending_queue) {
            clm_async_pool_internal_remove_pending_locked(pool, task);
            task->state = CLM_ASYNC_TASK_CANCELLED;
            task->cleanup_called = true;
            task->cleanup_in_progress = (task->cleanup != NULL);
            cleanup_task = task;
            cleanup_callback = task->cleanup;
            cleanup_context_id = task->context_id;
            status = CLM_ASYNC_OPERATION_SUCCESS;
            cnd_broadcast(&pool->changed);
        } else {
            task->cancel_requested = true;
            status = clm_async_pool_internal_queue_task_command_locked(task,
                CLM_ASYNC_POOL_COMMAND_CANCEL);
            if (status != CLM_ASYNC_OPERATION_SUCCESS)
                task->cancel_requested = false;
        }
    }
    mtx_unlock(&pool->mutex);
    if (cleanup_task != NULL && cleanup_callback != NULL) {
        cleanup_callback(cleanup_context_id);
        mtx_lock(&pool->mutex);
        cleanup_task->cleanup_in_progress = false;
        cnd_broadcast(&pool->changed);
        mtx_unlock(&pool->mutex);
    }
    clm_async_pool_internal_release(pool);
    return status;
}

/** Reads a task's snapshot state. Includes QUEUED and terminal records. */
static inline clm_int clm_async_pool_get_state(
    const clm_id pool_id,
    const clm_id task_id,
    clm_async_task_state* out_state)
{
    if (out_state == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    mtx_lock(&pool->mutex);
    clm_async_pool_internal_task* task =
        clm_async_pool_internal_find_task(pool, task_id);
    clm_int status = CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task != NULL) {
        *out_state = task->state;
        status = CLM_ASYNC_OPERATION_SUCCESS;
    }
    mtx_unlock(&pool->mutex);
    clm_async_pool_internal_release(pool);
    return status;
}

/** Returns assigned zero-based worker index, or CLM_ASYNC_NO_WORKER if QUEUED.
 * Once assigned, a task's index never changes even when it is WAITING.
 */
static inline clm_int clm_async_pool_task_get_worker_index(
    const clm_id pool_id,
    const clm_id task_id,
    size_t* out_worker_index)
{
    if (out_worker_index == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    mtx_lock(&pool->mutex);
    clm_async_pool_internal_task* task =
        clm_async_pool_internal_find_task(pool, task_id);
    clm_int status = CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task != NULL) {
        *out_worker_index = task->worker_index;
        status = CLM_ASYNC_OPERATION_SUCCESS;
    }
    mtx_unlock(&pool->mutex);
    clm_async_pool_internal_release(pool);
    return status;
}

/** Releases a terminal task record. Active / QUEUED tasks cannot be released.
 * Does not invoke cleanup a second time.
 */
static inline clm_int clm_async_pool_task_release(
    const clm_id pool_id,
    const clm_id task_id)
{
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (clm_async_pool_internal_is_worker_thread(pool)) {
        clm_async_pool_internal_release(pool);
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    }
    clm_async_pool_internal_command command = {0};
    mtx_lock(&pool->mutex);
    if (pool->stopping) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        return CLM_ASYNC_OPERATION_STOPPING;
    }
    clm_async_pool_internal_task* task =
        clm_async_pool_internal_find_task(pool, task_id);
    if (task == NULL) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    }
    if (!clm_async_task_is_terminal(task->state) ||
        task->pending_commands != 0 || task->cleanup_in_progress) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    }
    if (pool->stopping) {
        mtx_unlock(&pool->mutex);
        clm_async_pool_internal_release(pool);
        return CLM_ASYNC_OPERATION_STOPPING;
    }
    if (task->worker_index == CLM_ASYNC_NO_WORKER) {
        clm_async_pool_internal_task_list_remove_locked(pool, task);
        mtx_unlock(&pool->mutex);
        free(task);
        clm_async_pool_internal_release(pool);
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    clm_async_pool_internal_worker* worker =
        clm_async_pool_internal_find_worker(pool, task->worker_index);
    command.type = CLM_ASYNC_POOL_COMMAND_RELEASE;
    command.task = task;
    command.stack_owned = true;
    clm_async_pool_internal_enqueue_command_locked(worker, &command);
    while (!command.complete)
        cnd_wait(&pool->changed, &pool->mutex);
    const clm_int status = command.result;
    mtx_unlock(&pool->mutex);
    clm_async_pool_internal_release(pool);
    return status;
}

/** Manually enable/disable NEW assignments for a worker. Existing tasks
 * continue as normal. An enabled worker is still BUSY if its capacity is full.
 * worker_index is zero-based in [0, worker_count).
 */
static inline clm_int clm_async_pool_worker_set_accepting(
    const clm_id pool_id,
    const size_t worker_index,
    const bool accepting)
{
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    mtx_lock(&pool->mutex);
    clm_async_pool_internal_worker* worker =
        clm_async_pool_internal_find_worker(pool, worker_index);
    clm_int status = CLM_ASYNC_OPERATION_SUCCESS;
    if (worker == NULL)
        status = CLM_ASYNC_OPERATION_INVALID_STATE;
    else if (pool->stopping)
        status = CLM_ASYNC_OPERATION_STOPPING;
    else {
        worker->accepting = accepting;
        clm_async_pool_internal_drain_pending_locked(pool);
        cnd_broadcast(&pool->changed);
    }
    mtx_unlock(&pool->mutex);
    clm_async_pool_internal_release(pool);
    return status;
}

/** Reads the effective worker admission state (manual + capacity). */
static inline clm_int clm_async_pool_worker_get_state(
    const clm_id pool_id,
    const size_t worker_index,
    clm_async_worker_state* out_state)
{
    if (out_state == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    mtx_lock(&pool->mutex);
    clm_async_pool_internal_worker* worker =
        clm_async_pool_internal_find_worker(pool, worker_index);
    clm_int status = CLM_ASYNC_OPERATION_SUCCESS;
    if (worker == NULL)
        status = CLM_ASYNC_OPERATION_INVALID_STATE;
    else if (worker->exited)
        *out_state = CLM_ASYNC_WORKER_STOPPED;
    else if (pool->stopping)
        *out_state = CLM_ASYNC_WORKER_STOPPING;
    else if (!worker->accepting ||
        worker->active_count >= pool->config.max_tasks_per_worker)
        *out_state = CLM_ASYNC_WORKER_BUSY;
    else
        *out_state = CLM_ASYNC_WORKER_AVAILABLE;
    mtx_unlock(&pool->mutex);
    clm_async_pool_internal_release(pool);
    return status;
}

/** Reads the configured worker count. */
static inline clm_int clm_async_pool_worker_count(
    const clm_id pool_id,
    size_t* out_worker_count)
{
    if (out_worker_count == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    *out_worker_count = pool->config.worker_count;
    clm_async_pool_internal_release(pool);
    return CLM_ASYNC_OPERATION_SUCCESS;
}

/** Reads aggregate state counts, for metrics and backpressure diagnostics. */
static inline clm_int clm_async_pool_get_stats(
    const clm_id pool_id,
    clm_async_pool_stats* out_stats)
{
    if (out_stats == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_pool_internal* pool = clm_async_pool_internal_acquire(pool_id);
    if (pool == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    clm_async_pool_stats stats = {0};
    mtx_lock(&pool->mutex);
    for (clm_async_pool_internal_task* task = pool->tasks; task != NULL;
        task = task->next) {
        if (task->state == CLM_ASYNC_TASK_QUEUED)
            ++stats.pending;
        else if (task->state == CLM_ASYNC_TASK_READY)
            ++stats.ready;
        else if (task->state == CLM_ASYNC_TASK_RUNNING)
            ++stats.running;
        else if (task->state == CLM_ASYNC_TASK_WAITING)
            ++stats.waiting;
        else
            ++stats.terminal;
    }
    mtx_unlock(&pool->mutex);
    *out_stats = stats;
    clm_async_pool_internal_release(pool);
    return CLM_ASYNC_OPERATION_SUCCESS;
}

