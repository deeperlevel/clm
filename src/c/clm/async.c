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

#include <stdlib.h>

clm_async_internal_scheduler* clm_async_internal_schedulers = NULL;
atomic_flag clm_async_internal_registry_lock = ATOMIC_FLAG_INIT;
_Atomic(clm_ulong) clm_async_internal_generation_sequence = 1;

bool clm_async_task_is_terminal(const clm_async_task_state state)
{
    return state == CLM_ASYNC_TASK_COMPLETED ||
        state == CLM_ASYNC_TASK_FAILED ||
        state == CLM_ASYNC_TASK_CANCELLED;
}

clm_ulong clm_async_internal_new_generation(void)
{
    clm_ulong old = atomic_load_explicit(
        &clm_async_internal_generation_sequence, memory_order_relaxed);
    while (old != 0 && old != ~(clm_ulong)0)
    {
        if (atomic_compare_exchange_weak_explicit(
            &clm_async_internal_generation_sequence, &old, old + 1,
            memory_order_relaxed, memory_order_relaxed))
            return old;
    }
    return 0;
}

void clm_async_internal_lock_registry(void)
{
    while (atomic_flag_test_and_set_explicit(&clm_async_internal_registry_lock,
                                             memory_order_acquire))
    {
    }
}

void clm_async_internal_unlock_registry(void)
{
    atomic_flag_clear_explicit(&clm_async_internal_registry_lock,
                               memory_order_release);
}

clm_async_internal_scheduler* clm_async_internal_find_scheduler(
    const clm_id scheduler_id)
{
    clm_async_internal_scheduler* found = NULL;
    clm_async_internal_lock_registry();
    for (clm_async_internal_scheduler* item = clm_async_internal_schedulers;
         item != NULL; item = item->next)
        if (item->id == scheduler_id)
        {
            found = item;
            break;
        }
    clm_async_internal_unlock_registry();
    return found;
}

