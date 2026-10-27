unit console;

{$mode objfpc}{$H+}

interface

uses
  Classes, base;

const
  CLM_CONSOLE_SUCCESS = 0;
  CLM_CONSOLE_FAILURE = -1;
  CLM_CONSOLE_INVALID_ARGUMENT = -2;
  CLM_CONSOLE_INPUT_CAPACITY = 4096;

  CONSOLE_COLOR_RESET = #27'[0m';
  CONSOLE_COLOR_BLACK = #27'[30m';
  CONSOLE_COLOR_RED = #27'[31m';
  CONSOLE_COLOR_GREEN = #27'[32m';
  CONSOLE_COLOR_YELLOW = #27'[33m';
  CONSOLE_COLOR_BLUE = #27'[34m';
  CONSOLE_COLOR_MAGENTA = #27'[35m';
  CONSOLE_COLOR_CYAN = #27'[36m';
  CONSOLE_COLOR_WHITE = #27'[37m';
  CONSOLE_COLOR_BLACK_BOLD = #27'[1;30m';
  CONSOLE_COLOR_RED_BOLD = #27'[1;31m';
  CONSOLE_COLOR_GREEN_BOLD = #27'[1;32m';
  CONSOLE_COLOR_YELLOW_BOLD = #27'[1;33m';
  CONSOLE_COLOR_BLUE_BOLD = #27'[1;34m';
  CONSOLE_COLOR_MAGENTA_BOLD = #27'[1;35m';
  CONSOLE_COLOR_CYAN_BOLD = #27'[1;36m';
  CONSOLE_COLOR_WHITE_BOLD = #27'[1;37m';
  CONSOLE_COLOR_BLACK_BG = #27'[40m';
  CONSOLE_COLOR_RED_BG = #27'[41m';
  CONSOLE_COLOR_GREEN_BG = #27'[42m';
  CONSOLE_COLOR_YELLOW_BG = #27'[43m';
  CONSOLE_COLOR_BLUE_BG = #27'[44m';
  CONSOLE_COLOR_MAGENTA_BG = #27'[45m';
  CONSOLE_COLOR_CYAN_BG = #27'[46m';
  CONSOLE_COLOR_WHITE_BG = #27'[47m';

type
  clm_console_status = LongInt;
  clm_console_text = UTF8String;

function clm_console_write_to(Stream: TStream; const Text: clm_console_text): clm_console_status;
function clm_console_init: clm_console_status;
function clm_console_restore: clm_console_status;
function clm_console_clear_to(Stream: TStream): clm_console_status;
function clm_console_clear: clm_console_status;
function clm_console_flush: clm_console_status;
function clm_console_set_cursor_position_to(Stream: TStream; const Position: clm_position): clm_console_status;
function clm_console_set_cursor_position(const Position: clm_position): clm_console_status;
function clm_console_set_cursor_visible_to(Stream: TStream; Visible: Boolean): clm_console_status;
function clm_console_show_cursor: clm_console_status;
function clm_console_hide_cursor: clm_console_status;
function clm_console_write(const Text: clm_console_text): clm_console_status;
function clm_console_vwrite_format_to(Stream: TStream; const FormatString: clm_console_text; const Args: array of const): clm_console_status;
function clm_console_vwrite_line_format_to(Stream: TStream; const FormatString: clm_console_text; const Args: array of const): clm_console_status;
function clm_console_write_format_to(Stream: TStream; const FormatString: clm_console_text; const Args: array of const): clm_console_status;
function clm_console_write_line_format_to(Stream: TStream; const FormatString: clm_console_text; const Args: array of const): clm_console_status;
function clm_console_write_format(const FormatString: clm_console_text; const Args: array of const): clm_console_status;
function clm_console_write_line_format(const FormatString: clm_console_text; const Args: array of const): clm_console_status;
function clm_console_write_line_to(Stream: TStream; const Text: clm_console_text): clm_console_status;
function clm_console_write_line(const Text: clm_console_text): clm_console_status;
function clm_console_read_line_from(Stream: TStream; Buffer: PAnsiChar; Capacity: SizeUInt): clm_console_status;
function clm_console_read_line(out Value: clm_console_text): clm_console_status;
function clm_console_read(const Prompt: clm_console_text; out Value: clm_console_text): clm_console_status;
function clm_console_read_character_from(Stream: TStream; out Character: AnsiChar): clm_console_status;
function clm_console_read_character(out Value: clm_console_text): clm_console_status;
function clm_console_read_key(out Value: clm_console_text): clm_console_status;
function clm_console_write_character_to(Stream: TStream; const Character: clm_console_text): clm_console_status;
function clm_console_write_character(const Character: clm_console_text): clm_console_status;
function clm_console_confirm_from(InputStream, OutputStream: TStream; const Prompt: clm_console_text; var Value: Boolean): clm_console_status;
function clm_console_confirm(const Prompt: clm_console_text; var Value: Boolean): clm_console_status;
function clm_console_write_color_to(Stream: TStream; const Text, Color: clm_console_text): clm_console_status;
function clm_console_write_color(const Text, Color: clm_console_text): clm_console_status;
function clm_console_wait(Seconds: Double): clm_console_status;
function clm_console_write_effect_to(Stream: TStream; const Text: clm_console_text; DelaySeconds: Double): clm_console_status;
function clm_console_write_effect(const Text: clm_console_text; DelaySeconds: Double): clm_console_status;

