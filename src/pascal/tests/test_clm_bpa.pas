unit test_clm_bpa;

{$mode objfpc}{$H+}

interface

function clm_bpa_tests: LongInt;

implementation

uses
  base, bpa, test;

type
  PInt = ^clm_int;

function CompareInts(Left, Right: Pointer): LongInt;
begin
  if PInt(Left)^ > PInt(Right)^ then Result := 1
  else if PInt(Left)^ < PInt(Right)^ then Result := -1
  else Result := 0;
end;

function clm_bpa_tests: LongInt;
var
  TestResult: clm_test_result;
  Value, ReadValue: clm_int;
  Count, Capacity: clm_uint;
  Text: array[0..31] of AnsiChar;
  Borrowed: pclm_bpaa_array_node;
  Active, Reserved: SizeUInt;
begin
  TestResult.passed := 0;
  TestResult.failed := 0;
  clm_test_begin('CLM - Block Pool Allocation (BPA)');

  clm_test_assert(TestResult, not clm_bpa_is_existing(1), 'new block ID is absent');
  clm_test_assert(TestResult, clm_bpa_create(1, CLM_BPA_TYPE_INT) = CLM_BPA_SUCCESS, 'create typed BPA block');
  Value := 9;
  clm_test_assert(TestResult, clm_bpa_set(1, @Value, SizeOf(Value)) = CLM_BPA_SET_SUCCESS, 'set block bytes');
  ReadValue := 0;
  clm_test_assert(TestResult, (clm_bpa_get(1, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_GET_SUCCESS) and (ReadValue = 9), 'get block bytes');
  clm_test_assert(TestResult, clm_bpa_get(1, @ReadValue, 1) = CLM_BPA_FAILURE, 'reject undersized block output');
  clm_test_assert(TestResult, clm_bpa_ref(1) = CLM_BPA_SUCCESS, 'retain block');
  clm_test_assert(TestResult, clm_bpa_unref(1) = CLM_BPA_SUCCESS, 'release one block reference');
  clm_test_assert(TestResult, clm_bpa_unref(1) = CLM_BPA_SUCCESS, 'release final block reference');
  clm_test_assert(TestResult, not clm_bpa_is_existing(1), 'final unref deletes block');

  clm_test_assert(TestResult, clm_bpa_create(2, CLM_BPA_TYPE_STRING) = CLM_BPA_SUCCESS, 'create BPA string');
  clm_test_assert(TestResult, clm_bpa_set_string(2, PAnsiChar('hello'), 6) = 5, 'set BPA string');
  FillChar(Text, SizeOf(Text), 0);
  clm_test_assert(TestResult, (clm_bpa_get_string(2, @Text[0], SizeOf(Text)) = 5) and (StrPas(@Text[0]) = 'hello'), 'get BPA string');
  clm_test_assert(TestResult, clm_bpa_delete(2) = CLM_BPA_SUCCESS, 'delete BPA string');

  clm_test_assert(TestResult, clm_bpaa_create(10, CLM_BPA_TYPE_INT, 1, 0) = CLM_BPA_SUCCESS, 'create fixed-width BPAA');
  Value := 4; clm_test_assert(TestResult, clm_bpaa_append_raw(10, @Value, SizeOf(Value)) = CLM_BPA_SUCCESS, 'append first array item');
  Value := 1; clm_test_assert(TestResult, clm_bpaa_append_raw(10, @Value, SizeOf(Value)) = CLM_BPA_SUCCESS, 'append second array item');
  Value := 3; clm_test_assert(TestResult, clm_bpaa_append_raw(10, @Value, SizeOf(Value)) = CLM_BPA_SUCCESS, 'append third array item');
  clm_test_assert(TestResult, (clm_bpaa_get_count_to(10, Count) = CLM_BPA_GET_SUCCESS) and (Count = 3), 'query BPAA count');
  clm_test_assert(TestResult, (clm_bpaa_get_capacity_to(10, Capacity) = CLM_BPA_GET_SUCCESS) and (Capacity >= Count), 'query BPAA capacity');
  clm_test_assert(TestResult, clm_bpaa_get_first_raw(10, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_GET_SUCCESS, 'get first array item');
  clm_test_assert(TestResult, ReadValue = 4, 'first item value is retained');
  clm_test_assert(TestResult, clm_bpaa_insert_raw(10, 1, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_SUCCESS, 'insert array item');
  clm_test_assert(TestResult, clm_bpaa_sort_with(10, @CompareInts) = CLM_BPA_SUCCESS, 'sort fixed-width array');
  clm_test_assert(TestResult, clm_bpaa_get_raw(10, 0, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_GET_SUCCESS, 'read sorted first item');
  clm_test_assert(TestResult, ReadValue = 1, 'sort orders array values');
  clm_test_assert(TestResult, clm_bpaa_reverse(10) = CLM_BPA_SUCCESS, 'reverse array');
  clm_test_assert(TestResult, clm_bpaa_move_last(10) = CLM_BPA_SUCCESS, 'move array cursor to last');
  clm_test_assert(TestResult, clm_bpaa_get_current_index_to(10, Value) = CLM_BPA_GET_SUCCESS, 'query array cursor');
  clm_test_assert(TestResult, Value = 3, 'cursor has last index');
  clm_test_assert(TestResult, clm_bpaa_extend(10, 2) = CLM_BPA_SUCCESS, 'extend capacity');
  clm_test_assert(TestResult, clm_bpaa_remove_first(10) = CLM_BPA_SUCCESS, 'remove first item');
  clm_test_assert(TestResult, clm_bpaa_clear(10) = CLM_BPA_SUCCESS, 'clear array');
  clm_test_assert(TestResult, clm_bpaa_delete(10) = CLM_BPA_SUCCESS, 'delete array');

  clm_test_assert(TestResult, clm_bpaa_create(11, CLM_BPA_TYPE_INT, 1, 0) = CLM_BPA_SUCCESS, 'create array for advanced borrow');
  Value := 17;
  clm_test_assert(TestResult, clm_bpaa_append_raw(11, @Value, SizeOf(Value)) = CLM_BPA_SUCCESS, 'append advanced-array value');
  clm_test_assert(TestResult, clm_bpaa_advanced_borrow(11, Borrowed) = CLM_BPA_SUCCESS, 'borrow array metadata');
  clm_test_assert(TestResult, (Borrowed <> nil) and (clm_bpaa_advanced_get_raw(Borrowed, 0, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_GET_SUCCESS) and (ReadValue = 17), 'read through advanced borrow');
  clm_test_assert(TestResult, clm_bpaa_unref(11) = CLM_BPA_SUCCESS, 'ordinary unref retains borrowed array');
  clm_test_assert(TestResult, clm_bpaa_advanced_release(Borrowed) = CLM_BPA_SUCCESS, 'release advanced borrow');

  clm_test_assert(TestResult, clm_bpa_pool_stats(SizeOf(Value), Active, Reserved) = CLM_BPA_SUCCESS, 'read pool statistics');
  clm_test_assert(TestResult, clm_bpa_pool_cleanup = CLM_BPA_SUCCESS, 'clean empty allocation pools');
  clm_test_summary(TestResult.passed, TestResult.failed);
  Result := TestResult.failed;
end;

end.