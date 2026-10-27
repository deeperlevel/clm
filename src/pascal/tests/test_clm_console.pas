unit test_clm_console;

{$mode objfpc}{$H+}

interface

function clm_console_tests: LongInt;

implementation

uses
  Classes, Math, SysUtils, base, console, test;

procedure WriteText(Stream: TMemoryStream; const Text: UTF8String);
begin
  if Text <> '' then
    Stream.WriteBuffer(Text[1], Length(Text));
end;

function Utf8Bytes(First, Second: Byte): UTF8String;
var
  Bytes: UTF8String;
begin
  SetLength(Bytes, 2);
  Bytes[1] := AnsiChar(First);
  Bytes[2] := AnsiChar(Second);
  Result := Bytes;
end;

function StreamEquals(Stream: TMemoryStream; const Expected: UTF8String): Boolean;
var
  Actual: UTF8String;
begin
  Result := Stream.Size = Length(Expected);
  if not Result then
    Exit;
  SetLength(Actual, Stream.Size);
  Stream.Position := 0;
  if Actual <> '' then
    Stream.ReadBuffer(Actual[1], Length(Actual));
  Result := Actual = Expected;
end;

function StreamHex(Stream: TMemoryStream): UTF8String;
var
  Index: Int64;
  Value: Byte;
begin
  Result := '';
  Stream.Position := 0;
  for Index := 0 to Stream.Size - 1 do
  begin
    Stream.ReadBuffer(Value, 1);
    Result := Result + IntToHex(Value, 2);
  end;
end;

function clm_console_tests: LongInt;
var
  TestResult: clm_test_result;
  OutputStream: TMemoryStream;
  InputStream: TMemoryStream;
  Position: clm_position;
  Confirmed: Boolean;
  NanValue: Double;
  LineBuffer: array[0..15] of AnsiChar;
  ShortLineBuffer: array[0..7] of AnsiChar;
  Character: AnsiChar;
  Expected: UTF8String;
  OutputMatches: Boolean;
