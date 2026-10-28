unit test_clm_id;

{$mode objfpc}{$H+}

interface

function clm_id_tests: LongInt;

implementation

uses
  base, bpa, id, test;

function clm_id_tests: LongInt;
var
  TestResult: clm_test_result;
  Value: clm_id;
begin
  TestResult.passed := 0;
  TestResult.failed := 0;
  clm_test_begin('CLM - ID');
  clm_test_assert(TestResult, clm_id_is_case_sensitive, 'case-sensitive matching is default');
  clm_test_assert(TestResult, clm_id_clear = CLM_BPA_SUCCESS, 'clear ID mappings');
  clm_test_assert(TestResult, clm_id_register('host', 1001) = CLM_BPA_SET_SUCCESS, 'register name');
  clm_test_assert(TestResult, clm_id_get('host') = 1001, 'resolve name');
  clm_test_assert(TestResult, clm_id_get('HOST') = CLM_ID_NOT_FOUND, 'default matching is case-sensitive');
  clm_test_assert(TestResult, clm_id_register('host', 2000) = CLM_BPA_FAILURE, 'reject duplicate name');
  clm_test_assert(TestResult, clm_id_try_get('host', Value) = CLM_BPA_GET_SUCCESS, 'status-based lookup succeeds');
  clm_test_assert(TestResult, Value = 1001, 'status lookup returns value');
  { UTF-8 widths, continuation bytes, and invalid scalar encodings. }
  clm_test_assert(TestResult, clm_id_register(UTF8String(#$C3#$A9), 6001) = CLM_BPA_SET_SUCCESS,
    'accept a valid two-byte UTF-8 name');
  clm_test_assert(TestResult, clm_id_register(UTF8String(#$E4#$B8#$AD), 6002) = CLM_BPA_SET_SUCCESS,
    'accept a valid three-byte UTF-8 name');
  clm_test_assert(TestResult, clm_id_register(UTF8String(#$F0#$9F#$90#$B1), 6003) = CLM_BPA_SET_SUCCESS,
    'accept a valid four-byte UTF-8 name');
  clm_test_assert(TestResult, clm_id_get(UTF8String(#$C3#$A9)) = 6001,
    'resolve two-byte UTF-8 name');
  clm_test_assert(TestResult, clm_id_get(UTF8String(#$F0#$9F#$90#$B1)) = 6003,
    'resolve four-byte UTF-8 name');
  clm_test_assert(TestResult, clm_id_register(UTF8String(#$E2#$28#$A1), 6004) = CLM_BPA_FAILURE,
    'reject malformed UTF-8 continuation');
  clm_test_assert(TestResult, clm_id_register(UTF8String(#$C0#$AF), 6004) = CLM_BPA_FAILURE,
    'reject overlong UTF-8');
  clm_test_assert(TestResult, clm_id_register(UTF8String(#$ED#$A0#$80), 6004) = CLM_BPA_FAILURE,
    'reject UTF-8 surrogate');
  clm_test_assert(TestResult, clm_id_register(UTF8String(#$F4#$90#$80#$80), 6004) = CLM_BPA_FAILURE,
    'reject code points above Unicode maximum');
  clm_test_assert(TestResult, clm_id_register('max-id', CLM_ID_NOT_FOUND) = CLM_BPA_SET_SUCCESS,
    'permit the maximum numeric ID in a named mapping');
  clm_test_assert(TestResult, (clm_id_try_get('max-id', Value) = CLM_BPA_GET_SUCCESS) and
    (Value = CLM_ID_NOT_FOUND), 'status lookup disambiguates a mapped maximum ID');
  clm_test_assert(TestResult, clm_id_register('Host', 1002) = CLM_BPA_SET_SUCCESS, 'register case collision candidate');
  clm_test_assert(TestResult, clm_id_set_case_sensitive(False) = CLM_BPA_FAILURE, 'reject case-fold collision');
  clm_test_assert(TestResult, clm_id_remove('Host') = CLM_BPA_SUCCESS, 'remove collision candidate');
  clm_test_assert(TestResult, clm_id_set_case_sensitive(False) = CLM_BPA_SUCCESS, 'enable ASCII case-insensitive lookup');
  clm_test_assert(TestResult, clm_id_get('HOST') = 1001, 'case-insensitive lookup');
  clm_test_assert(TestResult, clm_id_set('HOST', 2001) = CLM_BPA_SET_SUCCESS, 'update mapping');
  clm_test_assert(TestResult, clm_id_get('host') = 2001, 'updated value is resolved');
  clm_test_assert(TestResult, clm_id_remove('host') = CLM_BPA_SUCCESS, 'remove mapping');
  clm_test_assert(TestResult, clm_id_gen_next = 0, 'generate first sequential ID');
  clm_test_assert(TestResult, clm_id_gen_next = 1, 'generate next sequential ID');
  clm_id_gen_reset;
  clm_test_assert(TestResult, clm_id_gen_next = 0, 'reset generator');
  clm_test_assert(TestResult, clm_id_clear = CLM_BPA_SUCCESS, 'clear registry while preserving case mode');
  clm_test_assert(TestResult, not clm_id_is_case_sensitive, 'case mode survives clear');
  clm_test_assert(TestResult, clm_id_set_case_sensitive(True) = CLM_BPA_SUCCESS, 'restore case-sensitive mode');
  clm_test_assert(TestResult, clm_id_shutdown = CLM_BPA_SUCCESS, 'shutdown ID registry');
  clm_test_summary(TestResult.passed, TestResult.failed);
  Result := TestResult.failed;
end;

end.