unit test_clm_dictionary;

{$mode objfpc}{$H+}

interface

function clm_dictionary_tests: LongInt;

implementation

uses
  base, bpa, dictionary, test;

function clm_dictionary_tests: LongInt;
var
  TestResult: clm_test_result;
  Value, ReadValue: clm_int;
  Key: clm_id;
  Keys: array[0..2] of clm_id;
  Text: array[0..31] of AnsiChar;
  Source: array[0..5] of AnsiChar;
begin
  TestResult.passed := 0;
  TestResult.failed := 0;
  clm_test_begin('CLM - Dictionary');
  clm_test_assert(TestResult, clm_dictionary_new(71000, CLM_BPA_TYPE_INT, 0) = CLM_BPA_SUCCESS, 'create typed dictionary');
  Value := 10;
  clm_test_assert(TestResult, clm_dictionary_set_from(71000, 1, @Value, SizeOf(Value)) = CLM_BPA_SET_SUCCESS, 'insert first key');
  Value := 20;
  clm_test_assert(TestResult, clm_dictionary_add_from(71000, 2, @Value, SizeOf(Value)) = CLM_BPA_SET_SUCCESS, 'insert second key');
  clm_test_assert(TestResult, clm_dictionary_add_from(71000, 2, @Value, SizeOf(Value)) = CLM_BPA_FAILURE, 'reject duplicate add');
  Value := 22;
  clm_test_assert(TestResult, clm_dictionary_update_from(71000, 2, @Value, SizeOf(Value)) = CLM_BPA_SET_SUCCESS, 'update existing value');
  clm_test_assert(TestResult, clm_dictionary_count(71000) = 2, 'count inserted keys');
  clm_test_assert(TestResult, clm_dictionary_get_to(71000, 2, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_GET_SUCCESS, 'get value to caller buffer');
  clm_test_assert(TestResult, ReadValue = 22, 'updated value is stored');
  clm_test_assert(TestResult, clm_dictionary_keys_to(71000, @Keys[0], Length(Keys)) = CLM_BPA_GET_SUCCESS, 'copy ordered keys');
  clm_test_assert(TestResult, (Keys[0] = 1) and (Keys[1] = 2), 'keys retain insertion order');
  clm_test_assert(TestResult, clm_dictionary_iterate_reset(71000) = CLM_BPA_SUCCESS, 'reset dictionary iteration');
  clm_test_assert(TestResult, (clm_dictionary_iterate_next(71000, Key) = CLM_BPA_GET_SUCCESS) and (Key = 1), 'iterate first key');
  clm_test_assert(TestResult, clm_dictionary_remove(71000, 1) = CLM_BPA_SUCCESS, 'remove key');
  clm_test_assert(TestResult, not clm_dictionary_contains(71000, 1), 'removed key is absent');
  clm_test_assert(TestResult, clm_dictionary_clear(71000) = CLM_BPA_SUCCESS, 'clear dictionary');
  clm_test_assert(TestResult, clm_dictionary_count(71000) = 0, 'cleared dictionary is empty');
  clm_test_assert(TestResult, clm_dictionary_delete(71000) = CLM_BPA_SUCCESS, 'delete dictionary');

  clm_test_assert(TestResult, clm_dictionary_new(71010, CLM_BPA_TYPE_STRING, 0) = CLM_BPA_SUCCESS, 'create string dictionary');
  Source[0] := 'v'; Source[1] := 'a'; Source[2] := 'l'; Source[3] := 'u'; Source[4] := 'e'; Source[5] := #0;
  clm_test_assert(TestResult, clm_dictionary_set_from(71010, 5, @Source[0], SizeOf(Source)) = CLM_BPA_SET_SUCCESS, 'store string value');
  FillChar(Text, SizeOf(Text), 0);
  clm_test_assert(TestResult, clm_dictionary_get_to(71010, 5, @Text[0], SizeOf(Text)) = CLM_BPA_GET_SUCCESS, 'retrieve string value');
  clm_test_assert(TestResult, StrPas(@Text[0]) = 'value', 'string bytes match');
  clm_test_assert(TestResult, clm_dictionary_delete(71010) = CLM_BPA_SUCCESS, 'delete string dictionary');

  clm_test_summary(TestResult.passed, TestResult.failed);
  Result := TestResult.failed;
end;

end.