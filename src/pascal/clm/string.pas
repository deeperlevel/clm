unit clm_string;

{$mode objfpc}{$H+}

interface

uses
  base, bpa;

const
  CLM_STRING_ENCODING_UTF8 = 1;
  CLM_STRING_ENCODING_LATIN1 = 2;
  CLM_STRING_ENCODING_UTF16LE = 3;

function clm_string_set_default_encoding(Encoding: clm_byte): LongInt;
function clm_string_get_default_encoding: clm_byte;
function clm_string_new(Id: clm_id; const Text: UTF8String): Boolean;
function clm_string_delete(Id: clm_id): Boolean;
function clm_string_set(Id: clm_id; const Text: UTF8String): clm_int;
function clm_string_append(Id: clm_id; const Suffix: UTF8String): clm_int;
function clm_string_append_line(Id: clm_id; const Suffix: UTF8String): clm_int;
function clm_string_insert(Id: clm_id; Index: clm_int; const Inserted: UTF8String): clm_int;
function clm_string_remove(Id: clm_id; Index, Count: clm_int): clm_int;
function clm_string_contains(Id: clm_id; const Search: UTF8String): Boolean;
function clm_string_length(Id: clm_id): clm_int;
function clm_string_replace(Id: clm_id; const Search, Replacement: UTF8String): clm_int;
function clm_string_get_bytes(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): clm_int;
function clm_string_from_bytes(Id: clm_id; Encoding: clm_byte; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_string_change_encoding(Id: clm_id; Encoding: clm_byte): LongInt;
function clm_string_slice(SourceId: clm_id; Start, Count: clm_int; DestinationId: clm_id): LongInt;
function clm_string_slice_borrow(SourceId: clm_id; Start, Count: clm_int; out Text: PAnsiChar; out ByteLength: SizeUInt): LongInt;
function clm_string_array_new(Id: clm_id; Reserved: clm_int): Boolean;
function clm_string_array_length_at(Id: clm_id; Index: clm_int): clm_int;
function clm_string_array_contains_at(Id: clm_id; Index: clm_int; const Search: UTF8String): Boolean;
function clm_string_array_append_at(Id: clm_id; Index: clm_int; const Suffix: UTF8String): clm_int;
function clm_string_array_append_line_at(Id: clm_id; Index: clm_int; const Suffix: UTF8String): clm_int;
function clm_string_array_insert_at(Id: clm_id; Index, CharacterIndex: clm_int; const Inserted: UTF8String): clm_int;
function clm_string_array_remove_at(Id: clm_id; Index, CharacterIndex, Count: clm_int): clm_int;
function clm_string_array_replace_at(Id: clm_id; Index: clm_int; const Search, Replacement: UTF8String): clm_int;
function clm_string_array_delete(Id: clm_id): Boolean;
function clm_string_array_count(Id: clm_id): clm_int;
function clm_string_array_get(Id: clm_id; Index: clm_int; DestinationStringId: clm_id): LongInt;
function clm_string_array_set(Id: clm_id; Index: clm_int; SourceStringId: clm_id): LongInt;
function clm_string_array_insert(Id: clm_id; Index: clm_int; SourceStringId: clm_id): LongInt;
function clm_string_array_append(Id, SourceStringId: clm_id): LongInt;
function clm_string_array_remove(Id: clm_id; Index: clm_int): LongInt;
function clm_string_array_clear(Id: clm_id): LongInt;
function clm_string_array_find(Id, SourceStringId: clm_id): clm_int;
function clm_string_array_sort(Id: clm_id): LongInt;
function clm_string_array_shuffle(Id: clm_id): LongInt;
function clm_string_array_move_first(Id: clm_id): LongInt;
function clm_string_array_move_previous(Id: clm_id): LongInt;
function clm_string_array_move_next(Id: clm_id): LongInt;
function clm_string_array_move_last(Id: clm_id): LongInt;
function clm_string_array_move_to(Id: clm_id; Index: clm_int): LongInt;
function clm_string_array_get_current(Id, DestinationStringId: clm_id): LongInt;
function clm_string_array_draw_at(Id: clm_id; Index: clm_int; DestinationStringId: clm_id): LongInt;
function clm_string_array_draw(Id, DestinationStringId: clm_id): LongInt;

implementation

uses
  SysUtils;

type
  TByteData = array of Byte;

var
  DefaultEncoding: clm_byte = CLM_STRING_ENCODING_UTF8;

function IsSupported(Encoding: clm_byte): Boolean;
begin Result := (Encoding = CLM_STRING_ENCODING_UTF8) or (Encoding = CLM_STRING_ENCODING_LATIN1) or (Encoding = CLM_STRING_ENCODING_UTF16LE); end;

function Utf8Next(const S: UTF8String; var Offset: SizeInt; out CodePoint: Cardinal): Boolean;
var First, Second, Third, Fourth: Byte; Need: SizeInt;
begin
  Result := False;
  if (Offset < 1) or (Offset > Length(S)) then Exit;
  First := Ord(S[Offset]);
  if First < $80 then begin CodePoint := First; Inc(Offset); Exit(True); end
  else if (First >= $C2) and (First <= $DF) then Need := 2
  else if (First >= $E0) and (First <= $EF) then Need := 3
  else if (First >= $F0) and (First <= $F4) then Need := 4
  else Exit;
  if Offset + Need - 1 > Length(S) then Exit;
  Second := Ord(S[Offset + 1]);
  if (Second and $C0) <> $80 then Exit;
  if Need >= 3 then begin Third := Ord(S[Offset + 2]); if (Third and $C0) <> $80 then Exit; end;
  if Need = 4 then begin Fourth := Ord(S[Offset + 3]); if (Fourth and $C0) <> $80 then Exit; end;
  if ((First = $E0) and (Second < $A0)) or ((First = $ED) and (Second >= $A0)) or
     ((First = $F0) and (Second < $90)) or ((First = $F4) and (Second >= $90)) then Exit;
  case Need of
    2: CodePoint := ((First and $1F) shl 6) or (Second and $3F);
    3: CodePoint := ((First and $0F) shl 12) or ((Second and $3F) shl 6) or (Third and $3F);
    4: CodePoint := ((First and $07) shl 18) or ((Second and $3F) shl 12) or ((Third and $3F) shl 6) or (Fourth and $3F);
  else CodePoint := First;
  end;
  Inc(Offset, Need);
  Result := True;
end;

function ValidText(const S: UTF8String; out Characters: SizeInt): Boolean;
var Offset: SizeInt; CodePoint: Cardinal;
begin
  Offset := 1; Characters := 0;
  while Offset <= Length(S) do begin
    if not Utf8Next(S, Offset, CodePoint) then Exit(False);
    Inc(Characters);
  end;
  Result := True;
end;

function ByteOffset(const S: UTF8String; CharacterIndex: SizeInt): SizeInt;
var Offset, Count: SizeInt; CodePoint: Cardinal;
begin
  if CharacterIndex < 0 then Exit(-1);
  Offset := 1; Count := 0;
  while (Count < CharacterIndex) and (Offset <= Length(S)) do begin
    if not Utf8Next(S, Offset, CodePoint) then Exit(-1);
    Inc(Count);
  end;
  if Count <> CharacterIndex then Exit(-1);
  Result := Offset - 1;
end;

function GetText(Id: clm_id; out Block: pclm_bpa_block_node; out Text: UTF8String; out Count: SizeInt): Boolean;
begin
  Block := clm_bpa_find_block(Id);
  if (Block = nil) or (Block^.kind <> CLM_BPA_TYPE_STRING) then Exit(False);
  if Block^.data = nil then Text := '' else Text := UTF8String(PAnsiChar(Block^.data));
  Result := ValidText(Text, Count);
end;

function SetBpaText(Id: clm_id; const Text: UTF8String): LongInt;
var Empty: AnsiChar;
begin
  if Text = '' then begin Empty := #0; Result := clm_bpa_set_string(Id, @Empty, 1); end
  else Result := clm_bpa_set_string(Id, Pointer(Text), Length(Text) + 1);
end;

function clm_string_set_default_encoding(Encoding: clm_byte): LongInt;
begin if not IsSupported(Encoding) then Exit(CLM_BPA_FAILURE); DefaultEncoding := Encoding; Result := CLM_BPA_SUCCESS; end;
function clm_string_get_default_encoding: clm_byte; begin Result := DefaultEncoding; end;

function clm_string_new(Id: clm_id; const Text: UTF8String): Boolean;
var Count: SizeInt; Block: pclm_bpa_block_node;
begin
  Result := False;
  if not ValidText(Text, Count) or (clm_bpa_create(Id, CLM_BPA_TYPE_STRING) <> CLM_BPA_SUCCESS) then Exit;
  if SetBpaText(Id, Text) < 0 then begin clm_bpa_delete(Id); Exit; end;
  Block := clm_bpa_find_block(Id);
  if Block = nil then begin clm_bpa_delete(Id); Exit; end;
  Block^.encoding := DefaultEncoding;
  Result := True;
end;

function clm_string_delete(Id: clm_id): Boolean;
var Block: pclm_bpa_block_node;
begin Block := clm_bpa_find_block(Id); Result := (Block <> nil) and (Block^.kind = CLM_BPA_TYPE_STRING) and (clm_bpa_delete(Id) = CLM_BPA_SUCCESS); end;

function clm_string_set(Id: clm_id; const Text: UTF8String): clm_int;
var Block: pclm_bpa_block_node; Count, ExistingCount: SizeInt; ExistingText: UTF8String;
begin if not ValidText(Text, Count) or not GetText(Id, Block, ExistingText, ExistingCount) then Exit(CLM_BPA_FAILURE); Result := SetBpaText(Id, Text); if Result >= 0 then Result := Count; end;

function ReplaceText(Id: clm_id; const NewText: UTF8String): Boolean;
begin Result := SetBpaText(Id, NewText) >= 0; end;

function clm_string_append(Id: clm_id; const Suffix: UTF8String): clm_int;
var Block: pclm_bpa_block_node; Text: UTF8String; Count, Added: SizeInt;
begin if not GetText(Id, Block, Text, Count) or not ValidText(Suffix, Added) then Exit(CLM_BPA_FAILURE); Result := Added; if not ReplaceText(Id, Text + Suffix) then Result := CLM_BPA_FAILURE; end;
function clm_string_append_line(Id: clm_id; const Suffix: UTF8String): clm_int;
begin Result := clm_string_append(Id, Suffix); if Result < 0 then Exit; if clm_string_append(Id, #10) < 0 then Exit(CLM_BPA_FAILURE); Inc(Result); end;

function clm_string_insert(Id: clm_id; Index: clm_int; const Inserted: UTF8String): clm_int;
var Block: pclm_bpa_block_node; Text: UTF8String; Count, Added, Offset: SizeInt;
begin
  if (Index < 0) or not GetText(Id, Block, Text, Count) or not ValidText(Inserted, Added) or (Index > Count) then Exit(CLM_BPA_FAILURE);
  Offset := ByteOffset(Text, Index);
  Result := Added;
  if not ReplaceText(Id, Copy(Text, 1, Offset) + Inserted + Copy(Text, Offset + 1, MaxInt)) then Result := CLM_BPA_FAILURE;
end;

function clm_string_remove(Id: clm_id; Index, Count: clm_int): clm_int;
var Block: pclm_bpa_block_node; Text: UTF8String; Chars: SizeInt; StartByte, EndByte: SizeInt;
begin
  if (Index < 0) or (Count < 0) or not GetText(Id, Block, Text, Chars) or (Index > Chars) or (Count > Chars - Index) then Exit(CLM_BPA_FAILURE);
  if Count = 0 then Exit(0);
  StartByte := ByteOffset(Text, Index); EndByte := ByteOffset(Text, Index + Count);
  if not ReplaceText(Id, Copy(Text, 1, StartByte) + Copy(Text, EndByte + 1, MaxInt)) then Exit(CLM_BPA_FAILURE);
  Result := Count;
end;

function clm_string_contains(Id: clm_id; const Search: UTF8String): Boolean;
var Block: pclm_bpa_block_node; Text: UTF8String; Count, SearchCount: SizeInt;
begin Result := GetText(Id, Block, Text, Count) and ValidText(Search, SearchCount) and (Pos(Search, Text) > 0); end;
function clm_string_length(Id: clm_id): clm_int;
var Block: pclm_bpa_block_node; Text: UTF8String; Count: SizeInt;
begin if not GetText(Id, Block, Text, Count) then Exit(CLM_BPA_FAILURE); Result := Count; end;

function ReplaceAllBytes(const Text, Search, Replacement: UTF8String; out MatchCount: SizeInt): UTF8String;
var
  Cursor, MatchPosition, RelativePosition: SizeInt;
begin
  Result := '';
  MatchCount := 0;
  Cursor := 1;
  repeat
    RelativePosition := Pos(Search, Copy(Text, Cursor, MaxInt));
    if RelativePosition = 0 then Break;
    MatchPosition := Cursor + RelativePosition - 1;
    Result := Result + Copy(Text, Cursor, MatchPosition - Cursor) + Replacement;
    Cursor := MatchPosition + Length(Search);
    Inc(MatchCount);
  until Cursor > Length(Text);
  Result := Result + Copy(Text, Cursor, MaxInt);
end;

function clm_string_replace(Id: clm_id; const Search, Replacement: UTF8String): clm_int;
var Block: pclm_bpa_block_node; Text: UTF8String; Count, SearchCount, ReplacementCount: SizeInt; NewText: UTF8String;
begin
  if (Search = '') or not GetText(Id, Block, Text, Count) or not ValidText(Search, SearchCount) or not ValidText(Replacement, ReplacementCount) then Exit(CLM_BPA_FAILURE);
  NewText := ReplaceAllBytes(Text, Search, Replacement, Count);
  Result := Count;
  if not ReplaceText(Id, NewText) then Result := CLM_BPA_FAILURE;
end;

function EncodeText(const Text: UTF8String; Encoding: clm_byte; out Bytes: TByteData): Boolean;
var Offset: SizeInt; CP: Cardinal; N: SizeInt; UnitValue: Word;
begin
  Result := False; SetLength(Bytes, 0);
  if not IsSupported(Encoding) then Exit;
  if Encoding = CLM_STRING_ENCODING_UTF8 then begin
    if not ValidText(Text, N) then Exit;
    SetLength(Bytes, Length(Text));
    if Length(Text) > 0 then Move(Text[1], Bytes[0], Length(Text));
    Exit(True);
  end;
  Offset := 1;
  while Offset <= Length(Text) do begin
    if not Utf8Next(Text, Offset, CP) then Exit;
    if Encoding = CLM_STRING_ENCODING_LATIN1 then begin if CP > 255 then Exit; N := Length(Bytes); SetLength(Bytes,N+1); Bytes[N] := CP; end
    else if Encoding = CLM_STRING_ENCODING_UTF16LE then begin
      if CP < $10000 then begin N := Length(Bytes); SetLength(Bytes,N+2); Bytes[N] := CP and $FF; Bytes[N+1] := CP shr 8; end
      else begin CP := CP - $10000; UnitValue := $D800 or (CP shr 10); N := Length(Bytes); SetLength(Bytes,N+4); Bytes[N] := UnitValue and $FF; Bytes[N+1] := UnitValue shr 8; UnitValue := $DC00 or (CP and $3FF); Bytes[N+2] := UnitValue and $FF; Bytes[N+3] := UnitValue shr 8; end;
    end else Exit;
  end;
  Result := True;
end;

function clm_string_get_bytes(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): clm_int;
var Block: pclm_bpa_block_node; Text: UTF8String; Count: SizeInt; Bytes: TByteData;
begin
  if not GetText(Id, Block, Text, Count) or not EncodeText(Text, Block^.encoding, Bytes) then Exit(CLM_BPA_FAILURE);
  if Buffer = nil then begin if BufferSize = 0 then Result := Length(Bytes) else Result := CLM_BPA_FAILURE; Exit; end;
  if BufferSize < Length(Bytes) then Exit(CLM_BPA_FAILURE);
  if Length(Bytes) > 0 then Move(Bytes[0], Buffer^, Length(Bytes));
  Result := Length(Bytes);
end;

function AppendCodePoint(var Text: UTF8String; CP: Cardinal): Boolean;
begin
  if CP = 0 then Exit(False);
  if CP < $80 then Text := Text + AnsiChar(CP)
  else if CP < $800 then Text := Text + AnsiChar($C0 or (CP shr 6)) + AnsiChar($80 or (CP and $3F))
  else if CP < $10000 then Text := Text + AnsiChar($E0 or (CP shr 12)) + AnsiChar($80 or ((CP shr 6) and $3F)) + AnsiChar($80 or (CP and $3F))
  else Text := Text + AnsiChar($F0 or (CP shr 18)) + AnsiChar($80 or ((CP shr 12) and $3F)) + AnsiChar($80 or ((CP shr 6) and $3F)) + AnsiChar($80 or (CP and $3F));
  Result := True;
end;

function DecodeBytes(Encoding: clm_byte; Buffer: Pointer; BufferSize: clm_uint; out Text: UTF8String): Boolean;
var I: SizeInt; B: PByte; CP, LowSurrogate: Cardinal;
begin
  Text := ''; Result := IsSupported(Encoding) and ((BufferSize = 0) or (Buffer <> nil)); if not Result then Exit;
  B := Buffer; I := 0;
  if Encoding = CLM_STRING_ENCODING_UTF8 then begin
    if BufferSize > 0 then begin SetLength(Text, BufferSize); Move(B^, Text[1], BufferSize); end;
    Result := ValidText(Text, I) and (Pos(#0, Text) = 0);
  end else if Encoding = CLM_STRING_ENCODING_LATIN1 then begin
    while I < BufferSize do begin if B[I] = 0 then Exit(False); AppendCodePoint(Text, B[I]); Inc(I); end;
  end else begin
    if (BufferSize mod 2) <> 0 then Exit(False);
    while I < BufferSize do begin
      CP := B[I] or (Cardinal(B[I+1]) shl 8); Inc(I, 2);
      if (CP >= $D800) and (CP <= $DBFF) then begin
        if I + 1 >= BufferSize then Exit(False);
        LowSurrogate := B[I] or (Cardinal(B[I+1]) shl 8);
        if (LowSurrogate < $DC00) or (LowSurrogate > $DFFF) then Exit(False);
        CP := $10000 + ((CP - $D800) shl 10) + (LowSurrogate - $DC00); Inc(I, 2);
      end else if (CP >= $DC00) and (CP <= $DFFF) then Exit(False);
      if not AppendCodePoint(Text, CP) then Exit(False);
    end;
  end;
end;

function clm_string_from_bytes(Id: clm_id; Encoding: clm_byte; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Text: UTF8String; Block: pclm_bpa_block_node;
begin
  if not DecodeBytes(Encoding, Buffer, BufferSize, Text) then Exit(CLM_BPA_FAILURE);
  Block := clm_bpa_find_block(Id);
  if Block = nil then begin if not clm_string_new(Id, Text) then Exit(CLM_BPA_FAILURE); Block := clm_bpa_find_block(Id); end
  else if (Block^.kind <> CLM_BPA_TYPE_STRING) or (SetBpaText(Id, Text) < 0) then Exit(CLM_BPA_FAILURE);
  Block^.encoding := Encoding; Result := CLM_BPA_SUCCESS;
end;

function clm_string_change_encoding(Id: clm_id; Encoding: clm_byte): LongInt;
var Block: pclm_bpa_block_node; Text: UTF8String; Count: SizeInt; Bytes: TByteData;
begin Block := clm_bpa_find_block(Id); if (Block = nil) or (Block^.kind <> CLM_BPA_TYPE_STRING) or not GetText(Id, Block, Text, Count) or not EncodeText(Text, Encoding, Bytes) then Exit(CLM_BPA_FAILURE); Block^.encoding := Encoding; Result := CLM_BPA_SUCCESS; end;

function clm_string_slice(SourceId: clm_id; Start, Count: clm_int; DestinationId: clm_id): LongInt;
var Block, Dest: pclm_bpa_block_node; Text, Part: UTF8String; Chars, A, B: SizeInt;
begin
  if (Start < 0) or (Count < 0) or not GetText(SourceId, Block, Text, Chars) or (Start > Chars) or (Count > Chars-Start) then Exit(CLM_BPA_FAILURE);
  A := ByteOffset(Text, Start); B := ByteOffset(Text, Start+Count); Part := Copy(Text, A+1, B-A);
  if not clm_string_new(DestinationId, Part) then Exit(CLM_BPA_FAILURE);
  Dest := clm_bpa_find_block(DestinationId); Dest^.encoding := Block^.encoding; Result := CLM_BPA_SUCCESS;
end;

function clm_string_slice_borrow(SourceId: clm_id; Start, Count: clm_int; out Text: PAnsiChar; out ByteLength: SizeUInt): LongInt;
var Block: pclm_bpa_block_node; Value: UTF8String; Chars, A, B: SizeInt;
begin
  Text := nil; ByteLength := 0;
  if (Start < 0) or (Count < 0) or not GetText(SourceId, Block, Value, Chars) or (Start > Chars) or (Count > Chars-Start) then Exit(CLM_BPA_FAILURE);
  A := ByteOffset(Value, Start); B := ByteOffset(Value, Start+Count);
  if Block^.data = nil then Text := PAnsiChar(AnsiString('')) else Text := PAnsiChar(Block^.data) + A;
  ByteLength := B-A; Result := CLM_BPA_SUCCESS;
end;

function ArrayNode(Id: clm_id): pclm_bpaa_array_node;
begin Result := clm_bpaa_find_array(Id); if (Result <> nil) and ((Result^.kind <> CLM_BPA_TYPE_STRING) or (Result^.encoding <> CLM_STRING_ENCODING_UTF8)) then Result := nil; end;
function ArrayText(Node: pclm_bpaa_array_node; Index: clm_int): UTF8String;
begin Result := UTF8String(Node^.strings[Index]); end;
function StoreArrayText(Node: pclm_bpaa_array_node; Index: clm_int; const Text: UTF8String): Boolean;
var Empty: AnsiChar;
begin if Text = '' then begin Empty := #0; Result := clm_bpaa_set_string_raw(Node^.id, Index, @Empty, 1) >= 0; end else Result := clm_bpaa_set_string_raw(Node^.id, Index, Pointer(Text), Length(Text)+1) >= 0; end;

function clm_string_array_new(Id: clm_id; Reserved: clm_int): Boolean;
begin Result := clm_bpaa_create(Id, CLM_BPA_TYPE_STRING, Reserved, CLM_STRING_ENCODING_UTF8) = CLM_BPA_SUCCESS; end;
function clm_string_array_length_at(Id: clm_id; Index: clm_int): clm_int;
var Node: pclm_bpaa_array_node; Count: SizeInt;
begin Node := ArrayNode(Id); if (Node = nil) or (Index < 0) or (clm_uint(Index) >= Node^.count) or not ValidText(ArrayText(Node,Index), Count) then Exit(CLM_BPA_FAILURE); Result := Count; end;
function clm_string_array_contains_at(Id: clm_id; Index: clm_int; const Search: UTF8String): Boolean;
var Node: pclm_bpaa_array_node; Text: UTF8String; Count: SizeInt;
begin Node := ArrayNode(Id); Result := (Node <> nil) and (Index >= 0) and (clm_uint(Index) < Node^.count) and ValidText(Search,Count); if Result then begin Text := ArrayText(Node,Index); Result := Pos(Search,Text) > 0; end; end;
function clm_string_array_append_at(Id: clm_id; Index: clm_int; const Suffix: UTF8String): clm_int;
var Node: pclm_bpaa_array_node; Text: UTF8String; Count, Added: SizeInt;
begin Node := ArrayNode(Id); if (Node=nil) or (Index<0) or (clm_uint(Index)>=Node^.count) or not ValidText(Suffix,Added) then Exit(CLM_BPA_FAILURE); Text:=ArrayText(Node,Index); if not StoreArrayText(Node,Index,Text+Suffix) then Exit(CLM_BPA_FAILURE); Result:=Added; end;
function clm_string_array_append_line_at(Id: clm_id; Index: clm_int; const Suffix: UTF8String): clm_int;
begin Result:=clm_string_array_append_at(Id,Index,Suffix); if Result<0 then Exit; if not StoreArrayText(ArrayNode(Id),Index,ArrayText(ArrayNode(Id),Index)+#10) then Exit(CLM_BPA_FAILURE); Inc(Result); end;
function clm_string_array_insert_at(Id: clm_id; Index, CharacterIndex: clm_int; const Inserted: UTF8String): clm_int;
var Node: pclm_bpaa_array_node; Text: UTF8String; Chars, Added, Offset: SizeInt;
begin Node:=ArrayNode(Id); if (Node=nil) or (Index<0) or (clm_uint(Index)>=Node^.count) or (CharacterIndex<0) then Exit(CLM_BPA_FAILURE); Text:=ArrayText(Node,Index); if not ValidText(Text,Chars) or not ValidText(Inserted,Added) or (CharacterIndex>Chars) then Exit(CLM_BPA_FAILURE); Offset:=ByteOffset(Text,CharacterIndex); if not StoreArrayText(Node,Index,Copy(Text,1,Offset)+Inserted+Copy(Text,Offset+1,MaxInt)) then Exit(CLM_BPA_FAILURE); Result:=Added; end;
function clm_string_array_remove_at(Id: clm_id; Index, CharacterIndex, Count: clm_int): clm_int;
var Node: pclm_bpaa_array_node; Text: UTF8String; Chars, A, B: SizeInt;
begin Node:=ArrayNode(Id); if (Node=nil) or (Index<0) or (clm_uint(Index)>=Node^.count) or (CharacterIndex<0) or (Count<0) then Exit(CLM_BPA_FAILURE); Text:=ArrayText(Node,Index); if not ValidText(Text,Chars) or (CharacterIndex>Chars) or (Count>Chars-CharacterIndex) then Exit(CLM_BPA_FAILURE); if Count=0 then Exit(0); A:=ByteOffset(Text,CharacterIndex); B:=ByteOffset(Text,CharacterIndex+Count); if not StoreArrayText(Node,Index,Copy(Text,1,A)+Copy(Text,B+1,MaxInt)) then Exit(CLM_BPA_FAILURE); Result:=Count; end;
function clm_string_array_replace_at(Id: clm_id; Index: clm_int; const Search, Replacement: UTF8String): clm_int;
var Node:pclm_bpaa_array_node; Text, Updated:UTF8String; Count,A,B:SizeInt;
begin Node:=ArrayNode(Id); if (Node=nil) or (Index<0) or (clm_uint(Index)>=Node^.count) or (Search='') or not ValidText(Search,A) or not ValidText(Replacement,B) then Exit(CLM_BPA_FAILURE); Text:=ArrayText(Node,Index); Updated:=ReplaceAllBytes(Text,Search,Replacement,Count); if not StoreArrayText(Node,Index,Updated) then Exit(CLM_BPA_FAILURE); Result:=Count; end;

function clm_string_array_delete(Id: clm_id): Boolean;
begin Result := (ArrayNode(Id)<>nil) and (clm_bpaa_delete(Id)=CLM_BPA_SUCCESS); end;
function clm_string_array_count(Id: clm_id): clm_int;
var Node: pclm_bpaa_array_node;
begin Node:=ArrayNode(Id); if Node=nil then Exit(CLM_BPA_FAILURE); Result:=Node^.count; end;
function clm_string_array_get(Id: clm_id; Index: clm_int; DestinationStringId: clm_id): LongInt;
var Node: pclm_bpaa_array_node; Text: UTF8String; Dest: pclm_bpa_block_node;
begin Node:=ArrayNode(Id); Dest:=clm_bpa_find_block(DestinationStringId); if (Node=nil) or (Dest=nil) or (Dest^.kind<>CLM_BPA_TYPE_STRING) or (Index<0) or (clm_uint(Index)>=Node^.count) then Exit(CLM_BPA_FAILURE); Text:=ArrayText(Node,Index); Result:=SetBpaText(DestinationStringId,Text); if Result>=0 then begin Dest^.encoding:=Node^.encoding; Result:=CLM_BPA_GET_SUCCESS; end; end;
function clm_string_array_set(Id: clm_id; Index: clm_int; SourceStringId: clm_id): LongInt;
var Node:pclm_bpaa_array_node; Src:pclm_bpa_block_node;
begin Node:=ArrayNode(Id); Src:=clm_bpa_find_block(SourceStringId); if (Node=nil) or (Src=nil) or (Src^.kind<>CLM_BPA_TYPE_STRING) then Exit(CLM_BPA_FAILURE); Result:=clm_bpaa_set_string_raw(Id,Index,Src^.data,Src^.length+1); if Result>=0 then Result:=CLM_BPA_SET_SUCCESS; end;
function clm_string_array_insert(Id: clm_id; Index: clm_int; SourceStringId: clm_id): LongInt;
var Src:pclm_bpa_block_node; begin Src:=clm_bpa_find_block(SourceStringId); if (ArrayNode(Id)=nil) or (Src=nil) or (Src^.kind<>CLM_BPA_TYPE_STRING) then Exit(CLM_BPA_FAILURE); Result:=clm_bpaa_insert_raw(Id,Index,Src^.data,Src^.length+1); end;
function clm_string_array_append(Id, SourceStringId: clm_id): LongInt;
var Node:pclm_bpaa_array_node; begin Node:=ArrayNode(Id); if Node=nil then Exit(CLM_BPA_FAILURE); Result:=clm_string_array_insert(Id,Node^.count,SourceStringId); end;
function clm_string_array_remove(Id: clm_id; Index: clm_int): LongInt; begin if ArrayNode(Id)=nil then Exit(CLM_BPA_FAILURE); Result:=clm_bpaa_remove(Id,Index); end;
function clm_string_array_clear(Id: clm_id): LongInt; begin if ArrayNode(Id)=nil then Exit(CLM_BPA_FAILURE); Result:=clm_bpaa_clear(Id); end;
function clm_string_array_find(Id, SourceStringId: clm_id): clm_int;
var Node:pclm_bpaa_array_node; Src:pclm_bpa_block_node; I:clm_uint;
begin Node:=ArrayNode(Id); Src:=clm_bpa_find_block(SourceStringId); if (Node=nil) or (Src=nil) or (Src^.kind<>CLM_BPA_TYPE_STRING) then Exit(CLM_BPA_FAILURE); for I:=0 to Node^.count-1 do if UTF8String(Node^.strings[I])=UTF8String(PAnsiChar(Src^.data)) then Exit(I); Result:=CLM_BPA_FAILURE; end;
function clm_string_array_sort(Id: clm_id): LongInt;
var Node:pclm_bpaa_array_node; I,J:clm_uint; Temp:PAnsiChar;
begin Node:=ArrayNode(Id); if Node=nil then Exit(CLM_BPA_FAILURE); for I:=0 to Node^.count-2 do for J:=0 to Node^.count-I-2 do if StrComp(Node^.strings[J],Node^.strings[J+1])>0 then begin Temp:=Node^.strings[J]; Node^.strings[J]:=Node^.strings[J+1]; Node^.strings[J+1]:=Temp; end; Node^.index:=0; Node^.deck_size:=Node^.count; Result:=CLM_BPA_SUCCESS; end;
function clm_string_array_shuffle(Id: clm_id): LongInt;
var Node:pclm_bpaa_array_node; I,J:clm_uint; Temp:PAnsiChar;
begin Node:=ArrayNode(Id); if Node=nil then Exit(CLM_BPA_FAILURE); for I:=Node^.count downto 2 do begin J:=Random(I); Temp:=Node^.strings[I-1]; Node^.strings[I-1]:=Node^.strings[J]; Node^.strings[J]:=Temp; end; Node^.index:=0; Node^.deck_size:=Node^.count; Result:=CLM_BPA_SUCCESS; end;
function clm_string_array_move_first(Id: clm_id): LongInt; begin if ArrayNode(Id)=nil then Exit(CLM_BPA_FAILURE); Result:=clm_bpaa_move_first(Id); end;
function clm_string_array_move_previous(Id: clm_id): LongInt; begin if ArrayNode(Id)=nil then Exit(CLM_BPA_FAILURE); Result:=clm_bpaa_move_previous(Id); end;
function clm_string_array_move_next(Id: clm_id): LongInt; begin if ArrayNode(Id)=nil then Exit(CLM_BPA_FAILURE); Result:=clm_bpaa_move_next(Id); end;
function clm_string_array_move_last(Id: clm_id): LongInt; begin if ArrayNode(Id)=nil then Exit(CLM_BPA_FAILURE); Result:=clm_bpaa_move_last(Id); end;
function clm_string_array_move_to(Id: clm_id; Index: clm_int): LongInt; begin if ArrayNode(Id)=nil then Exit(CLM_BPA_FAILURE); Result:=clm_bpaa_move_to(Id,Index); end;
function clm_string_array_get_current(Id, DestinationStringId: clm_id): LongInt;
var Node:pclm_bpaa_array_node;
begin Node:=ArrayNode(Id); if (Node=nil) or (Node^.index>=Node^.count) then Exit(CLM_BPA_FAILURE); Result:=clm_string_array_get(Id,Node^.index,DestinationStringId); end;
function clm_string_array_draw_at(Id: clm_id; Index: clm_int; DestinationStringId: clm_id): LongInt;
var Node:pclm_bpaa_array_node; Last:clm_uint; Temp:PAnsiChar;
begin Node:=ArrayNode(Id); if (Node=nil) or (Index<0) or (clm_uint(Index)>=Node^.deck_size) then Exit(CLM_BPA_FAILURE); Result:=clm_string_array_get(Id,Index,DestinationStringId); if Result<>CLM_BPA_GET_SUCCESS then Exit; Last:=Node^.deck_size-1; Temp:=Node^.strings[Index]; Node^.strings[Index]:=Node^.strings[Last]; Node^.strings[Last]:=Temp; Dec(Node^.deck_size); end;
function clm_string_array_draw(Id, DestinationStringId: clm_id): LongInt;
var Node:pclm_bpaa_array_node;
begin Node:=ArrayNode(Id); if Node=nil then Exit(CLM_BPA_FAILURE); if (Node^.deck_size=0) and (clm_string_array_shuffle(Id)<>CLM_BPA_SUCCESS) then Exit(CLM_BPA_FAILURE); Result:=clm_string_array_draw_at(Id,0,DestinationStringId); end;

initialization
  Randomize;

end.