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

#include "async.hpp"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

typedef enum clm_async_overflow_policy {
    CLM_ASYNC_OVERFLOW_REJECT = 0,
    CLM_ASYNC_OVERFLOW_QUEUE = 1
} clm_async_overflow_policy;

typedef enum clm_async_worker_state {
    CLM_ASYNC_WORKER_AVAILABLE = 0,
    CLM_ASYNC_WORKER_BUSY = 1,
    CLM_ASYNC_WORKER_STOPPING = 2,
    CLM_ASYNC_WORKER_STOPPED = 3
} clm_async_worker_state;

typedef struct clm_async_pool_config {
    std::size_t worker_count;
    std::size_t max_tasks_per_worker;
    std::size_t max_pending_tasks;
    std::size_t steps_per_cycle;
    clm_async_overflow_policy overflow_policy;
} clm_async_pool_config;

typedef struct clm_async_pool_stats {
    std::size_t pending;
    std::size_t ready;
    std::size_t running;
    std::size_t waiting;
    std::size_t terminal;
} clm_async_pool_stats;

#define CLM_ASYNC_NO_WORKER ((std::size_t)-1)

namespace clm::async_pool {

inline constexpr clm_int SUCCESS = CLM_ASYNC_OPERATION_SUCCESS;
inline constexpr clm_int FAILURE = CLM_ASYNC_OPERATION_FAILURE;
inline constexpr clm_int BUSY = CLM_ASYNC_OPERATION_BUSY;
inline constexpr clm_int QUEUE_FULL = CLM_ASYNC_OPERATION_QUEUE_FULL;
inline constexpr clm_int NOT_FOUND = CLM_ASYNC_OPERATION_NOT_FOUND;
inline constexpr clm_int INVALID_STATE = CLM_ASYNC_OPERATION_INVALID_STATE;
inline constexpr clm_int STOPPING = CLM_ASYNC_OPERATION_STOPPING;

inline constexpr std::size_t NO_WORKER = CLM_ASYNC_NO_WORKER;

using overflow_policy = clm_async_overflow_policy;
using worker_state = clm_async_worker_state;
using pool_config = clm_async_pool_config;
using pool_stats = clm_async_pool_stats;

namespace detail {

struct pool_task;
struct pool_worker;
struct pool_internal;

enum class command_type {
    SPAWN,
    WAKE,
    CANCEL,
    RELEASE
};

struct pool_command {
    command_type type{command_type::SPAWN};
    pool_task* task{nullptr};
};

struct pool_task {
    clm_id id{0};
    clm_id internal_id{0};
    clm_ulong generation{0};
    clm_id context_id{0};
    clm_async_step_fn step{nullptr};
    clm_async_context_cleanup_fn cleanup{nullptr};
    clm_async_task_state state{CLM_ASYNC_TASK_QUEUED};
    std::size_t worker_index{CLM_ASYNC_NO_WORKER};
    bool pending_wake{false};
    bool counted_active{false};
    bool cancel_dispatched{false};
};

struct pool_worker {
    std::size_t index{0};
    pool_internal* pool{nullptr};
    clm_id scheduler_id{0};
    std::thread thread;
    bool accepting{true};
    bool started{false};
    std::size_t active_count{0};
    std::deque<pool_command> mailbox;
};

struct pool_internal {
    clm_id id{0};
    pool_config config{};
    clm_id next_internal_id{1};
    std::size_t next_worker_index{0};
    bool stopping{false};

    std::mutex mtx;
    std::condition_variable cv;

    std::vector<std::unique_ptr<pool_worker>> workers;
    std::unordered_map<clm_id, std::unique_ptr<pool_task>> tasks;
    std::deque<clm_id> pending_tasks;
};

class pool_registry {
public:
    static pool_registry& instance()
    {
        static pool_registry reg;
        return reg;
    }

    std::shared_ptr<pool_internal> find(const clm_id id)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        auto it = pools_.find(id);
        return (it != pools_.end()) ? it->second : nullptr;
    }

    clm_int create(const clm_id id, const pool_config& config, std::shared_ptr<pool_internal>& out_pool)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (pools_.contains(id))
            return BUSY;
        auto p = std::make_shared<pool_internal>();
        p->id = id;
        p->config = config;
        pools_.emplace(id, p);
        out_pool = p;
        return SUCCESS;
    }

