unit test;

{$mode objfpc}{$H+}

interface

uses
  console;

type
  clm_test_result = record
    passed: LongInt;
    failed: LongInt;
  end;

const
  DIVIDER_SINGLE = '-------------------------------------------------------------';
  DIVIDER_DOUBLE = '=============================================================';

function clm_test_log_open(const Path: UTF8String = 'main.log'): Boolean;
function clm_test_log_close: Boolean;
procedure clm_test_write(const Text: UTF8String);
procedure clm_test_write_line(const Text: UTF8String);
procedure clm_test_write_color(const Text, Color: UTF8String);
procedure clm_test_write_format(const FormatString: UTF8String; const Args: array of const);
procedure clm_test_write_line_format(const FormatString: UTF8String; const Args: array of const);
procedure clm_test_begin(const Title: UTF8String);
procedure clm_test_assert(var TestResult: clm_test_result; Condition: Boolean; const Text: UTF8String);
procedure clm_test_summary(Passed, Failed: LongInt);
procedure clm_overall_test_summary(const Title: UTF8String);

implementation

uses
  Classes, SysUtils;

var
  clm_tests_passed: LongInt = 0;
  clm_tests_failed: LongInt = 0;
  clm_test_log_file: TFileStream = nil;
  clm_test_log_failed: Boolean = False;

procedure CheckLogStatus(Status: clm_console_status);
begin
  if (clm_test_log_file <> nil) and (Status <> CLM_CONSOLE_SUCCESS) then
    clm_test_log_failed := True;
end;

function clm_test_log_open(const Path: UTF8String): Boolean;
begin
  if (Path = '') or (clm_test_log_file <> nil) then
    Exit(False);
  try
    clm_test_log_file := TFileStream.Create(Path, fmCreate);
    clm_test_log_failed := False;
    Result := True;
  except
    clm_test_log_file := nil;
    Result := False;
  end;
end;

function clm_test_log_close: Boolean;
var
  LogFile: TFileStream;
begin
  if clm_test_log_file = nil then
    Exit(False);
  Result := not clm_test_log_failed;
  LogFile := clm_test_log_file;
  clm_test_log_file := nil;
  try
    LogFile.Free;
  except
    Result := False;
  end;
  Result := Result and not clm_test_log_failed;
end;

procedure clm_test_write(const Text: UTF8String);
begin
  clm_console_write(Text);
  if clm_test_log_file <> nil then
    CheckLogStatus(clm_console_write_to(clm_test_log_file, Text));
end;

procedure clm_test_write_line(const Text: UTF8String);
begin
  clm_console_write_line(Text);
  if clm_test_log_file <> nil then
    CheckLogStatus(clm_console_write_line_to(clm_test_log_file, Text));
end;

procedure clm_test_write_color(const Text, Color: UTF8String);
begin
  clm_console_write_color(Text, Color);
  if clm_test_log_file <> nil then
    CheckLogStatus(clm_console_write_to(clm_test_log_file, Text));
end;

procedure clm_test_write_format(const FormatString: UTF8String; const Args: array of const);
begin
  clm_console_write_format(FormatString, Args);
  if clm_test_log_file <> nil then
    CheckLogStatus(clm_console_write_format_to(clm_test_log_file, FormatString, Args));
end;

procedure clm_test_write_line_format(const FormatString: UTF8String; const Args: array of const);
begin
  clm_console_write_line_format(FormatString, Args);
  if clm_test_log_file <> nil then
    CheckLogStatus(clm_console_write_line_format_to(clm_test_log_file, FormatString, Args));
end;

procedure clm_test_begin(const Title: UTF8String);
begin
  clm_test_write_line('');
  clm_test_write_line(DIVIDER_DOUBLE);
  clm_test_write_color(Title, CONSOLE_COLOR_YELLOW);
  clm_test_write_line('');
  clm_test_write_line(DIVIDER_DOUBLE);
end;

procedure clm_test_assert(var TestResult: clm_test_result; Condition: Boolean; const Text: UTF8String);
begin
  clm_test_write('    ');
  if Condition then
  begin
    Inc(TestResult.passed);
    clm_test_write_color('PASSED:', CONSOLE_COLOR_GREEN);
  end
  else
  begin
    Inc(TestResult.failed);
    clm_test_write_color('FAILED:', CONSOLE_COLOR_RED);
  end;
  clm_test_write(' ');
  clm_test_write_line(Text);
end;

procedure clm_test_summary(Passed, Failed: LongInt);
begin
  Inc(clm_tests_passed, Passed);
  Inc(clm_tests_failed, Failed);

  clm_test_write_line(DIVIDER_SINGLE);
  clm_test_write('    ');
  clm_test_write_color('PASSED:', CONSOLE_COLOR_GREEN);
  clm_test_write_format(' %d  |  ', [Passed]);
  clm_test_write_color('FAILED:', CONSOLE_COLOR_RED);
  clm_test_write_format(' %d  |  ', [Failed]);
  clm_test_write_color('TOTAL:', CONSOLE_COLOR_CYAN);
  clm_test_write_line_format(' %d', [Passed + Failed]);
end;

procedure clm_overall_test_summary(const Title: UTF8String);
begin
  clm_test_write_line('');
  clm_test_write_line(DIVIDER_DOUBLE);
  clm_test_write_color(Title, CONSOLE_COLOR_YELLOW);
  clm_test_write_line('');
  clm_test_write_line(DIVIDER_DOUBLE);
  clm_test_write('  ');
  clm_test_write_color('PASSED:', CONSOLE_COLOR_GREEN);
  clm_test_write_format(' %d  |  ', [clm_tests_passed]);
  clm_test_write_color('FAILED:', CONSOLE_COLOR_RED);
  clm_test_write_format(' %d  |  ', [clm_tests_failed]);
  clm_test_write_color('TOTAL:', CONSOLE_COLOR_CYAN);
  clm_test_write_line_format(' %d', [clm_tests_passed + clm_tests_failed]);
  clm_test_write_line(DIVIDER_DOUBLE);
  clm_test_write_line('');
end;

end.