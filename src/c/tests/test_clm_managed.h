#pragma once

#include "test.h"
#include "../clm/managed.h"

#include <string.h>

typedef struct clm_managed_test_hook_state {
    size_t pending;
    size_t disposed;
    int failure;
} clm_managed_test_hook_state;

static int clm_managed_test_sweep(void *user, size_t *out_disposed)
{
    clm_managed_test_hook_state *state =
        (clm_managed_test_hook_state *)user;
    if (state->failure != 0) {
        return state->failure;
    }
    state->disposed = state->pending;
    state->pending = 0;
    *out_disposed = state->disposed;
    return 0;
}

static size_t clm_managed_test_pending(void *user)
{
    clm_managed_test_hook_state *state =
        (clm_managed_test_hook_state *)user;
    return state->pending;
}

static int clm_managed_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Managed Cleanup");
    clm_managed_destroy();
    clm_managed_test_hook_state depend = {2, 0, 0};
    clm_managed_test_hook_state container = {1, 0, 0};
    clm_managed_test_hook_state storage = {3, 0, 0};
    clm_managed_hook depend_hook = {
        clm_managed_test_sweep, clm_managed_test_pending, &depend};
    clm_managed_hook container_hook = {
        clm_managed_test_sweep, clm_managed_test_pending, &container};
    clm_managed_hook storage_hook = {
        clm_managed_test_sweep, clm_managed_test_pending, &storage};
    size_t disposed_depend = 0;
    size_t disposed_container = 0;
    size_t disposed_storage = 0;
    bool collected = true;

    CLM_ASSERT(clm_managed_initialize() == CLM_MANAGED_SUCCESS &&
        clm_managed_configure_hooks(depend_hook, container_hook,
            storage_hook) == CLM_MANAGED_SUCCESS &&
        clm_managed_set_algorithm(CLM_MANAGED_MANUAL) == CLM_MANAGED_SUCCESS &&
        clm_managed_collect(&disposed_depend, &disposed_container,
            &disposed_storage) == CLM_MANAGED_SUCCESS &&
        disposed_depend == 2 && disposed_container == 1 &&
        disposed_storage == 3,
        "configure and manually run each registered cleanup hook");

    clm_managed_stats stats = clm_managed_get_stats();
    CLM_ASSERT(stats.cycles == 1 && stats.disposed_transients == 2 &&
        stats.disposed_containers == 1 && stats.disposed_storages == 3 &&
        stats.failures == 0 && stats.last_error == 0,
        "record successful cleanup counts");

    depend.pending = 2;
    container.pending = 1;
    storage.pending = 0;
    CLM_ASSERT(clm_managed_set_threshold(3) == CLM_MANAGED_SUCCESS &&
        clm_managed_set_algorithm(CLM_MANAGED_THRESHOLD) ==
            CLM_MANAGED_SUCCESS &&
        clm_managed_enable() == CLM_MANAGED_SUCCESS &&
        clm_managed_poll(&collected) == CLM_MANAGED_SUCCESS && collected &&
        clm_managed_disable() == CLM_MANAGED_SUCCESS &&
        !clm_managed_is_enabled(),
        "poll and collect when aggregate pending items meet the threshold");

    depend.failure = -17;
    CLM_ASSERT(clm_managed_collect(NULL, NULL, NULL) == CLM_MANAGED_FAILURE,
        "report failures returned by cleanup hooks");
    stats = clm_managed_get_stats();
    CLM_ASSERT(stats.cycles == 2 && stats.failures == 1 &&
        stats.last_error == -17,
        "preserve successful cycle totals and record the last hook error");

    depend.failure = 0;
    clm_managed_destroy();
    CLM_ASSERT(!clm_managed_is_enabled() &&
        clm_managed_disable() == CLM_MANAGED_NOT_INITIALIZED,
        "stop and reset the cleanup coordinator");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
