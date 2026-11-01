#pragma once

#include "test.hpp"
#include "../clm/dispatch.hpp"

#include <string.h>

#define CLM_DISPATCH_TEST_MESSAGE_ID 96201
#define CLM_DISPATCH_TEST_SERVICE_ID 96202

typedef struct clm_dispatch_test_resolver_context {
    clm_id service_id;
    int *service_value;
} clm_dispatch_test_resolver_context;

static clm_dispatch_envelope clm_dispatch_test_last_envelope;
static size_t clm_dispatch_test_log_count;

static clm_dispatch_status clm_dispatch_test_resolve(
    clm_id service_id, void **out_service, void *user)
{
    clm_dispatch_test_resolver_context *context =
        (clm_dispatch_test_resolver_context *)user;
    if (out_service == NULL || service_id != context->service_id) {
        return CLM_DISPATCH_RESOLVE_FAILED;
    }
    *out_service = context->service_value;
    return CLM_DISPATCH_SUCCESS;
}

static clm_dispatch_status clm_dispatch_test_handler(
    const void *message, void *result, void *const dependencies[],
    size_t dependency_count, void *user)
{
    (void)user;
    if (message == NULL || result == NULL || dependencies == NULL ||
        dependency_count != 1 || dependencies[0] == NULL) {
        return CLM_DISPATCH_INVALID;
    }
    *(int *)result = *(const int *)message + *(int *)dependencies[0];
    return CLM_DISPATCH_SUCCESS;
}

static void clm_dispatch_test_logger(
    const clm_dispatch_envelope *envelope, void *user)
{
    (void)user;
    clm_dispatch_test_last_envelope = *envelope;
    ++clm_dispatch_test_log_count;
}

static int clm_dispatch_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Dispatch");
    const clm_id dependency_ids[] = {CLM_DISPATCH_TEST_SERVICE_ID};
    int dependency_value = 7;
    int message = 5;
    int output = 0;
    clm_dispatch_test_resolver_context resolver_context = {
        CLM_DISPATCH_TEST_SERVICE_ID, &dependency_value};
    clm_dispatch_test_log_count = 0;
    memset(&clm_dispatch_test_last_envelope, 0,
        sizeof clm_dispatch_test_last_envelope);

    clm_dispatch_destroy();
    clm_dispatch_test_resolver_context missing_context = {
        CLM_DISPATCH_TEST_SERVICE_ID + 1, &dependency_value};
    clm_dispatch_set_resolver(clm_dispatch_test_resolve, &resolver_context);
    CLM_ASSERT(clm_dispatch_register_handler(CLM_DISPATCH_TEST_MESSAGE_ID,
        dependency_ids, 1, clm_dispatch_test_handler, NULL) ==
        CLM_DISPATCH_SUCCESS &&
        clm_dispatch_register_handler(CLM_DISPATCH_TEST_MESSAGE_ID,
            dependency_ids, 1, clm_dispatch_test_handler, NULL) ==
            CLM_DISPATCH_DUPLICATE &&
        clm_dispatch_register_logger(CLM_DISPATCH_COMMAND,
            clm_dispatch_test_logger, NULL) == CLM_DISPATCH_SUCCESS,
        "register a handler, explicit dependency, and command logger");

    clm_dispatch_chain parent = clm_dispatch_start_new();
    CLM_ASSERT(clm_dispatch_execute(CLM_DISPATCH_TEST_MESSAGE_ID, &message,
        &output, true, &parent) == CLM_DISPATCH_SUCCESS && output == 12 &&
        clm_dispatch_test_log_count == 1 &&
        clm_dispatch_test_last_envelope.message_id ==
            CLM_DISPATCH_TEST_MESSAGE_ID &&
        clm_dispatch_test_last_envelope.kind == CLM_DISPATCH_COMMAND &&
        clm_dispatch_test_last_envelope.chain.correlation_id ==
            parent.correlation_id &&
        clm_dispatch_test_last_envelope.chain.causation_id == parent.id,
        "log a child message and invoke it with resolved services");

    clm_dispatch_set_resolver(clm_dispatch_test_resolve, &missing_context);
    CLM_ASSERT(clm_dispatch_invoke(CLM_DISPATCH_TEST_MESSAGE_ID, &message,
        &output) == CLM_DISPATCH_RESOLVE_FAILED &&
        clm_dispatch_unregister_logger(CLM_DISPATCH_COMMAND) ==
            CLM_DISPATCH_SUCCESS &&
        clm_dispatch_unregister_handler(CLM_DISPATCH_TEST_MESSAGE_ID) ==
            CLM_DISPATCH_SUCCESS &&
        clm_dispatch_unregister_handler(CLM_DISPATCH_TEST_MESSAGE_ID) ==
            CLM_DISPATCH_NOT_FOUND,
        "report resolver and missing-registration failures");
    clm_dispatch_destroy();

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}


static int test_clm_dispatch(void)
{
    return clm_dispatch_tests();
}

