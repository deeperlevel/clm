package main

import (
	"fmt"
	"os"

	"github.com/deeperlevel/clm/src/go/pkg/clm"
	"github.com/deeperlevel/clm/src/go/tests"
)

func main() {
	os.Exit(run())
}

func run() int {
	if !tests.CLMTestLogOpen("main.log") {
		_, err := fmt.Fprintln(os.Stderr, "Unable to open main.log for test output.")
		if err != nil {
			return 0
		}
		return 1
	}
	failed := 0
	for _, suite := range []func() tests.CLMTestResult{
		tests.BaseTests, tests.BpaTests, tests.ConsoleTests, tests.DictionaryTests,
		tests.IDTests, tests.StringTests, tests.StorageTests,
	} {
		failed += suite().Failed
	}
	tests.CLMOverallTestSummary("CLM Tests Summary")
	closed := tests.CLMTestLogClose()
	clm.BpaShutdown()
	if !closed {
		_, err := fmt.Fprintln(os.Stderr, "Failed while writing or closing main.log.")
		if err != nil {
			return 0
		}
	}
	if failed != 0 || !closed {
		return 1
	}
	return 0
}
