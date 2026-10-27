unit test_clm_string;

{$mode objfpc}{$H+}

interface

function clm_string_tests: LongInt;

implementation

uses
  base, bpa, clm_string, test;

function RawUtf8(const Bytes: array of Byte): UTF8String;
var
  Index: SizeInt;
begin
  SetLength(Result, Length(Bytes));
  for Index := 0 to High(Bytes) do
    Result[Index + 1] := AnsiChar(Bytes[Index]);
end;

function clm_string_tests: LongInt;
var
  TestResult: clm_test_result;
  Bytes: array[0..7] of Byte;
  TextBlock: pclm_bpa_block_node;
  ArrayNode: pclm_bpaa_array_node;
  ByteCount: clm_int;
  Accent, Cat, Chinese, InitialText: UTF8String;
begin
  TestResult.passed := 0;
  TestResult.failed := 0;
  clm_test_begin('CLM - String');
  Accent := RawUtf8([$C3, $A9]);
  Cat := RawUtf8([$F0, $9F, $90, $B1]);
  Chinese := RawUtf8([$E4, $B8, $AD]);
  InitialText := 'A' + Accent + Cat;
  clm_test_assert(TestResult, clm_string_new(90001, InitialText), 'create UTF-8 string');
  clm_test_assert(TestResult, clm_string_length(90001) = 3, 'length counts Unicode code points');
  clm_test_assert(TestResult, clm_string_append(90001, '!') = 1, 'append text');
  clm_test_assert(TestResult, clm_string_insert(90001, 1, Chinese) = 1, 'insert at code-point index');
  clm_test_assert(TestResult, clm_string_length(90001) = 5, 'inserted scalar increments length');
  clm_test_assert(TestResult, clm_string_remove(90001, 1, 1) = 1, 'remove code-point range');
  clm_test_assert(TestResult, clm_string_contains(90001, Accent), 'accent survives insertion and removal');
  clm_test_assert(TestResult, clm_string_contains(90001, Cat), 'cat survives insertion and removal');
  clm_test_assert(TestResult, clm_string_replace(90001, Accent, Chinese) = 1, 'replace UTF-8 match');
  clm_test_assert(TestResult, clm_string_contains(90001, Cat), 'search UTF-8 content');
  clm_test_assert(TestResult, clm_string_append_line(90001, '!') = 2, 'append text and newline');
  clm_test_assert(TestResult, clm_string_remove(90001, -1, 1) = CLM_BPA_FAILURE, 'reject negative character index');

  clm_test_assert(TestResult, clm_string_set_default_encoding(CLM_STRING_ENCODING_LATIN1) = CLM_BPA_SUCCESS, 'set Latin-1 export default');
  clm_test_assert(TestResult, clm_string_new(90002, 'caf' + Accent), 'create accented string');
  clm_test_assert(TestResult, clm_string_get_bytes(90002, nil, 0) = 4, 'query Latin-1 encoded byte length');
  FillChar(Bytes, SizeOf(Bytes), 0);
  ByteCount := clm_string_get_bytes(90002, @Bytes[0], SizeOf(Bytes));
  clm_test_assert(TestResult, (ByteCount = 4) and (Bytes[3] = $E9), 'export Latin-1 byte');
  clm_test_assert(TestResult, clm_string_from_bytes(90003, CLM_STRING_ENCODING_LATIN1, @Bytes[0], 4) = CLM_BPA_SUCCESS, 'decode Latin-1 bytes');
  clm_test_assert(TestResult, clm_string_length(90003) = 4, 'decoded text stores Unicode');
  clm_test_assert(TestResult, clm_string_set_default_encoding(CLM_STRING_ENCODING_UTF8) = CLM_BPA_SUCCESS, 'restore UTF-8 default');

  clm_test_assert(TestResult, clm_string_new(90004, 'pear'), 'create array element');
  clm_test_assert(TestResult, clm_string_new(90005, 'apple'), 'create second array element');
  clm_test_assert(TestResult, clm_string_new(90006, ''), 'create empty destination string');
  clm_test_assert(TestResult, clm_string_array_new(90007, 0), 'create string array');
  clm_test_assert(TestResult, clm_string_array_append(90007, 90004) = CLM_BPA_SUCCESS, 'append first string to array');
  clm_test_assert(TestResult, clm_string_array_append(90007, 90005) = CLM_BPA_SUCCESS, 'append second string to array');
  clm_test_assert(TestResult, clm_string_array_count(90007) = 2, 'query string array count');
  clm_test_assert(TestResult, clm_string_array_find(90007, 90005) = 1, 'find string by value');
  clm_test_assert(TestResult, clm_string_array_get(90007, 0, 90006) = CLM_BPA_GET_SUCCESS, 'get string into BPA block');
  TextBlock := clm_bpa_find_block(90006);
  clm_test_assert(TestResult, (TextBlock <> nil) and (UTF8String(PAnsiChar(TextBlock^.data)) = 'pear'), 'array get returns expected text');
  ArrayNode := clm_bpaa_find_array(90007);
  clm_test_assert(TestResult, (ArrayNode <> nil) and (clm_string_array_sort(90007) = CLM_BPA_SUCCESS), 'sort string array');
  clm_test_assert(TestResult, clm_string_array_delete(90007), 'delete string array');

  clm_string_delete(90001);
  clm_string_delete(90002);
  clm_string_delete(90003);
  clm_string_delete(90004);
  clm_string_delete(90005);
  clm_string_delete(90006);
  clm_test_summary(TestResult.passed, TestResult.failed);
  Result := TestResult.failed;
end;

end.