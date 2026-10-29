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

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

#define CLM_CHANNEL_OPERATION_SUCCESS  0
#define CLM_CHANNEL_OPERATION_FAILURE -1
#define CLM_CHANNEL_OPERATION_EMPTY   -2
#define CLM_CHANNEL_OPERATION_FULL    -3
#define CLM_CHANNEL_OPERATION_CLOSED  -4
#define CLM_CHANNEL_OPERATION_TIMEOUT -5

#define CLM_CHANNEL_WAIT_FOREVER UINT64_MAX

typedef struct clm_channel_info {
    std::size_t capacity;
    std::size_t count;
    bool closed;
} clm_channel_info;

namespace clm::channel {

inline constexpr clm_int SUCCESS = CLM_CHANNEL_OPERATION_SUCCESS;
inline constexpr clm_int FAILURE = CLM_CHANNEL_OPERATION_FAILURE;
inline constexpr clm_int EMPTY = CLM_CHANNEL_OPERATION_EMPTY;
inline constexpr clm_int FULL = CLM_CHANNEL_OPERATION_FULL;
inline constexpr clm_int CLOSED = CLM_CHANNEL_OPERATION_CLOSED;
inline constexpr clm_int TIMEOUT = CLM_CHANNEL_OPERATION_TIMEOUT;
inline constexpr std::uint64_t WAIT_FOREVER = CLM_CHANNEL_WAIT_FOREVER;

using info = clm_channel_info;

namespace detail {

struct waiter {
    clm_id message_id{0};
    clm_id* out_id{nullptr};
    bool* out_open{nullptr};
    bool matched{false};
    bool closed{false};
};

struct channel_state {
    clm_id id{0};
    std::size_t capacity{0};
    bool closed{false};
    std::deque<clm_id> buffer;
    std::mutex mtx;
    std::condition_variable cv;
    std::list<waiter*> send_waiters;
    std::list<waiter*> receive_waiters;
};

class channel_registry {
public:
    static channel_registry& instance()
    {
        static channel_registry reg;
        return reg;
    }

    std::shared_ptr<channel_state> find(const clm_id id)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = channels_.find(id);
        return (it != channels_.end()) ? it->second : nullptr;
    }

    clm_int create(const clm_id id, const std::size_t capacity)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (channels_.find(id) != channels_.end())
            return FAILURE;
        auto state = std::make_shared<channel_state>();
        state->id = id;
        state->capacity = capacity;
        channels_.emplace(id, std::move(state));
        return SUCCESS;
    }

    clm_int destroy(const clm_id id)
    {
        std::shared_ptr<channel_state> ch;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            auto it = channels_.find(id);
            if (it == channels_.end())
                return FAILURE;
            ch = it->second;
            std::lock_guard<std::mutex> ch_lock(ch->mtx);
            if (!ch->send_waiters.empty() || !ch->receive_waiters.empty())
                return FAILURE;
            channels_.erase(it);
        }
        return SUCCESS;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mtx_);
        channels_.clear();
    }

private:
    std::mutex mtx_;
    std::unordered_map<clm_id, std::shared_ptr<channel_state>> channels_;
};

inline void drain_senders(channel_state& ch)
{
    while (!ch.closed && ch.capacity != 0 &&
           ch.buffer.size() < ch.capacity && !ch.send_waiters.empty()) {
        waiter* s = ch.send_waiters.front();
        ch.send_waiters.pop_front();
        ch.buffer.push_back(s->message_id);
        s->matched = true;
    }
    ch.cv.notify_all();
}

inline void drain_receivers(channel_state& ch)
{
    while (!ch.receive_waiters.empty() && !ch.buffer.empty()) {
        waiter* r = ch.receive_waiters.front();
        ch.receive_waiters.pop_front();
        if (r->out_id)
            *r->out_id = ch.buffer.front();
        if (r->out_open)
            *r->out_open = true;
        ch.buffer.pop_front();
        r->matched = true;
    }
    if (ch.closed && ch.buffer.empty()) {
        while (!ch.receive_waiters.empty()) {
            waiter* r = ch.receive_waiters.front();
            ch.receive_waiters.pop_front();
            if (r->out_id)
                *r->out_id = 0;
            if (r->out_open)
                *r->out_open = false;
            r->closed = true;
        }
    }
    ch.cv.notify_all();
}

