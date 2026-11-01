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

#include "managed.h"

#include <string.h>
#include <threads.h>
#include <time.h>

static once_flag clm_private_managed_once = ONCE_FLAG_INIT;
static mtx_t clm_private_managed_lock;
static cnd_t clm_private_managed_changed;
static bool clm_private_managed_initialized;
static bool clm_private_managed_enabled;
static bool clm_private_managed_stop_worker;
static bool clm_private_managed_worker_created;
static thrd_t clm_private_managed_worker;
static clm_managed_algorithm clm_private_managed_algorithm = CLM_MANAGED_MANUAL;
static clm_managed_thread_mode clm_private_managed_mode = CLM_MANAGED_CALLER;
static uint64_t clm_private_managed_interval_ms = 5000;
static size_t clm_private_managed_threshold = 10;
static clm_managed_hook clm_private_managed_hooks[3];
static clm_managed_stats clm_private_managed_stats;
static struct timespec clm_private_managed_last_collect;

static void clm_private_managed_init_sync(void)
{
    (void)mtx_init(&clm_private_managed_lock, mtx_plain);
    (void)cnd_init(&clm_private_managed_changed);
}

static struct timespec clm_private_managed_now(void)
{
    struct timespec now = {0, 0};
    (void)timespec_get(&now, TIME_UTC);
    return now;
}

static uint64_t clm_private_managed_elapsed_ms(
    struct timespec earlier, struct timespec later)
{
    if (later.tv_sec < earlier.tv_sec ||
        (later.tv_sec == earlier.tv_sec && later.tv_nsec < earlier.tv_nsec)) {
        return 0;
    }
    uint64_t seconds = (uint64_t)(later.tv_sec - earlier.tv_sec);
    long nanoseconds = later.tv_nsec - earlier.tv_nsec;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += 1000000000L;
    }
    const uint64_t maximum = ~(uint64_t)0;
    if (seconds > maximum / 1000) {
        return maximum;
    }
    return seconds * 1000 + (uint64_t)nanoseconds / 1000000;
}

static struct timespec clm_private_managed_add_ms(
    struct timespec start, uint64_t milliseconds)
{
    if (milliseconds > 86400000) {
        milliseconds = 86400000;
    }
    start.tv_sec += (time_t)(milliseconds / 1000);
    start.tv_nsec += (long)(milliseconds % 1000) * 1000000L;
    if (start.tv_nsec >= 1000000000L) {
        ++start.tv_sec;
        start.tv_nsec -= 1000000000L;
    }
    return start;
}

static clm_managed_status clm_private_managed_ready_status(void)
{
    if (!clm_private_managed_initialized) {
        return CLM_MANAGED_NOT_INITIALIZED;
    }
    if (clm_private_managed_enabled) {
        return CLM_MANAGED_ACTIVE;
    }
    return CLM_MANAGED_SUCCESS;
}

/** Initializes the coordinator; safe to call more than once.
 * @return CLM_MANAGED_SUCCESS.
 */
clm_managed_status clm_managed_initialize(void){
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    clm_private_managed_initialized = true;
    mtx_unlock(&clm_private_managed_lock);
    return CLM_MANAGED_SUCCESS;
}

/** Installs the three subsystem hooks while the coordinator is disabled.
 * @param depend Hook for transient dependencies.
 * @param container Hook for containers.
 * @param storage Hook for storages.
 * @return CLM_MANAGED_NOT_INITIALIZED, CLM_MANAGED_ACTIVE, or
 * CLM_MANAGED_SUCCESS.
 * Hooks with NULL callbacks are skipped.
 */
clm_managed_status clm_managed_configure_hooks(
    clm_managed_hook depend, clm_managed_hook container,
    clm_managed_hook storage){
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    clm_managed_status status = clm_private_managed_ready_status();
    if (status == CLM_MANAGED_SUCCESS) {
        clm_private_managed_hooks[0] = depend;
        clm_private_managed_hooks[1] = container;
        clm_private_managed_hooks[2] = storage;
    }
    mtx_unlock(&clm_private_managed_lock);
    return status;
}

