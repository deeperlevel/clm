unit test_clm_storage;

{$mode objfpc}{$H+}

interface

function clm_storage_tests: LongInt;

implementation

uses
  base, bpa, storage, test;

function clm_storage_tests: LongInt;
var
  TestResult: clm_test_result;
  Value, ReadValue: clm_int;
  Info: clm_storage_info;
  Frame, ReadFrame: array[0..3] of Byte;
begin
  TestResult.passed := 0;
  TestResult.failed := 0;
  clm_test_begin('CLM - Storage');
  clm_test_assert(TestResult, clm_storage_new_2d(100, CLM_BPA_TYPE_INT, 0, 4, 3) = CLM_BPA_SUCCESS, 'create two-dimensional grid');
  clm_test_assert(TestResult, clm_storage_get_to(100, 0, 2, 3, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_GET_SUCCESS, 'read zero-initialized cell');
  clm_test_assert(TestResult, ReadValue = 0, 'new storage is zero-initialized');
  Value := 72;
  clm_test_assert(TestResult, clm_storage_set_from(100, 0, 1, 2, @Value, SizeOf(Value)) = CLM_BPA_SET_SUCCESS, 'write grid cell');
  clm_test_assert(TestResult, clm_storage_get_to(100, 0, 1, 2, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_GET_SUCCESS, 'read grid cell');
  clm_test_assert(TestResult, ReadValue = 72, 'grid cell round-trips');
  clm_test_assert(TestResult, clm_storage_get_to(100, 1, 0, 0, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_FAILURE, 'reject out-of-range layer');
  clm_test_assert(TestResult, clm_storage_get_info(100, Info) = CLM_BPA_GET_SUCCESS, 'query storage metadata');
  clm_test_assert(TestResult, (Info.columns = 4) and (Info.rows = 3) and (Info.layers = 1), 'metadata dimensions match');
  clm_test_assert(TestResult, clm_storage_slice(100, 0, 1, 1, 1, 2, 2, 101) = CLM_BPA_SUCCESS, 'copy grid slice');
  clm_test_assert(TestResult, clm_storage_get_to(101, 0, 0, 1, @ReadValue, SizeOf(ReadValue)) = CLM_BPA_GET_SUCCESS, 'read copied slice');
  clm_test_assert(TestResult, ReadValue = 72, 'slice preserves selected value');
  clm_test_assert(TestResult, clm_storage_delete(101) = CLM_BPA_SUCCESS, 'delete slice');
  clm_test_assert(TestResult, clm_storage_delete(100) = CLM_BPA_SUCCESS, 'delete grid');

  clm_test_assert(TestResult, clm_storage_atlas_new(120, CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2) = CLM_BPA_SUCCESS, 'create atlas');
  Frame[0] := 1; Frame[1] := 2; Frame[2] := 3; Frame[3] := 4;
  clm_test_assert(TestResult, clm_storage_atlas_set_frame_from(120, 0, 1, 1, @Frame[0], SizeOf(Frame)) = CLM_BPA_SET_SUCCESS, 'write atlas frame');
  clm_test_assert(TestResult, clm_storage_atlas_get_frame_to(120, 0, 1, 1, @ReadFrame[0], SizeOf(ReadFrame)) = CLM_BPA_GET_SUCCESS, 'read atlas frame');
  clm_test_assert(TestResult, (ReadFrame[0] = 1) and (ReadFrame[3] = 4), 'frame bytes round-trip');
  clm_test_assert(TestResult, clm_storage_atlas_slice_frame(120, 0, 1, 1, 121) = CLM_BPA_SUCCESS, 'copy atlas frame to storage');
  clm_test_assert(TestResult, clm_storage_delete(121) = CLM_BPA_SUCCESS, 'delete copied frame');
  clm_test_assert(TestResult, clm_storage_delete(120) = CLM_BPA_SUCCESS, 'delete atlas');

  clm_test_summary(TestResult.passed, TestResult.failed);
  Result := TestResult.failed;
end;

end.