clm_int clm_async_internal_scheduler_create_auto(
    clm_id* out_scheduler_id)
{
    if (out_scheduler_id == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    clm_async_internal_lock_registry();
    for (clm_id candidate = ~(clm_id)0; candidate != 0; --candidate)
    {
        bool used = false;
        for (clm_async_internal_scheduler* item = clm_async_internal_schedulers;
             item != NULL; item = item->next)
            if (item->id == candidate)
            {
                used = true;
                break;
            }
        if (used)
            continue;
        clm_async_internal_scheduler* scheduler =
            calloc(1, sizeof(*scheduler));
        if (scheduler == NULL)
        {
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

clm_async_internal_task* clm_async_internal_find_task(
    const clm_async_internal_scheduler* scheduler,
    const clm_id task_id)
{
    for (clm_async_internal_task* task = scheduler->tasks; task != NULL;
         task = task->next)
        if (task->id == task_id)
            return task;
    return NULL;
}

void clm_async_internal_notify(const clm_async_internal_task* task)
{
    if (task->on_state != NULL)
        task->on_state(task->id, task->generation, task->state, task->on_state_context);
}

void clm_async_internal_enqueue(
    clm_async_internal_scheduler* scheduler,
    clm_async_internal_task* task
)
{
    task->ready_next = NULL;
    if (scheduler->ready_tail == NULL)
        scheduler->ready_head = task;
    else
        scheduler->ready_tail->ready_next = task;
    scheduler->ready_tail = task;
}

void clm_async_internal_remove_ready(
    clm_async_internal_scheduler* scheduler,
    const clm_async_internal_task* task)
{
    clm_async_internal_task* previous = NULL;
    for (clm_async_internal_task* current = scheduler->ready_head;
         current != NULL; current = current->ready_next)
    {
        if (current == task)
        {
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

void clm_async_internal_finish(
    clm_async_internal_task* task,
    const clm_async_task_state state)
{
    task->state = state;
    if (!task->cleanup_called)
    {
        task->cleanup_called = true;
        if (task->cleanup != NULL)
            task->cleanup(task->context_id);
    }
    clm_async_internal_notify(task);
}

clm_int clm_async_internal_spawn(
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

clm_int clm_async_scheduler_create(const clm_id scheduler_id)
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
        if (item->id == scheduler_id)
        {
            clm_async_internal_unlock_registry();
            free(scheduler);
            return CLM_ASYNC_OPERATION_BUSY;
        }
    scheduler->next = clm_async_internal_schedulers;
    clm_async_internal_schedulers = scheduler;
    clm_async_internal_unlock_registry();
    return CLM_ASYNC_OPERATION_SUCCESS;
}

clm_int clm_async_scheduler_destroy(const clm_id scheduler_id)
{
    clm_async_internal_lock_registry();
    clm_async_internal_scheduler** link = &clm_async_internal_schedulers;
    while (*link != NULL && (*link)->id != scheduler_id)
        link = &(*link)->next;
    clm_async_internal_scheduler* scheduler = *link;
    if (scheduler == NULL)
    {
        clm_async_internal_unlock_registry();
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    }
    if (scheduler->running || scheduler->destroying)
    {
        clm_async_internal_unlock_registry();
        return CLM_ASYNC_OPERATION_BUSY;
    }
    scheduler->destroying = true;
    *link = scheduler->next;
    clm_async_internal_unlock_registry();
    clm_async_internal_task* task = scheduler->tasks;
    while (task != NULL)
    {
        clm_async_internal_task* next = task->next;
        if (!clm_async_task_is_terminal(task->state))
            clm_async_internal_finish(task, CLM_ASYNC_TASK_CANCELLED);
        free(task);
        task = next;
    }
    free(scheduler);
    return CLM_ASYNC_OPERATION_SUCCESS;
}

clm_int clm_async_spawn(
    const clm_id scheduler_id,
    const clm_id task_id,
    const clm_async_step_fn step,
    const clm_id context_id,
    const clm_async_context_cleanup_fn cleanup)
{
    return clm_async_internal_spawn(scheduler_id, task_id, step, context_id, cleanup, NULL, NULL);
}

clm_int clm_async_run(
    const clm_id scheduler_id,
    const size_t max_steps,
    size_t* out_steps)
{
    clm_async_internal_scheduler* scheduler = clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (max_steps == 0)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    if (scheduler->running || scheduler->destroying)
        return CLM_ASYNC_OPERATION_BUSY;
    scheduler->running = true;
    size_t steps = 0;
    while (steps < max_steps && scheduler->ready_head != NULL)
    {
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
        if (task->cancel_requested || result == CLM_ASYNC_STEP_FAIL)
        {
            clm_async_internal_finish(task, task->cancel_requested ? CLM_ASYNC_TASK_CANCELLED : CLM_ASYNC_TASK_FAILED);
        }
        else if (result == CLM_ASYNC_STEP_COMPLETE)
        {
            clm_async_internal_finish(task, CLM_ASYNC_TASK_COMPLETED);
        }
        else if (result == CLM_ASYNC_STEP_YIELD ||
            result == CLM_ASYNC_STEP_WAIT)
        {
            if (result == CLM_ASYNC_STEP_YIELD || task->pending_wake)
            {
                task->pending_wake = false;
                task->state = CLM_ASYNC_TASK_READY;
                clm_async_internal_enqueue(scheduler, task);
            }
            else
            {
                task->state = CLM_ASYNC_TASK_WAITING;
            }
            clm_async_internal_notify(task);
        }
        else
        {
            clm_async_internal_finish(task, CLM_ASYNC_TASK_FAILED);
        }
    }
    scheduler->running = false;
    if (out_steps != NULL)
    {
        *out_steps = steps;
    }
    return CLM_ASYNC_OPERATION_SUCCESS;
}

clm_int clm_async_wake(
    const clm_id scheduler_id,
    const clm_id task_id)
{
    clm_async_internal_scheduler* scheduler = clm_async_internal_find_scheduler(scheduler_id);
    if (scheduler == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    clm_async_internal_task* task = clm_async_internal_find_task(scheduler, task_id);
    if (task == NULL)
        return CLM_ASYNC_OPERATION_NOT_FOUND;
    if (task->state == CLM_ASYNC_TASK_WAITING)
    {
        task->pending_wake = false;
        task->state = CLM_ASYNC_TASK_READY;
        clm_async_internal_enqueue(scheduler, task);
        clm_async_internal_notify(task);
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    if (task->state == CLM_ASYNC_TASK_READY ||
        task->state == CLM_ASYNC_TASK_RUNNING)
    {
        task->pending_wake = true;
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    return CLM_ASYNC_OPERATION_INVALID_STATE;
}

clm_int clm_async_task_get_token(
    const clm_id scheduler_id,
    const clm_id task_id,
    clm_async_task_token* out_token)
{
    if (out_token == NULL)
        return CLM_ASYNC_OPERATION_INVALID_STATE;
    const clm_async_internal_scheduler* scheduler = clm_async_internal_find_scheduler(scheduler_id);
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

clm_int clm_async_wake_token(
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

clm_int clm_async_cancel(
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
    if (task->state == CLM_ASYNC_TASK_RUNNING)
    {
        task->cancel_requested = true;
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    if (task->state == CLM_ASYNC_TASK_READY ||
        task->state == CLM_ASYNC_TASK_WAITING)
    {
        clm_async_internal_remove_ready(scheduler, task);
        clm_async_internal_finish(task, CLM_ASYNC_TASK_CANCELLED);
        return CLM_ASYNC_OPERATION_SUCCESS;
    }
    return CLM_ASYNC_OPERATION_INVALID_STATE;
}

clm_int clm_async_get_state(
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

clm_int clm_async_task_release(
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