/** Selects the scheduling policy while the coordinator is disabled.
 * @param algorithm Policy to use.
 * @return CLM_MANAGED_INVALID for an unknown policy; otherwise
 * CLM_MANAGED_NOT_INITIALIZED, CLM_MANAGED_ACTIVE, or CLM_MANAGED_SUCCESS.
 */
clm_managed_status clm_managed_set_algorithm(
    clm_managed_algorithm algorithm){
    if (algorithm < CLM_MANAGED_MANUAL || algorithm > CLM_MANAGED_HYBRID) {
        return CLM_MANAGED_INVALID;
    }
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    clm_managed_status status = clm_private_managed_ready_status();
    if (status == CLM_MANAGED_SUCCESS) {
        clm_private_managed_algorithm = algorithm;
    }
    mtx_unlock(&clm_private_managed_lock);
    return status;
}

/** Sets the interval schedule period in milliseconds.
 * @param milliseconds Period; zero is invalid.
 * @return CLM_MANAGED_INVALID for zero; otherwise CLM_MANAGED_NOT_INITIALIZED,
 * CLM_MANAGED_ACTIVE, or CLM_MANAGED_SUCCESS.
 */
clm_managed_status clm_managed_set_interval(uint64_t milliseconds){
    if (milliseconds == 0) {
        return CLM_MANAGED_INVALID;
    }
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    clm_managed_status status = clm_private_managed_ready_status();
    if (status == CLM_MANAGED_SUCCESS) {
        clm_private_managed_interval_ms = milliseconds;
    }
    mtx_unlock(&clm_private_managed_lock);
    return status;
}

/** Sets the aggregate pending-item threshold.
 * @param count Required pending-item count; zero is invalid.
 * @return CLM_MANAGED_INVALID for zero; otherwise CLM_MANAGED_NOT_INITIALIZED,
 * CLM_MANAGED_ACTIVE, or CLM_MANAGED_SUCCESS.
 */
clm_managed_status clm_managed_set_threshold(size_t count){
    if (count == 0) {
        return CLM_MANAGED_INVALID;
    }
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    clm_managed_status status = clm_private_managed_ready_status();
    if (status == CLM_MANAGED_SUCCESS) {
        clm_private_managed_threshold = count;
    }
    mtx_unlock(&clm_private_managed_lock);
    return status;
}

/** Selects where scheduled cleanup runs while the coordinator is disabled.
 * @param mode Caller polling or the coordinator-owned worker thread.
 * @return CLM_MANAGED_INVALID for an unknown mode; otherwise
 * CLM_MANAGED_NOT_INITIALIZED, CLM_MANAGED_ACTIVE, or CLM_MANAGED_SUCCESS.
 */
clm_managed_status clm_managed_set_thread(clm_managed_thread_mode mode){
    if (mode != CLM_MANAGED_CALLER && mode != CLM_MANAGED_WORKER) {
        return CLM_MANAGED_INVALID;
    }
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    clm_managed_status status = clm_private_managed_ready_status();
    if (status == CLM_MANAGED_SUCCESS) {
        clm_private_managed_mode = mode;
    }
    mtx_unlock(&clm_private_managed_lock);
    return status;
}

/** Runs each configured sweep once in dependency, container, storage order.
 * @param[out] depend Optional transient-disposal count, written on success.
 * @param[out] container Optional container-disposal count, written on success.
 * @param[out] storage Optional storage-disposal count, written on success.
 * @return CLM_MANAGED_NOT_INITIALIZED, CLM_MANAGED_FAILURE if a hook fails, or
 * CLM_MANAGED_SUCCESS. A failure stops the cycle at the first failing hook.
 */