inline clm_int try_send_locked(channel_state& ch, const clm_id message_id)
{
    if (ch.closed)
        return CLOSED;
    if (ch.capacity == 0) {
        if (!ch.receive_waiters.empty()) {
            waiter* r = ch.receive_waiters.front();
            ch.receive_waiters.pop_front();
            if (r->out_id)
                *r->out_id = message_id;
            if (r->out_open)
                *r->out_open = true;
            r->matched = true;
            ch.cv.notify_all();
            return SUCCESS;
        }
        return FULL;
    }
    if (ch.buffer.size() < ch.capacity) {
        ch.buffer.push_back(message_id);
        drain_receivers(ch);
        return SUCCESS;
    }
    return FULL;
}

inline clm_int try_receive_locked(channel_state& ch, clm_id* out_id, bool* out_open)
{
    if (ch.capacity == 0) {
        if (!ch.send_waiters.empty()) {
            waiter* s = ch.send_waiters.front();
            ch.send_waiters.pop_front();
            *out_id = s->message_id;
            *out_open = true;
            s->matched = true;
            ch.cv.notify_all();
            return SUCCESS;
        }
        if (ch.closed) {
            *out_id = 0;
            *out_open = false;
            return CLOSED;
        }
        return EMPTY;
    }
    if (!ch.buffer.empty()) {
        *out_id = ch.buffer.front();
        *out_open = true;
        ch.buffer.pop_front();
        drain_senders(ch);
        return SUCCESS;
    }
    if (ch.closed) {
        *out_id = 0;
        *out_open = false;
        return CLOSED;
    }
    return EMPTY;
}

} // namespace detail

inline clm_int create(const clm_id channel_id, const std::size_t capacity)
{
    return detail::channel_registry::instance().create(channel_id, capacity);
}

inline clm_int close(const clm_id channel_id)
{
    auto ch = detail::channel_registry::instance().find(channel_id);
    if (!ch)
        return FAILURE;
    std::lock_guard<std::mutex> lock(ch->mtx);
    if (ch->closed)
        return CLOSED;
    ch->closed = true;
    for (auto* s : ch->send_waiters)
        s->closed = true;
    ch->send_waiters.clear();
    detail::drain_receivers(*ch);
    ch->cv.notify_all();
    return SUCCESS;
}

inline clm_int destroy(const clm_id channel_id)
{
    return detail::channel_registry::instance().destroy(channel_id);
}

inline clm_int try_send(const clm_id channel_id, const clm_id message_id)
{
    auto ch = detail::channel_registry::instance().find(channel_id);
    if (!ch)
        return FAILURE;
    std::lock_guard<std::mutex> lock(ch->mtx);
    return detail::try_send_locked(*ch, message_id);
}

inline clm_int try_receive(const clm_id channel_id, clm_id* out_id, bool* out_open)
{
    if (!out_id || !out_open)
        return FAILURE;
    auto ch = detail::channel_registry::instance().find(channel_id);
    if (!ch)
        return FAILURE;
    std::lock_guard<std::mutex> lock(ch->mtx);
    return detail::try_receive_locked(*ch, out_id, out_open);
}

inline clm_int send_timeout(const clm_id channel_id, const clm_id message_id, const std::uint64_t timeout_ms)
{
    auto ch = detail::channel_registry::instance().find(channel_id);
    if (!ch)
        return FAILURE;

    std::unique_lock<std::mutex> lock(ch->mtx);
    clm_int status = detail::try_send_locked(*ch, message_id);
    if (status != FULL || timeout_ms == 0)
        return status;

    detail::waiter w;
    w.message_id = message_id;
    ch->send_waiters.push_back(&w);

    const bool timed = (timeout_ms != WAIT_FOREVER);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (!w.matched && !w.closed) {
        if (timed) {
            if (ch->cv.wait_until(lock, deadline) == std::cv_status::timeout &&
                !w.matched && !w.closed) {
                ch->send_waiters.remove(&w);
                return TIMEOUT;
            }
        } else {
            ch->cv.wait(lock);
        }
    }

    if (w.matched)
        return SUCCESS;
    if (w.closed)
        return CLOSED;
    return FAILURE;
}

