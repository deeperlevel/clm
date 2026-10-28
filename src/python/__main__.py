"""Run the CLM checks and write their plain-text log to ``__main__.log``."""

from clm.console import clm_console_write_line
from tests.test import (
    clm_overall_test_summary,
    clm_test_log_close,
    clm_test_log_open,
)
from tests.test_clm_base import run_tests as run_base_tests
from tests.test_clm_bpa import run_tests as run_bpa_tests
from tests.test_clm_console import run_tests as run_console_tests
from tests.test_clm_dictionary import run_tests as run_dictionary_tests
from tests.test_clm_id import run_tests as run_id_tests
from tests.test_clm_storage import run_tests as run_storage_tests
from tests.test_clm_string import run_tests as run_string_tests


def main() -> int:
    """Run the registered checks and report assertion or logging failures."""
    if not clm_test_log_open():
        clm_console_write_line("FAILED: could not open __main__.log")
        return 1

    try:
        failed = (
            run_base_tests()
            + run_bpa_tests()
            + run_dictionary_tests()
            + run_id_tests()
            + run_string_tests()
            + run_storage_tests()
            + run_console_tests()
        )
        clm_overall_test_summary("CLM - Python tests")
    finally:
        log_closed = clm_test_log_close()

    if not log_closed:
        clm_console_write_line("FAILED: could not write __main__.log")
    return int(failed != 0 or not log_closed)


if __name__ == "__main__":
    raise SystemExit(main())