clm_managed_status clm_managed_collect(size_t *depend, size_t *container, size_t *storage){
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    if (!clm_private_managed_initialized) {
        mtx_unlock(&clm_private_managed_lock);
        return CLM_MANAGED_NOT_INITIALIZED;
    }

    size_t disposed[3] = {0, 0, 0};
    for (size_t index = 0; index < 3; ++index) {
        clm_managed_hook *hook = &clm_private_managed_hooks[index];
        if (hook->sweep == NULL) {
            continue;
        }
        int error = hook->sweep(hook->user, &disposed[index]);
        if (error != 0) {
            ++clm_private_managed_stats.failures;
            clm_private_managed_stats.last_error = error;
            mtx_unlock(&clm_private_managed_lock);
            return CLM_MANAGED_FAILURE;
        }
    }

    ++clm_private_managed_stats.cycles;
    clm_private_managed_stats.disposed_transients += disposed[0];
    clm_private_managed_stats.disposed_containers += disposed[1];
    clm_private_managed_stats.disposed_storages += disposed[2];
    clm_private_managed_stats.last_error = 0;
    clm_private_managed_last_collect = clm_private_managed_now();
    if (depend != NULL) {
        *depend = disposed[0];
    }
    if (container != NULL) {
        *container = disposed[1];
    }
    if (storage != NULL) {
        *storage = disposed[2];
    }
    mtx_unlock(&clm_private_managed_lock);
    return CLM_MANAGED_SUCCESS;
}

static bool clm_private_managed_interval_due(void)
{
    if (clm_private_managed_algorithm != CLM_MANAGED_INTERVAL &&
        clm_private_managed_algorithm != CLM_MANAGED_HYBRID) {
        return false;
    }
    struct timespec now = clm_private_managed_now();
    return clm_private_managed_elapsed_ms(
        clm_private_managed_last_collect, now) >=
        clm_private_managed_interval_ms;
}

static bool clm_private_managed_threshold_due(void)
{
    if (clm_private_managed_algorithm != CLM_MANAGED_THRESHOLD &&
        clm_private_managed_algorithm != CLM_MANAGED_HYBRID) {
        return false;
    }
    size_t pending = 0;
    for (size_t index = 0; index < 3; ++index) {
        clm_managed_hook *hook = &clm_private_managed_hooks[index];
        if (hook->pending == NULL) {
            continue;
        }
        size_t next = hook->pending(hook->user);
        if (next >= clm_private_managed_threshold - pending) {
            return true;
        }
        pending += next;
    }
    return pending >= clm_private_managed_threshold;
}

static bool clm_private_managed_due(void)
{
    return clm_private_managed_interval_due() ||
        clm_private_managed_threshold_due();
}

static clm_managed_status clm_private_managed_poll_internal(
    bool is_worker, bool *collected)
{
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    if (collected != NULL) {
        *collected = false;
    }
    if (!clm_private_managed_initialized) {
        mtx_unlock(&clm_private_managed_lock);
        return CLM_MANAGED_NOT_INITIALIZED;
    }
    bool correct_thread =
        (clm_private_managed_mode == CLM_MANAGED_WORKER) == is_worker;
    bool should_collect = clm_private_managed_enabled && correct_thread &&
        clm_private_managed_algorithm != CLM_MANAGED_MANUAL &&
        clm_private_managed_due();
    mtx_unlock(&clm_private_managed_lock);

    if (!should_collect) {
        return CLM_MANAGED_SUCCESS;
    }
    clm_managed_status status = clm_managed_collect(NULL, NULL, NULL);
    if (status == CLM_MANAGED_SUCCESS && collected != NULL) {
        *collected = true;
    }
    return status;
}

/** Checks a caller-thread schedule and optionally runs cleanup.
 * @param[out] collected Optional flag set to true only when a sweep ran; set
 * false otherwise.
 * @return CLM_MANAGED_NOT_INITIALIZED, a collection status, or
 * CLM_MANAGED_SUCCESS when no cleanup is due.
 */
clm_managed_status clm_managed_poll(bool *collected){
    return clm_private_managed_poll_internal(false, collected);
}

static inline int clm_private_managed_worker_main(void* unused) // NOLINT(readability-non-const-parameter): required by thrd_start_t
{
    (void)unused;
    for (;;) {
        mtx_lock(&clm_private_managed_lock);
        if (clm_private_managed_stop_worker) {
            mtx_unlock(&clm_private_managed_lock);
            break;
        }
        uint64_t wait_ms = clm_private_managed_interval_ms;
        if (wait_ms > 100) {
            wait_ms = 100;
        }
        struct timespec deadline = clm_private_managed_add_ms(
            clm_private_managed_now(), wait_ms);
        (void)cnd_timedwait(&clm_private_managed_changed,
            &clm_private_managed_lock, &deadline);
        bool should_stop = clm_private_managed_stop_worker;
        mtx_unlock(&clm_private_managed_lock);
        if (should_stop) {
            break;
        }
        (void)clm_private_managed_poll_internal(true, NULL);
    }
    return 0;
}