    clm_int remove(const clm_id id)
    {
        std::lock_guard<std::mutex> lk(mtx_);
        return pools_.erase(id) > 0 ? SUCCESS : NOT_FOUND;
    }

private:
    std::mutex mtx_;
    std::unordered_map<clm_id, std::shared_ptr<pool_internal>> pools_;
};

inline void state_callback(
    const clm_id /*task_id*/,
    const clm_ulong /*generation*/,
    const clm_async_task_state state,
    void* context)
{
    auto* task = static_cast<pool_task*>(context);
    if (!task)
        return;
    // Synchronize task state with pool
    task->state = state;
}

inline void drain_pending_locked(pool_internal* pool);

inline void worker_thread_func(pool_worker* worker)
{
    pool_internal* pool = worker->pool;
    if (clm::async::detail::scheduler_registry::instance().create_auto(&worker->scheduler_id) != SUCCESS) {
        std::lock_guard<std::mutex> lk(pool->mtx);
        pool->stopping = true;
        pool->cv.notify_all();
        return;
    }

    {
        std::lock_guard<std::mutex> lk(pool->mtx);
        worker->started = true;
        pool->cv.notify_all();
    }

    for (;;) {
        pool_command cmd{};
        bool has_cmd = false;
        pool_task* cancel_task = nullptr;

        {
            std::unique_lock<std::mutex> lk(pool->mtx);
            if (!worker->mailbox.empty()) {
                cmd = worker->mailbox.front();
                worker->mailbox.pop_front();
                has_cmd = true;
            } else if (pool->stopping) {
                // Find any task on this worker needing cancellation
                for (auto& [_, t] : pool->tasks) {
                    if (t->worker_index == worker->index && t->counted_active && !t->cancel_dispatched) {
                        t->cancel_dispatched = true;
                        cancel_task = t.get();
                        break;
                    }
                }
            }

            if (!has_cmd && !cancel_task && pool->stopping && worker->active_count == 0) {
                break;
            }
        }

        if (has_cmd) {
            if (cmd.type == command_type::SPAWN) {
                clm_async_internal_state_fn on_state = state_callback;
                clm::async::detail::internal_spawn(
                    worker->scheduler_id,
                    cmd.task->internal_id,
                    cmd.task->step,
                    cmd.task->context_id,
                    cmd.task->cleanup,
                    on_state,
                    cmd.task);
                if (cmd.task->pending_wake) {
                    cmd.task->pending_wake = false;
                    clm::async::wake(worker->scheduler_id, cmd.task->internal_id);
                }
            } else if (cmd.type == command_type::WAKE) {
                clm::async::wake(worker->scheduler_id, cmd.task->internal_id);
            } else if (cmd.type == command_type::CANCEL) {
                clm::async::cancel(worker->scheduler_id, cmd.task->internal_id);
            } else if (cmd.type == command_type::RELEASE) {
                clm::async::task_release(worker->scheduler_id, cmd.task->internal_id);
            }
            continue;
        }

        if (cancel_task) {
            clm::async::cancel(worker->scheduler_id, cancel_task->internal_id);
            continue;
        }

        // Run scheduler steps
        std::size_t steps_executed = 0;
        clm::async::run(worker->scheduler_id, pool->config.steps_per_cycle, &steps_executed);

        // Update active count & check if completed tasks free up capacity
        {
            std::lock_guard<std::mutex> lk(pool->mtx);
            for (auto& [_, t] : pool->tasks) {
                if (t->worker_index == worker->index && t->counted_active && clm_async_task_is_terminal(t->state)) {
                    t->counted_active = false;
                    if (worker->active_count > 0)
                        --worker->active_count;
                }
            }
            drain_pending_locked(pool);
            pool->cv.notify_all();
        }

        if (steps_executed == 0) {
            std::unique_lock<std::mutex> lk(pool->mtx);
            if (worker->mailbox.empty() && !(pool->stopping && worker->active_count == 0)) {
                pool->cv.wait_for(lk, std::chrono::milliseconds(5));
            }
        }
    }

    clm::async::scheduler_destroy(worker->scheduler_id);
}

inline clm_int assign_locked(pool_internal* pool, pool_task* task)
{
    const std::size_t count = pool->workers.size();
    for (std::size_t offset = 0; offset < count; ++offset) {
        const std::size_t idx = (pool->next_worker_index + offset) % count;
        auto& worker = pool->workers[idx];
        if (worker->accepting && worker->active_count < pool->config.max_tasks_per_worker) {
            pool->next_worker_index = (idx + 1) % count;
            task->worker_index = idx;
            task->counted_active = true;
            task->state = CLM_ASYNC_TASK_READY;
            ++worker->active_count;
            worker->mailbox.push_back(pool_command{command_type::SPAWN, task});
            pool->cv.notify_all();
            return SUCCESS;
        }
    }
    return BUSY;
}