inline clm_int receive_timeout(const clm_id channel_id, clm_id* out_id, bool* out_open, const std::uint64_t timeout_ms)
{
    if (!out_id || !out_open)
        return FAILURE;
    auto ch = detail::channel_registry::instance().find(channel_id);
    if (!ch)
        return FAILURE;

    std::unique_lock<std::mutex> lock(ch->mtx);
    clm_int status = detail::try_receive_locked(*ch, out_id, out_open);
    if (status != EMPTY || timeout_ms == 0)
        return status;

    detail::waiter w;
    w.out_id = out_id;
    w.out_open = out_open;
    ch->receive_waiters.push_back(&w);

    const bool timed = (timeout_ms != WAIT_FOREVER);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (!w.matched && !w.closed) {
        if (timed) {
            if (ch->cv.wait_until(lock, deadline) == std::cv_status::timeout &&
                !w.matched && !w.closed) {
                ch->receive_waiters.remove(&w);
                return TIMEOUT;
            }
        } else {
            ch->cv.wait(lock);
        }
    }

    if (w.matched)
        return SUCCESS;
    if (w.closed)
        return CLOSED;
    return FAILURE;
}

inline clm_int send(const clm_id channel_id, const clm_id message_id)
{
    return send_timeout(channel_id, message_id, WAIT_FOREVER);
}

inline clm_int receive(const clm_id channel_id, clm_id* out_id, bool* out_open)
{
    return receive_timeout(channel_id, out_id, out_open, WAIT_FOREVER);
}

inline clm_int get_info(const clm_id channel_id, clm_channel_info* out_info)
{
    if (!out_info)
        return FAILURE;
    auto ch = detail::channel_registry::instance().find(channel_id);
    if (!ch)
        return FAILURE;
    std::lock_guard<std::mutex> lock(ch->mtx);
    out_info->capacity = ch->capacity;
    out_info->count = ch->buffer.size();
    out_info->closed = ch->closed;
    return SUCCESS;
}

} // namespace clm::channel

static inline clm_int clm_channel_create(const clm_id channel_id, const std::size_t capacity)
{
    return clm::channel::create(channel_id, capacity);
}

static inline clm_int clm_channel_close(const clm_id channel_id)
{
    return clm::channel::close(channel_id);
}

static inline clm_int clm_channel_destroy(const clm_id channel_id)
{
    return clm::channel::destroy(channel_id);
}

static inline clm_int clm_channel_try_send(const clm_id channel_id, const clm_id message_id)
{
    return clm::channel::try_send(channel_id, message_id);
}

static inline clm_int clm_channel_try_receive(const clm_id channel_id, clm_id* out_id, bool* out_open)
{
    return clm::channel::try_receive(channel_id, out_id, out_open);
}

static inline clm_int clm_channel_send(const clm_id channel_id, const clm_id message_id)
{
    return clm::channel::send(channel_id, message_id);
}

static inline clm_int clm_channel_receive(const clm_id channel_id, clm_id* out_id, bool* out_open)
{
    return clm::channel::receive(channel_id, out_id, out_open);
}

static inline clm_int clm_channel_send_timeout(const clm_id channel_id, const clm_id message_id, const std::uint64_t timeout_ms)
{
    return clm::channel::send_timeout(channel_id, message_id, timeout_ms);
}

static inline clm_int clm_channel_receive_timeout(const clm_id channel_id, clm_id* out_id, bool* out_open, const std::uint64_t timeout_ms)
{
    return clm::channel::receive_timeout(channel_id, out_id, out_open, timeout_ms);
}

static inline clm_int clm_channel_get_info(const clm_id channel_id, clm_channel_info* out_info)
{
    return clm::channel::get_info(channel_id, out_info);
}

