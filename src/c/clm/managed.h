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

#include <stdbool.h>
#include <stdint.h>

/** Policy used to decide when scheduled cleanup is due. */
typedef enum clm_managed_algorithm {
    /** Run cleanup only when explicitly requested with clm_managed_collect. */
    CLM_MANAGED_MANUAL = 1,
    /** Run cleanup after the configured interval elapses. */
    CLM_MANAGED_INTERVAL = 2,
    /** Run cleanup when the aggregate pending count reaches its threshold. */
    CLM_MANAGED_THRESHOLD = 3,
    /** Run cleanup when either the interval or threshold condition is met. */
    CLM_MANAGED_HYBRID = 4
} clm_managed_algorithm;

/** Thread on which scheduled cleanup checks and callbacks run. */
typedef enum clm_managed_thread_mode {
    /** Run scheduled checks when the application calls clm_managed_poll. */
    CLM_MANAGED_CALLER = 1,
    /** Run scheduled checks from a coordinator-owned worker thread. */
    CLM_MANAGED_WORKER = 2
} clm_managed_thread_mode;

/** Results returned by coordinator operations. */
typedef enum clm_managed_status {
    /** The requested operation completed successfully. */
    CLM_MANAGED_SUCCESS = 0,
    /** The coordinator has not been initialized. */
    CLM_MANAGED_NOT_INITIALIZED = -1,
    /** An argument or configuration value is invalid. */
    CLM_MANAGED_INVALID = -2,
    /** The operation requires the coordinator to be disabled. */
    CLM_MANAGED_ACTIVE = -3,
    /** A cleanup hook or worker-thread operation failed. */
    CLM_MANAGED_FAILURE = -4
} clm_managed_status;

/** Cumulative results from successful and failed cleanup cycles. */
typedef struct clm_managed_stats {
    /** Number of cleanup cycles completed successfully. */
    uint64_t cycles;
    /** Total transient items disposed by successful cycles. */
    uint64_t disposed_transients;
    /** Total containers disposed by successful cycles. */
    uint64_t disposed_containers;
    /** Total storages disposed by successful cycles. */
    uint64_t disposed_storages;
    /** Number of cleanup cycles that stopped because a hook failed. */
    uint64_t failures;
    /** Error returned by the most recent failed hook, or zero on success. */
    int last_error;
} clm_managed_stats;

/** Runs one subsystem sweep.
 * @param user Hook-specific context supplied in clm_managed_hook.
 * @param[out] out_disposed Receives the number of items disposed on success.
 * @return Zero on success; a nonzero value marks the cleanup cycle as failed.
 * The callback runs under the coordinator lock and must not re-enter it.
 */
typedef int (*clm_managed_sweep_fn)(void *user, size_t *out_disposed);

/** Reports how many items are waiting for cleanup.
 * @param user Hook-specific context supplied in clm_managed_hook.
 * @return Current pending-item count. The callback runs under the coordinator
 * lock and must not re-enter it.
 */
typedef size_t (*clm_managed_pending_fn)(void *user);

/** Optional callbacks and context for one cleanup subsystem. */
typedef struct clm_managed_hook {
    /** Cleanup callback; NULL skips this subsystem during collection. */
    clm_managed_sweep_fn sweep;
    /** Pending-count callback used by threshold scheduling; may be NULL. */
    clm_managed_pending_fn pending;
    /** Context passed to both callbacks. */
    void *user;
} clm_managed_hook;

/* Define CLM_MANAGED_IMPLEMENTATION in one translation unit, or define
 * CLM_IMPLEMENTATION once for all single-header modules.
 */
clm_managed_status clm_managed_initialize(void);
clm_managed_status clm_managed_configure_hooks(
    clm_managed_hook depend, clm_managed_hook container,
    clm_managed_hook storage);
clm_managed_status clm_managed_set_algorithm(
    clm_managed_algorithm algorithm);
clm_managed_status clm_managed_set_interval(uint64_t milliseconds);
clm_managed_status clm_managed_set_threshold(size_t count);
clm_managed_status clm_managed_set_thread(clm_managed_thread_mode mode);
clm_managed_status clm_managed_collect(size_t *depend, size_t *container, size_t *storage);
clm_managed_status clm_managed_poll(bool *collected);
clm_managed_status clm_managed_enable(void);
clm_managed_status clm_managed_disable(void);
clm_managed_stats clm_managed_get_stats(void);
bool clm_managed_is_enabled(void);
void clm_managed_destroy(void);