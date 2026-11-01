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

#include "channel.h"

#include <limits.h>
#include <stdlib.h>

clm_channel_internal* clm_channel_internal_channels = NULL;
atomic_flag clm_channel_internal_registry_lock = ATOMIC_FLAG_INIT;

void clm_channel_internal_lock_registry(void) {
    while (atomic_flag_test_and_set_explicit(&clm_channel_internal_registry_lock, memory_order_acquire)) { }
}

void clm_channel_internal_unlock_registry(void) {
    atomic_flag_clear_explicit(&clm_channel_internal_registry_lock, memory_order_release);
}

clm_channel_internal* clm_channel_internal_find(const clm_id channel_id) {
    clm_channel_internal* found = NULL;
    clm_channel_internal_lock_registry();
    for (clm_channel_internal* channel = clm_channel_internal_channels;
        channel != NULL; channel = channel->next)
        if (channel->id == channel_id) {
            found = channel;
            break;
        }
    clm_channel_internal_unlock_registry();
    return found;
}

void clm_channel_internal_push_waiter(
    clm_channel_internal_waiter** head,
    clm_channel_internal_waiter** tail,
    clm_channel_internal_waiter* waiter
) {
    waiter->next = NULL;
    if (*tail == NULL) {
        *head = waiter;
    }
    else {
        (*tail)->next = waiter;
    }
    *tail = waiter;
}

clm_channel_internal_waiter* clm_channel_internal_pop_waiter(
    clm_channel_internal_waiter** head,
    clm_channel_internal_waiter** tail
) {
    clm_channel_internal_waiter* waiter = *head;
    if (waiter != NULL) {
        *head = waiter->next;
        if (*head == NULL) {
            *tail = NULL;
        }
        waiter->next = NULL;
    }
    return waiter;
}

void clm_channel_internal_remove_waiter(
    clm_channel_internal_waiter** head,
    clm_channel_internal_waiter** tail,
    const clm_channel_internal_waiter* target
) {
    clm_channel_internal_waiter* previous = NULL;
    for (clm_channel_internal_waiter* waiter = *head; waiter != NULL;
        waiter = waiter->next) {
        if (waiter == target) {
            if (previous == NULL) {
                *head = waiter->next;
            }
            else {
                previous->next = waiter->next;
            }
            if (*tail == waiter) {
                *tail = previous;
            }
            waiter->next = NULL;
            return;
        }
        previous = waiter;
    }
}

void clm_channel_internal_buffer_push(clm_channel_internal* channel, const clm_id message_id) {
    channel->buffer[channel->tail] = message_id;
    channel->tail = (channel->tail + 1) % channel->capacity;
    ++channel->count;
}

clm_id clm_channel_internal_buffer_pop(clm_channel_internal* channel) {
    const clm_id message_id = channel->buffer[channel->head];
    channel->head = (channel->head + 1) % channel->capacity;
    --channel->count;
    return message_id;
}

void clm_channel_internal_drain_senders(clm_channel_internal* channel) {
    while (!channel->closed && channel->capacity != 0 &&
        channel->count < channel->capacity && channel->send_head != NULL) {
        clm_channel_internal_waiter* sender = clm_channel_internal_pop_waiter(&channel->send_head, &channel->send_tail);
        clm_channel_internal_buffer_push(channel, sender->message_id);
        sender->matched = true;
    }
    cnd_broadcast(&channel->changed);
}

void clm_channel_internal_drain_receivers(clm_channel_internal* channel) {
    while (channel->receive_head != NULL && channel->count != 0) {
        clm_channel_internal_waiter* receiver = clm_channel_internal_pop_waiter(&channel->receive_head, &channel->receive_tail);
        *receiver->out_id = clm_channel_internal_buffer_pop(channel);
        *receiver->out_open = true;
        receiver->matched = true;
    }
    if (channel->closed && channel->count == 0) {
        clm_channel_internal_waiter* receiver;
        while ((receiver = clm_channel_internal_pop_waiter(&channel->receive_head, &channel->receive_tail)) != NULL) {
            *receiver->out_id = 0;
            *receiver->out_open = false;
            receiver->closed = true;
        }
    }
    cnd_broadcast(&channel->changed);
}

