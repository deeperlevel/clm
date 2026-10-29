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

#include "tests/test_clm_base.hpp"
#include "tests/test_clm_bpa.hpp"
#include "tests/test_clm_console.hpp"
#include "tests/test_clm_dictionary.hpp"
#include "tests/test_clm_id.hpp"
#include "tests/test_clm_storage.hpp"
#include "tests/test_clm_string.hpp"
#include "tests/test_clm_clp.hpp"
#include "tests/test_clm_channel.hpp"
#include "tests/test_clm_async.hpp"
#include "tests/test_clm_path.hpp"
#include "tests/test_clm_container.hpp"

auto main() -> int {
    if (!clm_test_log_open("main.log")) {
        std::fputs("Unable to open main.log for test output.\n", stderr);
        return 1;
    }

    test_clm_base();
    test_clm_bpa();
    test_clm_console();
    test_clm_dictionary();
    test_clm_id();
    test_clm_string();
    test_clm_storage();
    test_clm_clp();
    test_clm_channel();
    test_clm_async();
    test_clm_path();
    test_clm_container();
    clm_overall_test_summary("CLM Tests Summary");

    const int log_closed = clm_test_log_close();
    clm::bpa::shutdown();
    if (!log_closed)
        std::fputs("Failed while writing or closing main.log.\n", stderr);
    return clm_tests_failed == 0 && log_closed ? 0 : 1;
}