inline void drain_pending_locked(pool_internal* pool)
{
    while (!pool->pending_tasks.empty()) {
        const clm_id tid = pool->pending_tasks.front();
        auto it = pool->tasks.find(tid);
        if (it == pool->tasks.end()) {
            pool->pending_tasks.pop_front();
            continue;
        }
        pool_task* task = it->second.get();
        if (assign_locked(pool, task) == SUCCESS) {
            pool->pending_tasks.pop_front();
        } else {
            break;
        }
    }
}

} // namespace detail

inline clm_int pool_create(const clm_id pool_id, const pool_config* config)
{
    if (!config || config->worker_count == 0 || config->max_tasks_per_worker == 0 ||
        config->steps_per_cycle == 0 ||
        (config->overflow_policy != CLM_ASYNC_OVERFLOW_REJECT &&
         config->overflow_policy != CLM_ASYNC_OVERFLOW_QUEUE) ||
        (config->overflow_policy == CLM_ASYNC_OVERFLOW_QUEUE && config->max_pending_tasks == 0))
        return INVALID_STATE;

    std::shared_ptr<detail::pool_internal> pool;
    const clm_int status = detail::pool_registry::instance().create(pool_id, *config, pool);
    if (status != SUCCESS)
        return status;

    pool->workers.reserve(config->worker_count);
    for (std::size_t i = 0; i < config->worker_count; ++i) {
        auto w = std::make_unique<detail::pool_worker>();
        w->index = i;
        w->pool = pool.get();
        w->accepting = true;
        w->thread = std::thread(detail::worker_thread_func, w.get());
        pool->workers.push_back(std::move(w));
    }

    // Wait until all workers have started
    std::unique_lock<std::mutex> lk(pool->mtx);
    pool->cv.wait(lk, [&]() {
        for (const auto& w : pool->workers)
            if (!w->started && !pool->stopping)
                return false;
        return true;
    });

    return SUCCESS;
}

inline clm_int pool_destroy(const clm_id pool_id)
{
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    {
        std::lock_guard<std::mutex> lk(pool->mtx);
        pool->stopping = true;
        pool->cv.notify_all();
    }

    for (auto& w : pool->workers) {
        if (w->thread.joinable())
            w->thread.join();
    }

    detail::pool_registry::instance().remove(pool_id);
    return SUCCESS;
}

inline clm_int pool_spawn(
    const clm_id pool_id,
    const clm_id task_id,
    const clm_async_step_fn step,
    const clm_id context_id,
    const clm_async_context_cleanup_fn cleanup)
{
    if (!step)
        return INVALID_STATE;
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    if (pool->stopping)
        return STOPPING;
    if (pool->tasks.contains(task_id))
        return BUSY;

    const clm_ulong gen = clm::async::detail::next_generation();
    if (gen == 0)
        return FAILURE;

    auto t = std::make_unique<detail::pool_task>();
    t->id = task_id;
    t->internal_id = pool->next_internal_id++;
    t->generation = gen;
    t->context_id = context_id;
    t->step = step;
    t->cleanup = cleanup;
    t->worker_index = CLM_ASYNC_NO_WORKER;

    detail::pool_task* raw_task = t.get();
    const clm_int assign_status = detail::assign_locked(pool.get(), raw_task);
    if (assign_status == SUCCESS) {
        pool->tasks.emplace(task_id, std::move(t));
        return SUCCESS;
    }

    if (pool->config.overflow_policy == CLM_ASYNC_OVERFLOW_QUEUE) {
        if (pool->pending_tasks.size() >= pool->config.max_pending_tasks)
            return QUEUE_FULL;
        t->state = CLM_ASYNC_TASK_QUEUED;
        pool->pending_tasks.push_back(task_id);
        pool->tasks.emplace(task_id, std::move(t));
        return SUCCESS;
    }

    return BUSY;
}

inline clm_int pool_wake(const clm_id pool_id, const clm_id task_id)
{
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    auto it = pool->tasks.find(task_id);
    if (it == pool->tasks.end())
        return NOT_FOUND;

    detail::pool_task* task = it->second.get();
    if (task->worker_index == CLM_ASYNC_NO_WORKER) {
        task->pending_wake = true;
        return SUCCESS;
    }

    auto& worker = pool->workers[task->worker_index];
    worker->mailbox.push_back(detail::pool_command{detail::command_type::WAKE, task});
    pool->cv.notify_all();
    return SUCCESS;
}

