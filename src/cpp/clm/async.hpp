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

#pragma once

#include "base.hpp"

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#define CLM_ASYNC_OPERATION_SUCCESS        0
#define CLM_ASYNC_OPERATION_FAILURE       -1
#define CLM_ASYNC_OPERATION_BUSY          -2
#define CLM_ASYNC_OPERATION_QUEUE_FULL    -3
#define CLM_ASYNC_OPERATION_NOT_FOUND     -4
#define CLM_ASYNC_OPERATION_INVALID_STATE -5
#define CLM_ASYNC_OPERATION_STOPPING      -6

typedef enum clm_async_step_result {
    CLM_ASYNC_STEP_YIELD = 0,
    CLM_ASYNC_STEP_WAIT = 1,
    CLM_ASYNC_STEP_COMPLETE = 2,
    CLM_ASYNC_STEP_FAIL = 3
} clm_async_step_result;

typedef enum clm_async_task_state {
    CLM_ASYNC_TASK_READY = 0,
    CLM_ASYNC_TASK_RUNNING = 1,
    CLM_ASYNC_TASK_WAITING = 2,
    CLM_ASYNC_TASK_COMPLETED = 3,
    CLM_ASYNC_TASK_FAILED = 4,
    CLM_ASYNC_TASK_CANCELLED = 5,
    CLM_ASYNC_TASK_QUEUED = 6
} clm_async_task_state;

static inline bool clm_async_task_is_terminal(const clm_async_task_state state)
{
    return state == CLM_ASYNC_TASK_COMPLETED ||
           state == CLM_ASYNC_TASK_FAILED ||
           state == CLM_ASYNC_TASK_CANCELLED;
}

typedef struct clm_async_task_token {
    clm_id task_id{0};
    clm_ulong generation{0};
} clm_async_task_token;

typedef clm_async_step_result (*clm_async_step_fn)(clm_id task_id, clm_id context_id);
typedef void (*clm_async_context_cleanup_fn)(clm_id context_id);
typedef void (*clm_async_internal_state_fn)(
    clm_id task_id,
    clm_ulong generation,
    clm_async_task_state state,
    void* context);

namespace clm::async {

inline constexpr clm_int SUCCESS = CLM_ASYNC_OPERATION_SUCCESS;
inline constexpr clm_int FAILURE = CLM_ASYNC_OPERATION_FAILURE;
inline constexpr clm_int BUSY = CLM_ASYNC_OPERATION_BUSY;
inline constexpr clm_int QUEUE_FULL = CLM_ASYNC_OPERATION_QUEUE_FULL;
inline constexpr clm_int NOT_FOUND = CLM_ASYNC_OPERATION_NOT_FOUND;
inline constexpr clm_int INVALID_STATE = CLM_ASYNC_OPERATION_INVALID_STATE;
inline constexpr clm_int STOPPING = CLM_ASYNC_OPERATION_STOPPING;

using step_result = clm_async_step_result;
using task_state = clm_async_task_state;
using task_token = clm_async_task_token;
using step_fn = clm_async_step_fn;
using context_cleanup_fn = clm_async_context_cleanup_fn;
using state_fn = clm_async_internal_state_fn;

inline bool is_terminal(const task_state state)
{
    return clm_async_task_is_terminal(state);
}

namespace detail {

inline std::atomic<clm_ulong> g_generation_sequence{1};

inline clm_ulong next_generation()
{
    clm_ulong old = g_generation_sequence.load(std::memory_order_relaxed);
    while (old != 0 && old != static_cast<clm_ulong>(~0ULL)) {
        if (g_generation_sequence.compare_exchange_weak(
                old, old + 1, std::memory_order_relaxed, std::memory_order_relaxed))
            return old;
    }
    return 0;
}

struct internal_task {
    clm_id id{0};
    clm_ulong generation{0};
    clm_id context_id{0};
    step_fn step{nullptr};
    context_cleanup_fn cleanup{nullptr};
    state_fn on_state{nullptr};
    void* on_state_context{nullptr};
    task_state state{CLM_ASYNC_TASK_READY};
    bool pending_wake{false};
    bool cancel_requested{false};
    bool cleanup_called{false};
    bool callback_active{false};
};

struct internal_scheduler {
    clm_id id{0};
    bool running{false};
    bool destroying{false};
    std::unordered_map<clm_id, std::unique_ptr<internal_task>> tasks;
    std::deque<clm_id> ready_queue;
};

class scheduler_registry {
public:
    static scheduler_registry& instance()
    {
        static scheduler_registry reg;
        return reg;
    }

    std::unique_lock<std::mutex> lock()
    {
        return std::unique_lock<std::mutex>(mtx_);
    }