implementation

uses
  Math, SysUtils;

type
  TStandardStream = class(THandleStream);

function OpenStandardInput: TStandardStream;
begin
  Result := TStandardStream.Create(StdInputHandle);
end;

function OpenStandardOutput: TStandardStream;
begin
  Result := TStandardStream.Create(StdOutputHandle);
end;

function WriteBytes(Stream: TStream; const Data; Count: LongInt): Boolean;
begin
  try
    Result := (Count = 0) or (Stream.Write(Data, Count) = Count);
  except
    Result := False;
  end;
end;

function clm_console_write_to(Stream: TStream; const Text: clm_console_text): clm_console_status;
begin
  if Stream = nil then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  if Text = '' then
    Exit(CLM_CONSOLE_SUCCESS);
  if WriteBytes(Stream, Pointer(Text)^, Length(Text)) then
    Result := CLM_CONSOLE_SUCCESS
  else
    Result := CLM_CONSOLE_FAILURE;
end;

function clm_console_init: clm_console_status;
begin
  Result := CLM_CONSOLE_SUCCESS;
end;

function clm_console_restore: clm_console_status;
begin
  Result := CLM_CONSOLE_SUCCESS;
end;

function clm_console_clear_to(Stream: TStream): clm_console_status;
begin
  Result := clm_console_write_to(Stream, #27'[2J'#27'[H');
end;

function clm_console_clear: clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_clear_to(Stream);
  finally
    Stream.Free;
  end;
end;

function clm_console_flush: clm_console_status;
begin
  Result := CLM_CONSOLE_SUCCESS;
end;

function clm_console_set_cursor_position_to(Stream: TStream; const Position: clm_position): clm_console_status;
var
  Sequence: clm_console_text;
begin
  if (Stream = nil) or (Position.x < 0) or (Position.y < 0) then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  Sequence := #27'[' + IntToStr(Int64(Position.y) + 1) + ';' + IntToStr(Int64(Position.x) + 1) + 'H';
  Result := clm_console_write_to(Stream, Sequence);
end;

function clm_console_set_cursor_position(const Position: clm_position): clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_set_cursor_position_to(Stream, Position);
  finally
    Stream.Free;
  end;
end;

function clm_console_set_cursor_visible_to(Stream: TStream; Visible: Boolean): clm_console_status;
begin
  if Visible then
    Result := clm_console_write_to(Stream, #27'[?25h')
  else
    Result := clm_console_write_to(Stream, #27'[?25l');
end;

function clm_console_show_cursor: clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_set_cursor_visible_to(Stream, True);
  finally
    Stream.Free;
  end;
end;

function clm_console_hide_cursor: clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_set_cursor_visible_to(Stream, False);
  finally
    Stream.Free;
  end;
end;

function clm_console_write(const Text: clm_console_text): clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_write_to(Stream, Text);
  finally
    Stream.Free;
  end;
end;

function clm_console_vwrite_format_to(Stream: TStream; const FormatString: clm_console_text; const Args: array of const): clm_console_status;
var
  Formatted: clm_console_text;
begin
  if Stream = nil then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  try
    Formatted := SysUtils.Format(FormatString, Args);
  except
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  end;
  Result := clm_console_write_to(Stream, Formatted);
end;

function clm_console_write_format_to(Stream: TStream; const FormatString: clm_console_text; const Args: array of const): clm_console_status;
begin
  Result := clm_console_vwrite_format_to(Stream, FormatString, Args);
end;

function clm_console_vwrite_line_format_to(Stream: TStream; const FormatString: clm_console_text; const Args: array of const): clm_console_status;
begin
  Result := clm_console_vwrite_format_to(Stream, FormatString, Args);
  if Result = CLM_CONSOLE_SUCCESS then
    Result := clm_console_write_to(Stream, #10);
end;

function clm_console_write_line_format_to(Stream: TStream; const FormatString: clm_console_text; const Args: array of const): clm_console_status;
begin
  Result := clm_console_vwrite_line_format_to(Stream, FormatString, Args);
end;

function clm_console_write_format(const FormatString: clm_console_text; const Args: array of const): clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_vwrite_format_to(Stream, FormatString, Args);
  finally
    Stream.Free;
  end;
end;

function clm_console_write_line_format(const FormatString: clm_console_text; const Args: array of const): clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_vwrite_line_format_to(Stream, FormatString, Args);
  finally
    Stream.Free;
  end;
end;

function clm_console_write_line_to(Stream: TStream; const Text: clm_console_text): clm_console_status;
begin
  Result := clm_console_write_to(Stream, Text);
  if Result = CLM_CONSOLE_SUCCESS then
    Result := clm_console_write_to(Stream, #10);
end;

function clm_console_write_line(const Text: clm_console_text): clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_write_line_to(Stream, Text);
  finally
    Stream.Free;
  end;
end;

function clm_console_read_line_from(Stream: TStream; Buffer: PAnsiChar; Capacity: SizeUInt): clm_console_status;
var
  Current: Byte;
  Count: LongInt;
  LengthRead: SizeUInt;
  SawInput, SawNewline, TooLong: Boolean;
begin
  if (Stream = nil) or (Buffer = nil) or (Capacity < 2) or (Capacity > High(LongInt)) then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  LengthRead := 0;
  SawInput := False;
  SawNewline := False;
  TooLong := False;
  repeat
    try
      Count := Stream.Read(Current, 1);
    except
      Buffer[0] := #0;
      Exit(CLM_CONSOLE_FAILURE);
    end;
    if Count = 0 then
      Break;
    SawInput := True;
    if Current = 10 then
    begin
      SawNewline := True;
      Break;
    end;
    if LengthRead < Capacity - 1 then
    begin
      Buffer[LengthRead] := AnsiChar(Current);
      Inc(LengthRead);
    end
    else
      TooLong := True;
  until False;
  if TooLong then
  begin
    if not SawNewline then
      repeat
        try
          Count := Stream.Read(Current, 1);
        except
          Break;
        end;
        if (Count = 0) or (Current = 10) then
          Break;
      until False;
    Buffer[0] := #0;
    Exit(CLM_CONSOLE_FAILURE);
  end;
  if not SawInput then
  begin
    Buffer[0] := #0;
    Exit(CLM_CONSOLE_FAILURE);
  end;
  if (LengthRead > 0) and (Buffer[LengthRead - 1] = #13) then
    Dec(LengthRead);
  Buffer[LengthRead] := #0;
  Result := CLM_CONSOLE_SUCCESS;
end;

function clm_console_read_line(out Value: clm_console_text): clm_console_status;
var
  Stream: TStandardStream;
  Buffer: array[0..CLM_CONSOLE_INPUT_CAPACITY - 1] of AnsiChar;
begin
  Value := '';
  Stream := OpenStandardInput;
  try
    Result := clm_console_read_line_from(Stream, @Buffer[0], SizeOf(Buffer));
    if Result = CLM_CONSOLE_SUCCESS then
      Value := UTF8String(PAnsiChar(@Buffer[0]));
  finally
    Stream.Free;
  end;
end;

function clm_console_read(const Prompt: clm_console_text; out Value: clm_console_text): clm_console_status;
begin
  if Prompt <> '' then
  begin
    Result := clm_console_write(Prompt);
    if Result <> CLM_CONSOLE_SUCCESS then
      Exit;
  end;
  Result := clm_console_read_line(Value);
end;

function clm_console_read_character_from(Stream: TStream; out Character: AnsiChar): clm_console_status;
var
  Value: Byte;
  Count: LongInt;
begin
  if Stream = nil then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  try
    Count := Stream.Read(Value, 1);
  except
    Exit(CLM_CONSOLE_FAILURE);
  end;
  if Count <> 1 then
    Exit(CLM_CONSOLE_FAILURE);
  Character := AnsiChar(Value);
  Result := CLM_CONSOLE_SUCCESS;
end;

function clm_console_read_character(out Value: clm_console_text): clm_console_status;
var
  Stream: TStandardStream;
  Character: AnsiChar;
begin
  Value := '';
  Stream := OpenStandardInput;
  try
    Result := clm_console_read_character_from(Stream, Character);
    if Result = CLM_CONSOLE_SUCCESS then
    begin
      SetLength(Value, 1);
      Value[1] := Character;
    end;
  finally
    Stream.Free;
  end;
end;

function clm_console_read_key(out Value: clm_console_text): clm_console_status;
begin
  Result := clm_console_read_character(Value);
end;

function clm_console_write_character_to(Stream: TStream; const Character: clm_console_text): clm_console_status;
var
  First, Second: Byte;
  ByteLength: SizeInt;
  Index: SizeInt;
begin
  if (Stream = nil) or (Character = '') then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  ByteLength := Length(Character);
  First := Ord(Character[1]);
  if First <= $7F then
    ByteLength := 1
  else if (First >= $C2) and (First <= $DF) then
    ByteLength := 2
  else if (First >= $E0) and (First <= $EF) then
    ByteLength := 3
  else if (First >= $F0) and (First <= $F4) then
    ByteLength := 4
  else
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  if Length(Character) <> ByteLength then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  if ByteLength > 1 then
    for Index := 2 to ByteLength do
      if (Ord(Character[Index]) and $C0) <> $80 then
        Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  if ByteLength >= 3 then
  begin
    Second := Ord(Character[2]);
    if ((First = $E0) and (Second < $A0)) or
       ((First = $ED) and (Second >= $A0)) or
       ((First = $F0) and (Second < $90)) or
       ((First = $F4) and (Second >= $90)) then
      Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  end;
  Result := clm_console_write_to(Stream, Character);
end;

function clm_console_write_character(const Character: clm_console_text): clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_write_character_to(Stream, Character);
  finally
    Stream.Free;
  end;
end;

function clm_console_confirm_from(InputStream, OutputStream: TStream; const Prompt: clm_console_text; var Value: Boolean): clm_console_status;
var
  Buffer: array[0..CLM_CONSOLE_INPUT_CAPACITY - 1] of AnsiChar;
  Response: clm_console_text;
begin
  if (InputStream = nil) or (OutputStream = nil) then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  if Prompt <> '' then
  begin
    Result := clm_console_write_to(OutputStream, Prompt);
    if Result <> CLM_CONSOLE_SUCCESS then
      Exit;
  end;
  Result := clm_console_write_to(OutputStream, ' [y/n]: ');
  if Result <> CLM_CONSOLE_SUCCESS then
    Exit;
  Result := clm_console_read_line_from(InputStream, @Buffer[0], SizeOf(Buffer));
  if Result <> CLM_CONSOLE_SUCCESS then
    Exit;
  Response := AnsiLowerCase(Trim(UTF8String(PAnsiChar(@Buffer[0]))));
  if (Response = 'y') or (Response = 'yes') then
  begin
    Value := True;
    Exit(CLM_CONSOLE_SUCCESS);
  end;
  if (Response = 'n') or (Response = 'no') then
  begin
    Value := False;
    Exit(CLM_CONSOLE_SUCCESS);
  end;
  Result := CLM_CONSOLE_INVALID_ARGUMENT;
end;

function clm_console_confirm(const Prompt: clm_console_text; var Value: Boolean): clm_console_status;
var
  InputStream, OutputStream: TStandardStream;
begin
  InputStream := OpenStandardInput;
  OutputStream := OpenStandardOutput;
  try
    Result := clm_console_confirm_from(InputStream, OutputStream, Prompt, Value);
  finally
    OutputStream.Free;
    InputStream.Free;
  end;
end;

function clm_console_write_color_to(Stream: TStream; const Text, Color: clm_console_text): clm_console_status;
begin
  if Stream = nil then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  Result := clm_console_write_to(Stream, Color);
  if Result = CLM_CONSOLE_SUCCESS then
    Result := clm_console_write_to(Stream, Text);
  if Result = CLM_CONSOLE_SUCCESS then
    Result := clm_console_write_to(Stream, CONSOLE_COLOR_RESET);
end;

function clm_console_write_color(const Text, Color: clm_console_text): clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_write_color_to(Stream, Text, Color);
  finally
    Stream.Free;
  end;
end;

function clm_console_wait(Seconds: Double): clm_console_status;
var
  StartTick, EndTick: QWord;
begin
  if IsNan(Seconds) or IsInfinite(Seconds) or (Seconds < 0) then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  if Seconds = 0 then
    Exit(CLM_CONSOLE_SUCCESS);
  if Seconds > (High(QWord) / 1000.0) then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  StartTick := GetTickCount64;
  EndTick := StartTick + Trunc(Seconds * 1000.0);
  while GetTickCount64 < EndTick do
    ;
  Result := CLM_CONSOLE_SUCCESS;
end;

function clm_console_write_effect_to(Stream: TStream; const Text: clm_console_text; DelaySeconds: Double): clm_console_status;
var
  Index: SizeInt;
  Current: AnsiChar;
begin
  if (Stream = nil) or IsNan(DelaySeconds) or IsInfinite(DelaySeconds) or (DelaySeconds < 0) then
    Exit(CLM_CONSOLE_INVALID_ARGUMENT);
  for Index := 1 to Length(Text) do
  begin
    Current := Text[Index];
    if not WriteBytes(Stream, Current, 1) then
      Exit(CLM_CONSOLE_FAILURE);
    if (Index < Length(Text)) and (clm_console_wait(DelaySeconds) <> CLM_CONSOLE_SUCCESS) then
      Exit(CLM_CONSOLE_FAILURE);
  end;
  Result := CLM_CONSOLE_SUCCESS;
end;

function clm_console_write_effect(const Text: clm_console_text; DelaySeconds: Double): clm_console_status;
var
  Stream: TStandardStream;
begin
  Stream := OpenStandardOutput;
  try
    Result := clm_console_write_effect_to(Stream, Text, DelaySeconds);
  finally
    Stream.Free;
  end;
end;

end.