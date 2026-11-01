#pragma once

#include "test.h"
#include "../clm/tls.h"

static int clm_tls_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - TLS");

    clm_tls_session* session = (clm_tls_session*)(uintptr_t)1;
    unsigned char byte = 0;
    size_t transferred = 99;
    clm_tls_options invalid_server = {0};
    CLM_ASSERT(clm_tls_client_open(1, NULL, NULL, &session) == CLM_TLS_INVALID_ARGUMENT &&
                   session == (clm_tls_session*)(uintptr_t)1 &&
                   clm_tls_client_open(1, "", NULL, &session) == CLM_TLS_INVALID_ARGUMENT &&
                   clm_tls_server_open(1, &invalid_server, &session) == CLM_TLS_INVALID_ARGUMENT,
               "reject invalid client names and missing server credentials without changing output");
    CLM_ASSERT(clm_tls_read(NULL, &byte, 1, &transferred) == CLM_TLS_INVALID_ARGUMENT &&
                   clm_tls_write(NULL, &byte, 1, &transferred) == CLM_TLS_INVALID_ARGUMENT &&
                   clm_tls_close(NULL) == CLM_TLS_INVALID_ARGUMENT,
               "reject operations on a missing TLS session");
    CLM_ASSERT(clm_tls_client_open(1, "example.test", NULL, NULL) == CLM_TLS_INVALID_ARGUMENT &&
                   clm_tls_server_open(1, NULL, NULL) == CLM_TLS_INVALID_ARGUMENT,
               "require a caller-owned session output");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0;
}