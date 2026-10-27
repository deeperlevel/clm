program main;

uses
  SysUtils, bpa, test, test_clm_base, test_clm_bpa, test_clm_console,
  test_clm_dictionary, test_clm_id, test_clm_string, test_clm_storage;

var
  FailedTests: LongInt;
  LogClosed: Boolean;
begin
  if not clm_test_log_open then
  begin
    WriteLn(ErrOutput, 'Unable to open main.log for test output.');
    Halt(1);
  end;

  FailedTests := clm_base_tests;
  Inc(FailedTests, clm_bpa_tests);
  Inc(FailedTests, clm_console_tests);
  Inc(FailedTests, clm_dictionary_tests);
  Inc(FailedTests, clm_id_tests);
  Inc(FailedTests, clm_string_tests);
  Inc(FailedTests, clm_storage_tests);
  clm_overall_test_summary('CLM Tests Summary');

  LogClosed := clm_test_log_close;
  clm_bpa_shutdown;
  if not LogClosed then
    WriteLn(ErrOutput, 'Failed while writing or closing main.log.');
  if (FailedTests <> 0) or not LogClosed then
    Halt(1);
end.