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

#define CLM_BPA_IMPLEMENTATION
#define CLM_STRING_IMPLEMENTATION
#define CLM_ID_IMPLEMENTATION

#include "tests/test.h"
#include "tests/test_clm_base.h"
#include "tests/test_clm_bpa.h"
#include "tests/test_clm_console.h"
#include "tests/test_clm_dictionary.h"
#include "tests/test_clm_id.h"
#include "tests/test_clm_string.h"
#include "tests/test_clm_storage.h"

int main(void)
{
    if (!clm_test_log_open("main.log")) {
        fputs("Unable to open main.log for test output.\n", stderr);
        return 1;
    }

    clm_base_tests();
    clm_bpa_tests();
    clm_console_tests();
    clm_dictionary_tests();
    clm_id_tests();
    clm_string_tests();
    clm_storage_tests();
    clm_overall_test_summary("CLM Tests Summary");

    const int log_closed = clm_test_log_close();
    clm_bpa_shutdown();
    if (!log_closed)
        fputs("Failed while writing or closing main.log.\n", stderr);
    return clm_tests_failed == 0 && log_closed ? 0 : 1;
}