    internal_scheduler* find_locked(const clm_id id)
    {
        auto it = schedulers_.find(id);
        return (it != schedulers_.end()) ? it->second.get() : nullptr;
    }

    internal_scheduler* find(const clm_id id)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        return find_locked(id);
    }

    clm_int create_auto(clm_id* out_id)
    {
        if (!out_id)
            return INVALID_STATE;
        std::lock_guard<std::mutex> lk(mtx_);
        for (clm_id candidate = ~0ULL; candidate != 0; --candidate) {
            if (!schedulers_.contains(candidate)) {
                auto s = std::make_unique<internal_scheduler>();
                s->id = candidate;
                schedulers_.emplace(candidate, std::move(s));
                *out_id = candidate;
                return SUCCESS;
            }
        }
        return FAILURE;
    }

    clm_int create(const clm_id id)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (schedulers_.contains(id))
            return BUSY;
        auto s = std::make_unique<internal_scheduler>();
        s->id = id;
        schedulers_.emplace(id, std::move(s));
        return SUCCESS;
    }

    clm_int destroy(const clm_id id)
    {
        std::unique_ptr<internal_scheduler> s;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = schedulers_.find(id);
            if (it == schedulers_.end())
                return NOT_FOUND;
            if (it->second->running || it->second->destroying)
                return BUSY;
            it->second->destroying = true;
            s = std::move(it->second);
            schedulers_.erase(it);
        }

        for (auto& [_, task] : s->tasks) {
            if (!is_terminal(task->state)) {
                task->state = CLM_ASYNC_TASK_CANCELLED;
                if (!task->cleanup_called) {
                    task->cleanup_called = true;
                    if (task->cleanup)
                        task->cleanup(task->context_id);
                }
                if (task->on_state)
                    task->on_state(task->id, task->generation, task->state, task->on_state_context);
            }
        }
        return SUCCESS;
    }

private:
    std::mutex mtx_;
    std::unordered_map<clm_id, std::unique_ptr<internal_scheduler>> schedulers_;
};

inline void notify(internal_task* task)
{
    if (task && task->on_state)
        task->on_state(task->id, task->generation, task->state, task->on_state_context);
}

inline void finish(internal_task* task, const task_state state)
{
    task->state = state;
    if (!task->cleanup_called) {
        task->cleanup_called = true;
        if (task->cleanup)
            task->cleanup(task->context_id);
    }
    notify(task);
}

inline clm_int internal_spawn(
    const clm_id scheduler_id,
    const clm_id task_id,
    const step_fn step,
    const clm_id context_id,
    const context_cleanup_fn cleanup,
    const state_fn on_state,
    void* on_state_context)
{
    auto lk = scheduler_registry::instance().lock();
    auto* scheduler = scheduler_registry::instance().find_locked(scheduler_id);
    if (!scheduler)
        return NOT_FOUND;
    if (!step || scheduler->destroying)
        return INVALID_STATE;
    if (scheduler->tasks.contains(task_id))
        return BUSY;

    const clm_ulong gen = next_generation();
    if (gen == 0)
        return FAILURE;

    auto t = std::make_unique<internal_task>();
    t->id = task_id;
    t->generation = gen;
    t->context_id = context_id;
    t->step = step;
    t->cleanup = cleanup;
    t->on_state = on_state;
    t->on_state_context = on_state_context;
    t->state = CLM_ASYNC_TASK_READY;

    scheduler->ready_queue.push_back(task_id);
    internal_task* raw_task = t.get();
    scheduler->tasks.emplace(task_id, std::move(t));

    notify(raw_task);
    return SUCCESS;
}

} // namespace detail

inline clm_int scheduler_create(const clm_id scheduler_id)
{
    return detail::scheduler_registry::instance().create(scheduler_id);
}

inline clm_int scheduler_destroy(const clm_id scheduler_id)
{
    return detail::scheduler_registry::instance().destroy(scheduler_id);
}

inline clm_int spawn(
    const clm_id scheduler_id,
    const clm_id task_id,
    const step_fn step,
    const clm_id context_id,
    const context_cleanup_fn cleanup)
{
    return detail::internal_spawn(scheduler_id, task_id, step, context_id, cleanup, nullptr, nullptr);
}