/** Enables scheduled collection and starts a worker when configured.
 * @return CLM_MANAGED_NOT_INITIALIZED, CLM_MANAGED_FAILURE if worker creation
 * fails, or CLM_MANAGED_SUCCESS.
 */
clm_managed_status clm_managed_enable(void){
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    if (!clm_private_managed_initialized) {
        mtx_unlock(&clm_private_managed_lock);
        return CLM_MANAGED_NOT_INITIALIZED;
    }
    if (clm_private_managed_enabled) {
        mtx_unlock(&clm_private_managed_lock);
        return CLM_MANAGED_SUCCESS;
    }

    clm_private_managed_enabled = true;
    clm_private_managed_stop_worker = false;
    clm_private_managed_last_collect = clm_private_managed_now();
    const bool needs_worker = clm_private_managed_mode == CLM_MANAGED_WORKER &&
        clm_private_managed_algorithm != CLM_MANAGED_MANUAL;
    if (needs_worker && thrd_create(&clm_private_managed_worker, clm_private_managed_worker_main, NULL) != thrd_success) {
        clm_private_managed_enabled = false;
        mtx_unlock(&clm_private_managed_lock);
        return CLM_MANAGED_FAILURE;
    }
    if (needs_worker) {
        clm_private_managed_worker_created = true;
    }
    mtx_unlock(&clm_private_managed_lock);
    return CLM_MANAGED_SUCCESS;
}

/** Disables scheduled collection and joins any worker thread.
 * @return CLM_MANAGED_NOT_INITIALIZED, CLM_MANAGED_FAILURE if joining fails,
 * or CLM_MANAGED_SUCCESS.
 */
clm_managed_status clm_managed_disable(void){
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    if (!clm_private_managed_initialized) {
        mtx_unlock(&clm_private_managed_lock);
        return CLM_MANAGED_NOT_INITIALIZED;
    }
    clm_private_managed_enabled = false;
    clm_private_managed_stop_worker = true;
    cnd_broadcast(&clm_private_managed_changed);
    const bool should_join = clm_private_managed_worker_created;
    const thrd_t worker = clm_private_managed_worker;
    clm_private_managed_worker_created = false;
    mtx_unlock(&clm_private_managed_lock);

    if (should_join && thrd_join(worker, NULL) != thrd_success) {
        return CLM_MANAGED_FAILURE;
    }
    return CLM_MANAGED_SUCCESS;
}

/** Returns a thread-safe snapshot of the coordinator statistics. */
clm_managed_stats clm_managed_get_stats(void){
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    clm_managed_stats stats = clm_private_managed_stats;
    mtx_unlock(&clm_private_managed_lock);
    return stats;
}

/** Returns whether scheduled collection is currently enabled.
 * @return true when enabled; otherwise false.
 */
bool clm_managed_is_enabled(void){
    call_once(&clm_private_managed_once, clm_private_managed_init_sync);
    mtx_lock(&clm_private_managed_lock);
    bool enabled = clm_private_managed_enabled;
    mtx_unlock(&clm_private_managed_lock);
    return enabled;
}

/** Stops collection and resets initialization, configuration, hooks, and
 * statistics. Safe to call when already stopped or uninitialized.
 */
void clm_managed_destroy(void){
    (void)clm_managed_disable();
    mtx_lock(&clm_private_managed_lock);
    clm_private_managed_initialized = false;
    clm_private_managed_enabled = false;
    clm_private_managed_algorithm = CLM_MANAGED_MANUAL;
    clm_private_managed_mode = CLM_MANAGED_CALLER;
    clm_private_managed_interval_ms = 5000;
    clm_private_managed_threshold = 10;
    memset(clm_private_managed_hooks, 0, sizeof clm_private_managed_hooks);
    memset(&clm_private_managed_stats, 0, sizeof clm_private_managed_stats);
    mtx_unlock(&clm_private_managed_lock);
}