clm_int clm_channel_internal_try_send_locked(
    clm_channel_internal* channel,
    const clm_id message_id) {
    if (channel->closed) {
        return CLM_CHANNEL_OPERATION_CLOSED;
    }
    clm_channel_internal_waiter* receiver = clm_channel_internal_pop_waiter(&channel->receive_head, &channel->receive_tail);
    if (receiver != NULL) {
        *receiver->out_id = message_id;
        *receiver->out_open = true;
        receiver->matched = true;
        cnd_broadcast(&channel->changed);
        return CLM_CHANNEL_OPERATION_SUCCESS;
    }
    if (channel->capacity == 0 || channel->send_head != NULL || channel->count == channel->capacity) {
        return CLM_CHANNEL_OPERATION_FULL;
    }
    clm_channel_internal_buffer_push(channel, message_id);
    clm_channel_internal_drain_receivers(channel);
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

clm_int clm_channel_internal_try_receive_locked(clm_channel_internal* channel, clm_id* out_id, bool* out_open) {
    if (channel->count != 0) {
        *out_id = clm_channel_internal_buffer_pop(channel);
        *out_open = true;
        clm_channel_internal_drain_senders(channel);
        clm_channel_internal_drain_receivers(channel);
        return CLM_CHANNEL_OPERATION_SUCCESS;
    }
    clm_channel_internal_waiter* sender = clm_channel_internal_pop_waiter(&channel->send_head, &channel->send_tail);
    if (sender != NULL) {
        *out_id = sender->message_id;
        *out_open = true;
        sender->matched = true;
        cnd_broadcast(&channel->changed);
        return CLM_CHANNEL_OPERATION_SUCCESS;
    }
    if (channel->closed) {
        *out_id = 0;
        *out_open = false;
        return CLM_CHANNEL_OPERATION_CLOSED;
    }
    return CLM_CHANNEL_OPERATION_EMPTY;
}

clm_int clm_channel_internal_deadline(const uint64_t timeout_ms, struct timespec* deadline) {
    if (deadline == NULL || timespec_get(deadline, TIME_UTC) != TIME_UTC) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    uint64_t additional_seconds = timeout_ms / 1000;
    const long additional_nanoseconds = (long)(timeout_ms % 1000) * 1000000L;
    long nanoseconds = deadline->tv_nsec + additional_nanoseconds;
    if (nanoseconds >= 1000000000L) {
        ++additional_seconds;
        nanoseconds -= 1000000000L;
    }
    const uint64_t max_time = sizeof(time_t) >= sizeof(int64_t)
        ? (uint64_t)INT64_MAX
        : (UINT64_C(1) << (sizeof(time_t) * CHAR_BIT - 1)) - 1;
    const uint64_t current_seconds = (uint64_t)deadline->tv_sec;
    if (additional_seconds > max_time || current_seconds > max_time - additional_seconds) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    deadline->tv_sec = (time_t)(current_seconds + additional_seconds);
    deadline->tv_nsec = nanoseconds;
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

clm_int clm_channel_create(const clm_id channel_id, const size_t capacity) {
    if (capacity > (size_t)-1 / sizeof(clm_id)) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    clm_channel_internal* channel = calloc(1, sizeof(*channel));
    if (channel == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    if (capacity != 0) {
        channel->buffer = (clm_id*)calloc(capacity, sizeof(clm_id));
        if (channel->buffer == NULL) {
            free(channel);
            return CLM_CHANNEL_OPERATION_FAILURE;
        }
    }
    if (mtx_init(&channel->mutex, mtx_plain) != thrd_success) {
        free(channel->buffer);
        free(channel);
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    if (cnd_init(&channel->changed) != thrd_success) {
        mtx_destroy(&channel->mutex);
        free(channel->buffer);
        free(channel);
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    channel->id = channel_id;
    channel->capacity = capacity;
    clm_channel_internal_lock_registry();
    for (clm_channel_internal* current = clm_channel_internal_channels;
        current != NULL; current = current->next)
        if (current->id == channel_id) {
            clm_channel_internal_unlock_registry();
            cnd_destroy(&channel->changed);
            mtx_destroy(&channel->mutex);
            free(channel->buffer);
            free(channel);
            return CLM_CHANNEL_OPERATION_FAILURE;
        }
    channel->next = clm_channel_internal_channels;
    clm_channel_internal_channels = channel;
    clm_channel_internal_unlock_registry();
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

clm_int clm_channel_close(const clm_id channel_id) {
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    mtx_lock(&channel->mutex);
    if (channel->closed) {
        mtx_unlock(&channel->mutex);
        return CLM_CHANNEL_OPERATION_CLOSED;
    }
    channel->closed = true;
    clm_channel_internal_waiter* sender;
    while ((sender = clm_channel_internal_pop_waiter(&channel->send_head, &channel->send_tail)) != NULL) {
        sender->closed = true;
    }
    clm_channel_internal_drain_receivers(channel);
    cnd_broadcast(&channel->changed);
    mtx_unlock(&channel->mutex);
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

clm_int clm_channel_destroy(const clm_id channel_id) {
    clm_channel_internal_lock_registry();
    clm_channel_internal** link = &clm_channel_internal_channels;
    while (*link != NULL && (*link)->id != channel_id) {
        link = &(*link)->next;
    }
    clm_channel_internal* channel = *link;
    if (channel == NULL) {
        clm_channel_internal_unlock_registry();
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    mtx_lock(&channel->mutex);
    if (channel->send_head != NULL || channel->receive_head != NULL) {
        mtx_unlock(&channel->mutex);
        clm_channel_internal_unlock_registry();
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    *link = channel->next;
    mtx_unlock(&channel->mutex);
    clm_channel_internal_unlock_registry();
    cnd_destroy(&channel->changed);
    mtx_destroy(&channel->mutex);
    free(channel->buffer);
    free(channel);
    return CLM_CHANNEL_OPERATION_SUCCESS;
}

clm_int clm_channel_try_send(const clm_id channel_id, const clm_id message_id) {
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    mtx_lock(&channel->mutex);
    const clm_int status = clm_channel_internal_try_send_locked(channel, message_id);
    mtx_unlock(&channel->mutex);
    return status;
}

clm_int clm_channel_try_receive(const clm_id channel_id, clm_id* out_id, bool* out_open) {
    if (out_id == NULL || out_open == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    mtx_lock(&channel->mutex);
    const clm_int status = clm_channel_internal_try_receive_locked(channel, out_id, out_open);
    mtx_unlock(&channel->mutex);
    return status;
}

clm_int clm_channel_send_timeout(
    const clm_id channel_id,
    const clm_id message_id,
    const uint64_t timeout_ms
) {
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    struct timespec deadline;
    const bool timed = timeout_ms != CLM_CHANNEL_WAIT_FOREVER && timeout_ms != 0;
    if (timed && clm_channel_internal_deadline(timeout_ms, &deadline) != CLM_CHANNEL_OPERATION_SUCCESS) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    mtx_lock(&channel->mutex);
    clm_int status = clm_channel_internal_try_send_locked(channel, message_id);
    if (status != CLM_CHANNEL_OPERATION_FULL || timeout_ms == 0) {
        mtx_unlock(&channel->mutex);
        return status;
    }
    clm_channel_internal_waiter waiter = {0};
    waiter.message_id = message_id;
    clm_channel_internal_push_waiter(&channel->send_head,
        &channel->send_tail, &waiter);
    for (;;) {
        if (waiter.matched) {
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_SUCCESS;
        }
        if (waiter.closed) {
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_CLOSED;
        }
        const int wait_status = timed
            ? cnd_timedwait(&channel->changed, &channel->mutex, &deadline)
            : cnd_wait(&channel->changed, &channel->mutex);
        if (wait_status == thrd_timedout && !waiter.matched && !waiter.closed) {
            clm_channel_internal_remove_waiter(&channel->send_head, &channel->send_tail, &waiter);
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_TIMEOUT;
        }
        if (wait_status != thrd_success && wait_status != thrd_timedout) {
            if (!waiter.matched && !waiter.closed) {
                clm_channel_internal_remove_waiter(&channel->send_head, &channel->send_tail, &waiter);
            }
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_FAILURE;
        }
    }
}

clm_int clm_channel_receive_timeout(
    const clm_id channel_id,
    clm_id* out_id,
    bool* out_open,
    const uint64_t timeout_ms
) {
    if (out_id == NULL || out_open == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    struct timespec deadline;
    const bool timed = timeout_ms != CLM_CHANNEL_WAIT_FOREVER && timeout_ms != 0;
    if (timed && clm_channel_internal_deadline(timeout_ms, &deadline) != CLM_CHANNEL_OPERATION_SUCCESS) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    mtx_lock(&channel->mutex);
    const clm_int status = clm_channel_internal_try_receive_locked(channel, out_id, out_open);
    if (status != CLM_CHANNEL_OPERATION_EMPTY || timeout_ms == 0) {
        mtx_unlock(&channel->mutex);
        return status;
    }
    clm_channel_internal_waiter waiter = {0};
    waiter.out_id = out_id;
    waiter.out_open = out_open;
    clm_channel_internal_push_waiter(&channel->receive_head, &channel->receive_tail, &waiter);
    for (;;) {
        if (waiter.matched) {
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_SUCCESS;
        }
        if (waiter.closed) {
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_CLOSED;
        }
        const int wait_status = timed
            ? cnd_timedwait(&channel->changed, &channel->mutex, &deadline)
            : cnd_wait(&channel->changed, &channel->mutex);
        if (wait_status == thrd_timedout && !waiter.matched && !waiter.closed) {
            clm_channel_internal_remove_waiter(&channel->receive_head, &channel->receive_tail, &waiter);
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_TIMEOUT;
        }
        if (wait_status != thrd_success && wait_status != thrd_timedout) {
            if (!waiter.matched && !waiter.closed) {
                clm_channel_internal_remove_waiter(&channel->receive_head, &channel->receive_tail, &waiter);
            }
            mtx_unlock(&channel->mutex);
            return CLM_CHANNEL_OPERATION_FAILURE;
        }
    }
}

clm_int clm_channel_send(const clm_id channel_id, const clm_id message_id) {
    return clm_channel_send_timeout(channel_id, message_id, CLM_CHANNEL_WAIT_FOREVER);
}

clm_int clm_channel_receive(const clm_id channel_id, clm_id* out_id, bool* out_open) {
    return clm_channel_receive_timeout(channel_id, out_id, out_open, CLM_CHANNEL_WAIT_FOREVER);
}

clm_int clm_channel_get_info(const clm_id channel_id, clm_channel_info* out_info) {
    if (out_info == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    clm_channel_internal* channel = clm_channel_internal_find(channel_id);
    if (channel == NULL) {
        return CLM_CHANNEL_OPERATION_FAILURE;
    }
    mtx_lock(&channel->mutex);
    const clm_channel_info info = {
        .capacity = channel->capacity, .count = channel->count, .closed = channel->closed
    };
    mtx_unlock(&channel->mutex);
    *out_info = info;
    return CLM_CHANNEL_OPERATION_SUCCESS;
}