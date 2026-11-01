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

#include "clm/net.h"
#include "tests/test.h"
#include "tests/test_clm_base.h"
#include "tests/test_clm_bpa.h"
#include "tests/test_clm_arena.h"
#include "tests/test_clm_hash.h"
#include "tests/test_clm_crypto.h"
#include "tests/test_clm_console.h"
#include "tests/test_clm_dictionary.h"
#include "tests/test_clm_tree.h"
#include "tests/test_clm_table.h"
#include "tests/test_clm_string_format.h"
#include "tests/test_clm_string_dsv.h"
#include "tests/test_clm_string_ini.h"
#include "tests/test_clm_string_env.h"
#include "tests/test_clm_string_toml.h"
#include "tests/test_clm_string_json.h"
#include "tests/test_clm_string_yaml.h"
#include "tests/test_clm_string_httpp.h"
#include "tests/test_clm_bytes.h"
#include "tests/test_clm_gen.h"
#include "tests/test_clm_id.h"
#include "tests/test_clm_string.h"
#include "tests/test_clm_storage.h"
#include "tests/test_clm_path.h"
#include "tests/test_clm_url.h"
#include "tests/test_clm_file.h"
#include "tests/test_clm_directory.h"
#include "tests/test_clm_async.h"
#include "tests/test_clm_container.h"
#include "tests/test_clm_channel.h"
#include "tests/test_clm_dispatch.h"
#include "tests/test_clm_ecs.h"
#include "tests/test_clm_managed.h"
#include "tests/test_clm_clp.h"
#include "tests/test_clm_ua.h"
#include "tests/test_clm_math.h"
#include "tests/test_clm_net.h"
#include "tests/test_clm_stream.h"
#include "tests/test_clm_template.h"
#include "tests/test_clm_compress.h"
#include "tests/test_clm_multipart.h"
#include "tests/test_clm_tls.h"
#include "tests/test_clm_net_web.h"

int main(void)
{
    if (!clm_test_log_open("main.log")) {
        fputs("Unable to open main.log for test output.\n", stderr);
        return 1;
    }

    clm_base_tests();
    clm_bpa_tests();
    clm_arena_tests();
    clm_hash_tests();
    clm_crypto_tests();
    clm_console_tests();
    clm_dictionary_tests();
    clm_tree_tests();
    clm_table_tests();
    clm_string_format_tests();
    clm_string_ini_tests();
    clm_string_env_tests();
    clm_string_toml_tests();
    clm_string_json_tests();
    clm_string_yaml_tests();
    clm_string_httpp_tests();
    clm_bytes_tests();
    clm_gen_tests();
    clm_id_tests();
    clm_string_dsv_tests();
    clm_string_tests();
    clm_storage_tests();
    clm_path_tests();
    clm_url_tests();
    clm_file_tests();
    clm_directory_tests();
    clm_async_tests();
    clm_container_tests();
    clm_channel_tests();
    clm_dispatch_tests();
    clm_ecs_tests();
    clm_managed_tests();
    clm_clp_tests();
    clm_ua_tests();
    clm_math_tests();
    clm_net_tests();
    clm_stream_tests();
    clm_template_tests();
    clm_compress_tests();
    clm_multipart_tests();
    clm_tls_tests();
    clm_net_web_tests();
    clm_overall_test_summary("CLM Tests Summary");

    const int files_closed = clm_file_shutdown();
    const int log_closed = clm_test_log_close();
    clm_bpa_shutdown();
    if (!log_closed)
        fputs("Failed while writing or closing main.log.\n", stderr);
    if (files_closed != 0)
        fputs("Failed while closing registered files.\n", stderr);
    return clm_tests_failed == 0 && log_closed && files_closed == 0 ? 0 : 1;
}