inline clm_int run(
    const clm_id scheduler_id,
    const std::size_t max_steps,
    std::size_t* out_steps)
{
    auto* scheduler = detail::scheduler_registry::instance().find(scheduler_id);
    if (!scheduler)
        return NOT_FOUND;
    if (max_steps == 0)
        return INVALID_STATE;
    if (scheduler->running || scheduler->destroying)
        return BUSY;

    scheduler->running = true;
    std::size_t steps = 0;

    while (steps < max_steps && !scheduler->ready_queue.empty()) {
        const clm_id tid = scheduler->ready_queue.front();
        scheduler->ready_queue.pop_front();

        auto it = scheduler->tasks.find(tid);
        if (it == scheduler->tasks.end())
            continue;
        detail::internal_task* task = it->second.get();
        if (task->state != CLM_ASYNC_TASK_READY)
            continue;

        task->state = CLM_ASYNC_TASK_RUNNING;
        task->callback_active = true;
        detail::notify(task);

        const step_result result = task->step(task->id, task->context_id);
        task->callback_active = false;
        ++steps;

        if (task->cancel_requested || result == CLM_ASYNC_STEP_FAIL) {
            detail::finish(task, task->cancel_requested ? CLM_ASYNC_TASK_CANCELLED : CLM_ASYNC_TASK_FAILED);
        } else if (result == CLM_ASYNC_STEP_COMPLETE) {
            detail::finish(task, CLM_ASYNC_TASK_COMPLETED);
        } else if (result == CLM_ASYNC_STEP_YIELD || result == CLM_ASYNC_STEP_WAIT) {
            if (result == CLM_ASYNC_STEP_YIELD || task->pending_wake) {
                task->pending_wake = false;
                task->state = CLM_ASYNC_TASK_READY;
                scheduler->ready_queue.push_back(task->id);
            } else {
                task->state = CLM_ASYNC_TASK_WAITING;
            }
            detail::notify(task);
        } else {
            detail::finish(task, CLM_ASYNC_TASK_FAILED);
        }
    }

    scheduler->running = false;
    if (out_steps)
        *out_steps = steps;
    return SUCCESS;
}

inline clm_int wake(const clm_id scheduler_id, const clm_id task_id)
{
    auto* scheduler = detail::scheduler_registry::instance().find(scheduler_id);
    if (!scheduler)
        return NOT_FOUND;
    auto it = scheduler->tasks.find(task_id);
    if (it == scheduler->tasks.end())
        return NOT_FOUND;
    detail::internal_task* task = it->second.get();

    if (task->state == CLM_ASYNC_TASK_WAITING) {
        task->pending_wake = false;
        task->state = CLM_ASYNC_TASK_READY;
        scheduler->ready_queue.push_back(task->id);
        detail::notify(task);
        return SUCCESS;
    }
    if (task->state == CLM_ASYNC_TASK_READY || task->state == CLM_ASYNC_TASK_RUNNING) {
        task->pending_wake = true;
        return SUCCESS;
    }
    return INVALID_STATE;
}

inline clm_int task_get_token(
    const clm_id scheduler_id,
    const clm_id task_id,
    task_token* out_token)
{
    if (!out_token)
        return INVALID_STATE;
    auto* scheduler = detail::scheduler_registry::instance().find(scheduler_id);
    if (!scheduler)
        return NOT_FOUND;
    auto it = scheduler->tasks.find(task_id);
    if (it == scheduler->tasks.end())
        return NOT_FOUND;
    out_token->task_id = it->second->id;
    out_token->generation = it->second->generation;
    return SUCCESS;
}

inline clm_int wake_token(const clm_id scheduler_id, const task_token token)
{
    auto* scheduler = detail::scheduler_registry::instance().find(scheduler_id);
    if (!scheduler)
        return NOT_FOUND;
    auto it = scheduler->tasks.find(token.task_id);
    if (it == scheduler->tasks.end() || it->second->generation != token.generation)
        return NOT_FOUND;
    return wake(scheduler_id, token.task_id);
}

inline clm_int cancel(const clm_id scheduler_id, const clm_id task_id)
{
    auto* scheduler = detail::scheduler_registry::instance().find(scheduler_id);
    if (!scheduler)
        return NOT_FOUND;
    auto it = scheduler->tasks.find(task_id);
    if (it == scheduler->tasks.end())
        return NOT_FOUND;
    detail::internal_task* task = it->second.get();

    if (task->state == CLM_ASYNC_TASK_RUNNING) {
        task->cancel_requested = true;
        return SUCCESS;
    }
    if (task->state == CLM_ASYNC_TASK_READY || task->state == CLM_ASYNC_TASK_WAITING) {
        auto& q = scheduler->ready_queue;
        std::erase(q, task_id);
        detail::finish(task, CLM_ASYNC_TASK_CANCELLED);
        return SUCCESS;
    }
    return INVALID_STATE;
}

