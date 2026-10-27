package tests

import "core:fmt"

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

test_result :: struct {
	passed: i32,
	failed: i32,
}

tests_passed: i32 = 0
tests_failed: i32 = 0

assert_test :: proc(result: ^test_result, condition: bool) {
	if condition {
		result.passed += 1
	} else {
		result.failed += 1
	}
}

test_summary :: proc(title: string, passed, failed: i32) {
	tests_passed += passed
	tests_failed += failed

	fmt.printf("\n%s Test Results\n", title)
	fmt.println("--------------------------------------------------")
	fmt.printf("Passed: %d\n", passed)
	fmt.printf("Failed: %d\n", failed)
	fmt.printf("Total: %d\n", passed + failed)
}

overall_test_summary :: proc(title: string) {
	fmt.printf("\n%s Test Results\n", title)
	fmt.println("--------------------------------------------------")
	fmt.printf("Passed: %d\n", tests_passed)
	fmt.printf("Failed: %d\n", tests_failed)
	fmt.printf("Total: %d\n", tests_passed + tests_failed)
}
