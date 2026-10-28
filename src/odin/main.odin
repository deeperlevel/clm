package main

import "clm"
import "core:fmt"
import "core:os"
import "tests"

main :: proc() {
	if !tests.test_log_open("main.log") {
		fmt.eprintln("Failed to open main.log")
		os.exit(1)
	}

	failed := tests.base_tests()
	failed += tests.console_tests()
	failed += tests.bpa_tests()
	failed += tests.dictionary_tests()
	failed += tests.id_tests()
	failed += tests.string_tests()
	failed += tests.storage_tests()
	tests.overall_test_summary("Overall CLM")

	log_closed := tests.test_log_close()
	clm.clm_bpa_shutdown()
	if !log_closed {
		fmt.eprintln("Failed to write or close main.log")
		os.exit(1)
	}
	if failed != 0 {
		os.exit(1)
	}
}