inline clm_int get_state(
    const clm_id scheduler_id,
    const clm_id task_id,
    task_state* out_state)
{
    if (!out_state)
        return INVALID_STATE;
    auto* scheduler = detail::scheduler_registry::instance().find(scheduler_id);
    if (!scheduler)
        return NOT_FOUND;
    auto it = scheduler->tasks.find(task_id);
    if (it == scheduler->tasks.end())
        return NOT_FOUND;
    *out_state = it->second->state;
    return SUCCESS;
}

inline clm_int task_release(const clm_id scheduler_id, const clm_id task_id)
{
    auto* scheduler = detail::scheduler_registry::instance().find(scheduler_id);
    if (!scheduler)
        return NOT_FOUND;
    if (scheduler->running || scheduler->destroying)
        return BUSY;
    auto it = scheduler->tasks.find(task_id);
    if (it == scheduler->tasks.end())
        return NOT_FOUND;
    if (it->second->callback_active || !is_terminal(it->second->state))
        return INVALID_STATE;
    scheduler->tasks.erase(it);
    return SUCCESS;
}

/* --------------------------------------------------------------------------
 * C++20 Coroutine Facade
 * Allows writing native C++20 coroutines that run on the CLM async scheduler.
 * -------------------------------------------------------------------------- */
namespace coro {

struct task {
    struct promise_type {
        step_result last_step{CLM_ASYNC_STEP_YIELD};
        std::exception_ptr exception;

        task get_return_object()
        {
            return task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept
        {
            last_step = CLM_ASYNC_STEP_COMPLETE;
        }
        void unhandled_exception() noexcept
        {
            exception = std::current_exception();
            last_step = CLM_ASYNC_STEP_FAIL;
        }
    };

    std::coroutine_handle<promise_type> handle;

    task() = default;
    explicit task(std::coroutine_handle<promise_type> h) : handle(h) {}
    task(const task&) = delete;
    task& operator=(const task&) = delete;
    task(task&& other) noexcept : handle(other.handle) { other.handle = nullptr; }
    task& operator=(task&& other) noexcept
    {
        if (this != &other) {
            if (handle)
                handle.destroy();
            handle = other.handle;
            other.handle = nullptr;
        }
        return *this;
    }
    ~task()
    {
        if (handle)
            handle.destroy();
    }
};

struct yield_awaiter {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<task::promise_type> h) noexcept
    {
        h.promise().last_step = CLM_ASYNC_STEP_YIELD;
    }
    void await_resume() const noexcept {}
};

struct wait_awaiter {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<task::promise_type> h) noexcept
    {
        h.promise().last_step = CLM_ASYNC_STEP_WAIT;
    }
    void await_resume() const noexcept {}
};

inline yield_awaiter yield() { return {}; }
inline wait_awaiter wait() { return {}; }

} // namespace coro

} // namespace clm::async

static inline clm_int clm_async_scheduler_create(const clm_id scheduler_id)
{
    return clm::async::scheduler_create(scheduler_id);
}

static inline clm_int clm_async_scheduler_destroy(const clm_id scheduler_id)
{
    return clm::async::scheduler_destroy(scheduler_id);
}

static inline clm_int clm_async_spawn(
    const clm_id scheduler_id,
    const clm_id task_id,
    const clm_async_step_fn step,
    const clm_id context_id,
    const clm_async_context_cleanup_fn cleanup)
{
    return clm::async::spawn(scheduler_id, task_id, step, context_id, cleanup);
}

static inline clm_int clm_async_run(
    const clm_id scheduler_id,
    const std::size_t max_steps,
    std::size_t* out_steps)
{
    return clm::async::run(scheduler_id, max_steps, out_steps);
}

static inline clm_int clm_async_wake(const clm_id scheduler_id, const clm_id task_id)
{
    return clm::async::wake(scheduler_id, task_id);
}

static inline clm_int clm_async_task_get_token(
    const clm_id scheduler_id,
    const clm_id task_id,
    clm_async_task_token* out_token)
{
    return clm::async::task_get_token(scheduler_id, task_id, out_token);
}

static inline clm_int clm_async_wake_token(
    const clm_id scheduler_id,
    const clm_async_task_token token)
{
    return clm::async::wake_token(scheduler_id, token);
}

static inline clm_int clm_async_cancel(const clm_id scheduler_id, const clm_id task_id)
{
    return clm::async::cancel(scheduler_id, task_id);
}

static inline clm_int clm_async_get_state(
    const clm_id scheduler_id,
    const clm_id task_id,
    clm_async_task_state* out_state)
{
    return clm::async::get_state(scheduler_id, task_id, out_state);
}

static inline clm_int clm_async_task_release(
    const clm_id scheduler_id,
    const clm_id task_id)
{
    return clm::async::task_release(scheduler_id, task_id);
}