inline clm_int pool_task_get_token(
    const clm_id pool_id,
    const clm_id task_id,
    clm_async_task_token* out_token)
{
    if (!out_token)
        return INVALID_STATE;
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    auto it = pool->tasks.find(task_id);
    if (it == pool->tasks.end())
        return NOT_FOUND;

    out_token->task_id = it->second->id;
    out_token->generation = it->second->generation;
    return SUCCESS;
}

inline clm_int pool_wake_token(const clm_id pool_id, const clm_async_task_token token)
{
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    auto it = pool->tasks.find(token.task_id);
    if (it == pool->tasks.end() || it->second->generation != token.generation)
        return NOT_FOUND;

    detail::pool_task* task = it->second.get();
    if (task->worker_index == CLM_ASYNC_NO_WORKER) {
        task->pending_wake = true;
        return SUCCESS;
    }

    auto& worker = pool->workers[task->worker_index];
    worker->mailbox.push_back(detail::pool_command{detail::command_type::WAKE, task});
    pool->cv.notify_all();
    return SUCCESS;
}

inline clm_int pool_cancel(const clm_id pool_id, const clm_id task_id)
{
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    auto it = pool->tasks.find(task_id);
    if (it == pool->tasks.end())
        return NOT_FOUND;

    detail::pool_task* task = it->second.get();
    if (task->worker_index == CLM_ASYNC_NO_WORKER) {
        std::erase(pool->pending_tasks, task_id);
        task->state = CLM_ASYNC_TASK_CANCELLED;
        if (task->cleanup)
            task->cleanup(task->context_id);
        return SUCCESS;
    }

    auto& worker = pool->workers[task->worker_index];
    worker->mailbox.push_back(detail::pool_command{detail::command_type::CANCEL, task});
    pool->cv.notify_all();
    return SUCCESS;
}

inline clm_int pool_get_state(
    const clm_id pool_id,
    const clm_id task_id,
    clm_async_task_state* out_state)
{
    if (!out_state)
        return INVALID_STATE;
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    auto it = pool->tasks.find(task_id);
    if (it == pool->tasks.end())
        return NOT_FOUND;

    *out_state = it->second->state;
    return SUCCESS;
}

inline clm_int pool_task_get_worker_index(
    const clm_id pool_id,
    const clm_id task_id,
    std::size_t* out_worker_index)
{
    if (!out_worker_index)
        return INVALID_STATE;
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    auto it = pool->tasks.find(task_id);
    if (it == pool->tasks.end())
        return NOT_FOUND;

    *out_worker_index = it->second->worker_index;
    return SUCCESS;
}

inline clm_int pool_task_release(const clm_id pool_id, const clm_id task_id)
{
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    auto it = pool->tasks.find(task_id);
    if (it == pool->tasks.end())
        return NOT_FOUND;

    detail::pool_task* task = it->second.get();
    if (!clm_async_task_is_terminal(task->state))
        return INVALID_STATE;

    if (task->worker_index != CLM_ASYNC_NO_WORKER) {
        auto& worker = pool->workers[task->worker_index];
        worker->mailbox.push_back(detail::pool_command{detail::command_type::RELEASE, task});
        pool->cv.notify_all();
    }

    pool->tasks.erase(it);
    return SUCCESS;
}

inline clm_int pool_worker_set_accepting(
    const clm_id pool_id,
    const std::size_t worker_index,
    const bool accepting)
{
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    if (worker_index >= pool->workers.size())
        return NOT_FOUND;

    pool->workers[worker_index]->accepting = accepting;
    if (accepting)
        detail::drain_pending_locked(pool.get());
    return SUCCESS;
}

inline clm_int pool_worker_get_state(
    const clm_id pool_id,
    const std::size_t worker_index,
    clm_async_worker_state* out_state)
{
    if (!out_state)
        return INVALID_STATE;
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    if (worker_index >= pool->workers.size())
        return NOT_FOUND;

    const auto& w = pool->workers[worker_index];
    if (pool->stopping) {
        *out_state = CLM_ASYNC_WORKER_STOPPING;
    } else if (!w->accepting || w->active_count >= pool->config.max_tasks_per_worker) {
        *out_state = CLM_ASYNC_WORKER_BUSY;
    } else {
        *out_state = CLM_ASYNC_WORKER_AVAILABLE;
    }
    return SUCCESS;
}

