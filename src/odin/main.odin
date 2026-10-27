package main

import "tests"

main :: proc() {
    tests.base_tests()
    tests.overall_test_summary("Overall CLM")
}