begin
  TestResult.passed := 0;
  TestResult.failed := 0;
  clm_test_begin('CLM - Console');
  OutputStream := TMemoryStream.Create;
  InputStream := TMemoryStream.Create;
  try
    clm_test_assert(TestResult, clm_console_init = CLM_CONSOLE_SUCCESS, 'clm_console_init() == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_restore = CLM_CONSOLE_SUCCESS, 'clm_console_restore() == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_flush = CLM_CONSOLE_SUCCESS, 'clm_console_flush() == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_to(nil, 'text') = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_write_to(nil, text) == CLM_CONSOLE_INVALID_ARGUMENT');
    clm_test_assert(TestResult, clm_console_write_line_to(nil, 'line') = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_write_line_to(nil, line) == CLM_CONSOLE_INVALID_ARGUMENT');
    clm_test_assert(TestResult, clm_console_write_character_to(nil, 'X') = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_write_character_to(nil, X) == CLM_CONSOLE_INVALID_ARGUMENT');
    clm_test_assert(TestResult, clm_console_write_effect_to(nil, 'go', 0.0) = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_write_effect_to(nil, go, 0.0) == CLM_CONSOLE_INVALID_ARGUMENT');
    clm_test_assert(TestResult, clm_console_wait(-1.0) = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_wait(-1.0) == CLM_CONSOLE_INVALID_ARGUMENT');
    NanValue := NaN;
    clm_test_assert(TestResult, clm_console_wait(NanValue) = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_wait(NaN) == CLM_CONSOLE_INVALID_ARGUMENT');

    clm_test_assert(TestResult, clm_console_clear_to(OutputStream) = CLM_CONSOLE_SUCCESS, 'clm_console_clear_to(output) == CLM_CONSOLE_SUCCESS');
    Position.x := 2;
    Position.y := 4;
    clm_test_assert(TestResult, clm_console_set_cursor_position_to(OutputStream, Position) = CLM_CONSOLE_SUCCESS, 'clm_console_set_cursor_position_to(output, x=2, y=4) == CLM_CONSOLE_SUCCESS');
    Position.x := -1;
    Position.y := 0;
    clm_test_assert(TestResult, clm_console_set_cursor_position_to(OutputStream, Position) = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_set_cursor_position_to(output, x=-1, y=0) == CLM_CONSOLE_INVALID_ARGUMENT');
    clm_test_assert(TestResult, clm_console_set_cursor_visible_to(OutputStream, False) = CLM_CONSOLE_SUCCESS, 'clm_console_set_cursor_visible_to(output, false) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_set_cursor_visible_to(OutputStream, True) = CLM_CONSOLE_SUCCESS, 'clm_console_set_cursor_visible_to(output, true) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_to(OutputStream, 'text') = CLM_CONSOLE_SUCCESS, 'clm_console_write_to(output, text) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_line_to(OutputStream, 'line') = CLM_CONSOLE_SUCCESS, 'clm_console_write_line_to(output, line) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_format_to(OutputStream, ' value=%d %s', [42, 'ok']) = CLM_CONSOLE_SUCCESS, 'clm_console_write_format_to(output, value format) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_line_format_to(OutputStream, 'line:%2.2d', [7]) = CLM_CONSOLE_SUCCESS, 'clm_console_write_line_format_to(output, line format) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_format_to(OutputStream, '', []) = CLM_CONSOLE_SUCCESS, 'clm_console_write_format_to(output, empty format) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_line_format_to(nil, '%d', [1]) = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_write_line_format_to(nil, format) == CLM_CONSOLE_INVALID_ARGUMENT');
    clm_test_assert(TestResult, clm_console_write_character_to(OutputStream, 'X') = CLM_CONSOLE_SUCCESS, 'clm_console_write_character_to(output, X) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_character_to(OutputStream, '') = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_write_character_to(output, empty) == CLM_CONSOLE_INVALID_ARGUMENT');
    clm_test_assert(TestResult, clm_console_write_character_to(OutputStream, 'XY') = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_write_character_to(output, XY) == CLM_CONSOLE_INVALID_ARGUMENT');
    clm_test_assert(TestResult, clm_console_write_character_to(OutputStream, Utf8Bytes($C3, $A9)) = CLM_CONSOLE_SUCCESS, 'clm_console_write_character_to(output, UTF-8 scalar) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_character_to(OutputStream, Utf8Bytes($C0, $AF)) = CLM_CONSOLE_INVALID_ARGUMENT, 'clm_console_write_character_to(output, malformed UTF-8) == CLM_CONSOLE_INVALID_ARGUMENT');
    clm_test_assert(TestResult, clm_console_write_color_to(OutputStream, 'red', CONSOLE_COLOR_RED) = CLM_CONSOLE_SUCCESS, 'clm_console_write_color_to(output, red, color) == CLM_CONSOLE_SUCCESS');
    clm_test_assert(TestResult, clm_console_write_effect_to(OutputStream, 'go', 0.0) = CLM_CONSOLE_SUCCESS, 'clm_console_write_effect_to(output, go, 0.0) == CLM_CONSOLE_SUCCESS');

    Expected := #27'[2J'#27'[H'#27'[5;3H'#27'[?25l'#27'[?25h' +
      'textline'#10' value=42 okline:07'#10'X' + Utf8Bytes($C3, $A9) +
      #27'[31mred'#27'[0mgo';
    OutputMatches := StreamEquals(OutputStream, Expected);
    clm_test_assert(TestResult, OutputMatches, 'output stream contains expected ANSI sequences and formatted text');
    if not OutputMatches then
      clm_test_write_line('actual output bytes: ' + StreamHex(OutputStream));

    WriteText(InputStream, 'hello'#13#10'last');
    InputStream.Position := 0;
    clm_test_assert(TestResult, (clm_console_read_line_from(InputStream, @LineBuffer[0], SizeOf(LineBuffer)) = CLM_CONSOLE_SUCCESS) and (UTF8String(PAnsiChar(@LineBuffer[0])) = 'hello'), 'read_line_from strips CRLF');
    clm_test_assert(TestResult, (clm_console_read_line_from(InputStream, @LineBuffer[0], SizeOf(LineBuffer)) = CLM_CONSOLE_SUCCESS) and (UTF8String(PAnsiChar(@LineBuffer[0])) = 'last'), 'read_line_from accepts final line without LF');
    clm_test_assert(TestResult, clm_console_read_line_from(InputStream, @LineBuffer[0], SizeOf(LineBuffer)) = CLM_CONSOLE_FAILURE, 'read_line_from reports EOF');

    InputStream.Clear;
    WriteText(InputStream, '123456789'#10'ok'#10);
    InputStream.Position := 0;
    clm_test_assert(TestResult, clm_console_read_line_from(InputStream, @ShortLineBuffer[0], SizeOf(ShortLineBuffer)) = CLM_CONSOLE_FAILURE, 'read_line_from rejects and drains overlong line');
    clm_test_assert(TestResult, (clm_console_read_line_from(InputStream, @ShortLineBuffer[0], SizeOf(ShortLineBuffer)) = CLM_CONSOLE_SUCCESS) and (UTF8String(PAnsiChar(@ShortLineBuffer[0])) = 'ok'), 'read_line_from resumes after overlong line');

    InputStream.Clear;
    WriteText(InputStream, 'Q');
    InputStream.Position := 0;
    clm_test_assert(TestResult, (clm_console_read_character_from(InputStream, Character) = CLM_CONSOLE_SUCCESS) and (Character = 'Q'), 'read_character_from reads one byte');

    InputStream.Clear;
    OutputStream.Clear;
    WriteText(InputStream, ' yes '#10);
    InputStream.Position := 0;
    Confirmed := False;
    clm_test_assert(TestResult, (clm_console_confirm_from(InputStream, OutputStream, 'Continue?', Confirmed) = CLM_CONSOLE_SUCCESS) and Confirmed, 'confirm_from accepts trimmed yes');

    InputStream.Clear;
    OutputStream.Clear;
    WriteText(InputStream, 'no'#10);
    InputStream.Position := 0;
    Confirmed := True;
    clm_test_assert(TestResult, (clm_console_confirm_from(InputStream, OutputStream, '', Confirmed) = CLM_CONSOLE_SUCCESS) and not Confirmed, 'confirm_from accepts no');

    InputStream.Clear;
    OutputStream.Clear;
    WriteText(InputStream, 'yesterday'#10);
    InputStream.Position := 0;
    Confirmed := True;
    clm_test_assert(TestResult, (clm_console_confirm_from(InputStream, OutputStream, '', Confirmed) = CLM_CONSOLE_INVALID_ARGUMENT) and Confirmed, 'confirm_from rejects invalid response without changing output');

    clm_test_summary(TestResult.passed, TestResult.failed);
    Result := TestResult.failed;
  finally
    InputStream.Free;
    OutputStream.Free;
  end;
end;

end.