inline clm_int pool_worker_count(const clm_id pool_id, std::size_t* out_count)
{
    if (!out_count)
        return INVALID_STATE;
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    *out_count = pool->workers.size();
    return SUCCESS;
}

inline clm_int pool_get_stats(const clm_id pool_id, clm_async_pool_stats* out_stats)
{
    if (!out_stats)
        return INVALID_STATE;
    auto pool = detail::pool_registry::instance().find(pool_id);
    if (!pool)
        return NOT_FOUND;

    std::lock_guard<std::mutex> lk(pool->mtx);
    out_stats->pending = pool->pending_tasks.size();
    out_stats->ready = 0;
    out_stats->running = 0;
    out_stats->waiting = 0;
    out_stats->terminal = 0;

    for (const auto& [_, t] : pool->tasks) {
        switch (t->state) {
        case CLM_ASYNC_TASK_READY:
            ++out_stats->ready;
            break;
        case CLM_ASYNC_TASK_RUNNING:
            ++out_stats->running;
            break;
        case CLM_ASYNC_TASK_WAITING:
            ++out_stats->waiting;
            break;
        case CLM_ASYNC_TASK_COMPLETED:
        case CLM_ASYNC_TASK_FAILED:
        case CLM_ASYNC_TASK_CANCELLED:
            ++out_stats->terminal;
            break;
        default:
            break;
        }
    }

    return SUCCESS;
}

} // namespace clm::async_pool

static inline clm_int clm_async_pool_create(const clm_id pool_id, const clm_async_pool_config* config)
{
    return clm::async_pool::pool_create(pool_id, config);
}

static inline clm_int clm_async_pool_destroy(const clm_id pool_id)
{
    return clm::async_pool::pool_destroy(pool_id);
}

static inline clm_int clm_async_pool_spawn(
    const clm_id pool_id,
    const clm_id task_id,
    const clm_async_step_fn step,
    const clm_id context_id,
    const clm_async_context_cleanup_fn cleanup)
{
    return clm::async_pool::pool_spawn(pool_id, task_id, step, context_id, cleanup);
}

static inline clm_int clm_async_pool_wake(const clm_id pool_id, const clm_id task_id)
{
    return clm::async_pool::pool_wake(pool_id, task_id);
}

static inline clm_int clm_async_pool_task_get_token(
    const clm_id pool_id,
    const clm_id task_id,
    clm_async_task_token* out_token)
{
    return clm::async_pool::pool_task_get_token(pool_id, task_id, out_token);
}

static inline clm_int clm_async_pool_wake_token(
    const clm_id pool_id,
    const clm_async_task_token token)
{
    return clm::async_pool::pool_wake_token(pool_id, token);
}

static inline clm_int clm_async_pool_cancel(const clm_id pool_id, const clm_id task_id)
{
    return clm::async_pool::pool_cancel(pool_id, task_id);
}

static inline clm_int clm_async_pool_get_state(
    const clm_id pool_id,
    const clm_id task_id,
    clm_async_task_state* out_state)
{
    return clm::async_pool::pool_get_state(pool_id, task_id, out_state);
}

static inline clm_int clm_async_pool_task_get_worker_index(
    const clm_id pool_id,
    const clm_id task_id,
    std::size_t* out_worker_index)
{
    return clm::async_pool::pool_task_get_worker_index(pool_id, task_id, out_worker_index);
}

static inline clm_int clm_async_pool_task_release(const clm_id pool_id, const clm_id task_id)
{
    return clm::async_pool::pool_task_release(pool_id, task_id);
}

static inline clm_int clm_async_pool_worker_set_accepting(
    const clm_id pool_id,
    const std::size_t worker_index,
    const bool accepting)
{
    return clm::async_pool::pool_worker_set_accepting(pool_id, worker_index, accepting);
}

static inline clm_int clm_async_pool_worker_get_state(
    const clm_id pool_id,
    const std::size_t worker_index,
    clm_async_worker_state* out_state)
{
    return clm::async_pool::pool_worker_get_state(pool_id, worker_index, out_state);
}

static inline clm_int clm_async_pool_worker_count(const clm_id pool_id, std::size_t* out_count)
{
    return clm::async_pool::pool_worker_count(pool_id, out_count);
}

static inline clm_int clm_async_pool_get_stats(const clm_id pool_id, clm_async_pool_stats* out_stats)
{
    return clm::async_pool::pool_get_stats(pool_id, out_stats);
}

