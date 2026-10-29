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

#include "test.h"
#include "../clm/platform.h"

static int clm_base_tests_passed = 0;
static int clm_base_tests_failed = 0;

/* Avoid <string.h>: CLM has a local string.h with that name. */
static int test_clm_platform_streq(const char *a, const char *b)
{
    if (!a || !b) return a == b;
    while (*a && *b) {
        if (*a != *b) return 0;
        ++a;
        ++b;
    }
    return *a == *b;
}

static int test_clm_platform_is_known(int id, const clm_id *ids, int count)
{
    int i;
    for (i = 0; i < count; ++i)
        if (id == (int)ids[i]) return 1;
    return 0;
}

static int clm_platform_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Base");

    int failures = 0;
    char buffer[256];
    int os = clm_platform_get_os();
    int arch = clm_platform_get_arch();
    int compiler = clm_platform_get_compiler();
    int endian = clm_platform_get_endian();
    int flags;
    const clm_id os_ids[] = {
        PLATFORM_INFO_OS_WINDOWS, 
        PLATFORM_INFO_OS_MACOS,
        PLATFORM_INFO_OS_LINUX,
        PLATFORM_INFO_OS_BSD,
        PLATFORM_INFO_OS_ANDROID, 
        PLATFORM_INFO_OS_IOS,
        PLATFORM_INFO_OS_UNKNOWN
    };
    const clm_id arch_ids[] = {
        PLATFORM_INFO_ARCH_X86, 
        PLATFORM_INFO_ARCH_X64,
        PLATFORM_INFO_ARCH_ARM32, 
        PLATFORM_INFO_ARCH_ARM64,
        PLATFORM_INFO_ARCH_RISCV32, 
        PLATFORM_INFO_ARCH_RISCV64,
        PLATFORM_INFO_ARCH_WASM32, 
        PLATFORM_INFO_ARCH_WASM64,
        PLATFORM_INFO_ARCH_UNKNOWN
    };
    const clm_id compiler_ids[] = {
        PLATFORM_INFO_COMPILER_MSVC, PLATFORM_INFO_COMPILER_GCC,
        PLATFORM_INFO_COMPILER_CLANG, PLATFORM_INFO_COMPILER_UNKNOWN
    };

    #define CLM_PLATFORM_CHECK(condition) do { \
        if (!(condition)) { \
            fprintf(stderr, "[FAIL] clm_platform: %s (line %d)\n", #condition, __LINE__); \
            ++failures; \
        } \
    } while (0)

    puts("[TEST] clm_platform");

    /* IDs are stable, known identities. */
    CLM_PLATFORM_CHECK(test_clm_platform_is_known(os, os_ids, (int)(sizeof os_ids / sizeof os_ids[0])));
    CLM_PLATFORM_CHECK(test_clm_platform_is_known(arch, arch_ids, (int)(sizeof arch_ids / sizeof arch_ids[0])));
    CLM_PLATFORM_CHECK(test_clm_platform_is_known(compiler, compiler_ids, (int)(sizeof compiler_ids / sizeof compiler_ids[0])));
    CLM_PLATFORM_CHECK(endian == (int)PLATFORM_INFO_ENDIAN_LITTLE ||
                       endian == (int)PLATFORM_INFO_ENDIAN_BIG ||
                       endian == (int)PLATFORM_INFO_ENDIAN_UNKNOWN);

    /* These identity IDs are also the BPA blocks for their names. */
    CLM_PLATFORM_CHECK(clm_platform_get_os_name() == os);
    CLM_PLATFORM_CHECK(clm_platform_get_arch_name() == arch);
    CLM_PLATFORM_CHECK(clm_platform_get_compiler_name() == compiler);
    CLM_PLATFORM_CHECK(clm_bpa_is_existing((clm_id)os));
    CLM_PLATFORM_CHECK(clm_bpa_is_existing((clm_id)arch));
    CLM_PLATFORM_CHECK(clm_bpa_is_existing((clm_id)compiler));

    /* Names are exact compile-time labels. */
    buffer[0] = '\0';
    clm_platform_get_os_name_to(buffer, (int)sizeof buffer);
    CLM_PLATFORM_CHECK(test_clm_platform_streq(buffer, CLM_PLATFORM_OS_NAME));
    buffer[0] = '\0';
    clm_platform_get_arch_name_to(buffer, (int)sizeof buffer);
    CLM_PLATFORM_CHECK(test_clm_platform_streq(buffer, CLM_PLATFORM_ARCH_NAME));
    buffer[0] = '\0';
    clm_platform_get_compiler_name_to(buffer, (int)sizeof buffer);
    CLM_PLATFORM_CHECK(test_clm_platform_streq(buffer, CLM_PLATFORM_COMPILER_NAME));

    /* Version hints are nonempty, and stored in designated BPA blocks. */
    CLM_PLATFORM_CHECK(clm_platform_get_os_version() == (int)PLATFORM_BLOCK_OS_VERSION_ID);
    CLM_PLATFORM_CHECK(clm_platform_get_compiler_version() == (int)PLATFORM_BLOCK_COMPILER_VERSION_ID);
    buffer[0] = '\0';
    clm_platform_get_os_version_to(buffer, (int)sizeof buffer);
    CLM_PLATFORM_CHECK(test_clm_platform_streq(buffer, CLM_PLATFORM_OS_VERSION));
    buffer[0] = '\0';
    clm_platform_get_compiler_version_to(buffer, (int)sizeof buffer);
    CLM_PLATFORM_CHECK(test_clm_platform_streq(buffer, CLM_PLATFORM_COMPILER_VERSION));

    /* Platform-conventional path separator and newline; no I/O side effects. */
    CLM_PLATFORM_CHECK(clm_platform_get_path_separator() == CLM_PLATFORM_PATH_SEPARATOR);
    CLM_PLATFORM_CHECK(clm_platform_get_newline() == (int)PLATFORM_BLOCK_NEWLINE_ID);
    buffer[0] = '\0';
    clm_platform_get_newline_to(buffer, (int)sizeof buffer);
    CLM_PLATFORM_CHECK(test_clm_platform_streq(buffer, CLM_PLATFORM_NEWLINE));
    if (os == (int)PLATFORM_INFO_OS_WINDOWS) {
        CLM_PLATFORM_CHECK(clm_platform_get_path_separator() == '\\');
        CLM_PLATFORM_CHECK(test_clm_platform_streq(buffer, "\r\n"));
    } else {
        CLM_PLATFORM_CHECK(clm_platform_get_path_separator() == '/');
        CLM_PLATFORM_CHECK(test_clm_platform_streq(buffer, "\n"));
    }

    /* Pointer width is a property of the compiled binary. */
    CLM_PLATFORM_CHECK(clm_platform_get_bits() == (int)(sizeof(void *) * 8u));
    CLM_PLATFORM_CHECK(clm_platform_get_bits() == 32 || clm_platform_get_bits() == 64);

    /* Individual OS predicates must agree exactly with the OS ID. */
    CLM_PLATFORM_CHECK(!!clm_platform_is_windows() == (os == (int)PLATFORM_INFO_OS_WINDOWS));
    CLM_PLATFORM_CHECK(!!clm_platform_is_linux()   == (os == (int)PLATFORM_INFO_OS_LINUX));
    CLM_PLATFORM_CHECK(!!clm_platform_is_bsd()     == (os == (int)PLATFORM_INFO_OS_BSD));
    CLM_PLATFORM_CHECK(!!clm_platform_is_macos()   == (os == (int)PLATFORM_INFO_OS_MACOS));
    CLM_PLATFORM_CHECK(!!clm_platform_is_android() == (os == (int)PLATFORM_INFO_OS_ANDROID));
    CLM_PLATFORM_CHECK(!!clm_platform_is_ios()     == (os == (int)PLATFORM_INFO_OS_IOS));

    flags = !!clm_platform_is_windows() + !!clm_platform_is_linux() +
            !!clm_platform_is_bsd() + !!clm_platform_is_macos() +
            !!clm_platform_is_android() + !!clm_platform_is_ios();
    CLM_PLATFORM_CHECK(flags == (os == (int)PLATFORM_INFO_OS_UNKNOWN ? 0 : 1));
    CLM_PLATFORM_CHECK(!!clm_platform_is_mobile() ==
                       (os == (int)PLATFORM_INFO_OS_ANDROID || os == (int)PLATFORM_INFO_OS_IOS));
    CLM_PLATFORM_CHECK(!!clm_platform_is_desktop() ==
                       (os == (int)PLATFORM_INFO_OS_WINDOWS || os == (int)PLATFORM_INFO_OS_LINUX ||
                        os == (int)PLATFORM_INFO_OS_BSD || os == (int)PLATFORM_INFO_OS_MACOS));
    CLM_PLATFORM_CHECK(!(clm_platform_is_mobile() && clm_platform_is_desktop()));

    printf("[INFO] OS=%s, Arch=%s (%d-bit), Compiler=%s\n",
           CLM_PLATFORM_OS_NAME, CLM_PLATFORM_ARCH_NAME,
           clm_platform_get_bits(), CLM_PLATFORM_COMPILER_NAME);
    printf("[%s] clm_platform (%d failure%s)\n", failures ? "FAIL" : "PASS",
           failures, failures == 1 ? "" : "s");

    #undef CLM_PLATFORM_CHECK

    clm_test_summary(result.passed, result.failed);

    return result.failed;
}
