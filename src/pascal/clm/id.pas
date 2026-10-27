unit id;

{$mode objfpc}{$H+}

interface

uses
  base, bpa;

const
  ID_STORAGE_ID: clm_id = $0F000001;
  CLM_ID_INDEX_STORAGE_ID: clm_id = $0F000002;
  CLM_ID_NAME_BUFFER_ID: clm_id = $0F000003;
  CLM_ID_NAMES_STORAGE_ID: clm_id = $0F000004;
  CLM_ID_VALUES_STORAGE_ID: clm_id = $0F000005;
  CLM_ID_NOT_FOUND: clm_id = High(clm_id);

function clm_id_register(const Name: UTF8String; Value: clm_id): LongInt;
function clm_id_map(const Name: UTF8String; Value: clm_id): LongInt;
function clm_id_get(const Name: UTF8String): clm_id;
function clm_id_try_get(const Name: UTF8String; out Value: clm_id): LongInt;
function clm_id_set(const Name: UTF8String; Value: clm_id): LongInt;
function clm_id_remove(const Name: UTF8String): LongInt;
function clm_id_unmap(Value: clm_id): LongInt;
function clm_id_unmap_name(const Name: UTF8String): LongInt;
function clm_id_set_case_sensitive(Enabled: Boolean): LongInt;
function clm_id_is_case_sensitive: Boolean;
function clm_id_clear: LongInt;
function clm_id_shutdown: LongInt;
procedure clm_id_gen_reset;
function clm_id_gen_next: clm_id;

implementation

uses
  SysUtils;

type
  TIdEntry = record
    Name: UTF8String;
    Value: clm_id;
  end;

var
  Entries: array of TIdEntry;
  CaseSensitive: Boolean = True;
  GeneratorValue: clm_id = 0;
  GeneratorExhausted: Boolean = False;

function ValidUtf8(const Value: UTF8String): Boolean;
var I, Need: SizeInt; First, Second: Byte;
begin
  I := 1;
  while I <= Length(Value) do
  begin
    First := Ord(Value[I]);
    if First <= $7F then begin Inc(I); Continue; end
    else if (First >= $C2) and (First <= $DF) then Need := 2
    else if (First >= $E0) and (First <= $EF) then Need := 3
    else if (First >= $F0) and (First <= $F4) then Need := 4
    else Exit(False);
    if I + Need - 1 > Length(Value) then Exit(False);
    Second := Ord(Value[I + 1]);
    if (Second and $C0) <> $80 then Exit(False);
    if ((First = $E0) and (Second < $A0)) or ((First = $ED) and (Second >= $A0)) or
       ((First = $F0) and (Second < $90)) or ((First = $F4) and (Second >= $90)) then Exit(False);
    while Need > 1 do
    begin
      if (Ord(Value[I + Need - 1]) and $C0) <> $80 then Exit(False);
      Dec(Need);
    end;
    Inc(I, 3);
    if First >= $F0 then Inc(I);
  end;
  Result := True;
end;

function FoldAscii(const Value: UTF8String): UTF8String;
var I: SizeInt; C: Byte;
begin
  Result := Value;
  for I := 1 to Length(Result) do
  begin
    C := Ord(Result[I]);
    if (C >= Ord('A')) and (C <= Ord('Z')) then Result[I] := AnsiChar(C + 32);
  end;
end;

function EffectiveName(const Value: UTF8String): UTF8String;
begin
  if CaseSensitive then Result := Value else Result := FoldAscii(Value);
end;

function FindName(const Value: UTF8String): SizeInt;
var I: SizeInt; Effective: UTF8String;
begin
  Effective := EffectiveName(Value);
  for I := 0 to High(Entries) do
    if EffectiveName(Entries[I].Name) = Effective then Exit(I);
  Result := -1;
end;

function ValidName(const Name: UTF8String): Boolean;
begin Result := (Name <> '') and ValidUtf8(Name); end;

function clm_id_register(const Name: UTF8String; Value: clm_id): LongInt;
var I: SizeInt;
begin
  if not ValidName(Name) or (FindName(Name) >= 0) then Exit(CLM_BPA_FAILURE);
  I := Length(Entries); SetLength(Entries, I + 1); Entries[I].Name := Name; Entries[I].Value := Value;
  Result := CLM_BPA_SET_SUCCESS;
end;

function clm_id_map(const Name: UTF8String; Value: clm_id): LongInt;
begin Result := clm_id_register(Name, Value); end;

function clm_id_get(const Name: UTF8String): clm_id;
var I: SizeInt;
begin
  if not ValidName(Name) then Exit(CLM_ID_NOT_FOUND);
  I := FindName(Name);
  if I < 0 then Result := CLM_ID_NOT_FOUND else Result := Entries[I].Value;
end;

function clm_id_try_get(const Name: UTF8String; out Value: clm_id): LongInt;
var I: SizeInt;
begin
  if not ValidName(Name) then Exit(CLM_BPA_FAILURE);
  I := FindName(Name);
  if I < 0 then Exit(CLM_BPA_FAILURE);
  Value := Entries[I].Value;
  Result := CLM_BPA_GET_SUCCESS;
end;

function clm_id_set(const Name: UTF8String; Value: clm_id): LongInt;
var I: SizeInt;
begin
  if not ValidName(Name) then Exit(CLM_BPA_FAILURE);
  I := FindName(Name);
  if I < 0 then Exit(clm_id_register(Name, Value));
  Entries[I].Value := Value;
  Result := CLM_BPA_SET_SUCCESS;
end;

function clm_id_remove(const Name: UTF8String): LongInt;
var I, Last: SizeInt;
begin
  if not ValidName(Name) then Exit(CLM_BPA_FAILURE);
  I := FindName(Name);
  if I < 0 then Exit(CLM_BPA_FAILURE);
  Last := High(Entries);
  if I <> Last then Entries[I] := Entries[Last];
  SetLength(Entries, Last);
  Result := CLM_BPA_SUCCESS;
end;

function clm_id_unmap(Value: clm_id): LongInt;
var I: SizeInt; Removed: Boolean;
begin
  Removed := False;
  I := High(Entries);
  while I >= 0 do
  begin
    if Entries[I].Value = Value then
    begin
      Entries[I] := Entries[High(Entries)];
      SetLength(Entries, Length(Entries) - 1);
      Removed := True;
    end;
    Dec(I);
  end;
  if Removed then Result := CLM_BPA_SUCCESS else Result := CLM_BPA_FAILURE;
end;

function clm_id_unmap_name(const Name: UTF8String): LongInt;
begin Result := clm_id_remove(Name); end;

function clm_id_set_case_sensitive(Enabled: Boolean): LongInt;
var OldValue: Boolean; I, J: SizeInt;
begin
  if Enabled = CaseSensitive then Exit(CLM_BPA_SUCCESS);
  OldValue := CaseSensitive;
  CaseSensitive := Enabled;
  for I := 0 to High(Entries) do
    for J := I + 1 to High(Entries) do
      if EffectiveName(Entries[I].Name) = EffectiveName(Entries[J].Name) then
      begin
        CaseSensitive := OldValue;
        Exit(CLM_BPA_FAILURE);
      end;
  Result := CLM_BPA_SUCCESS;
end;

function clm_id_is_case_sensitive: Boolean;
begin Result := CaseSensitive; end;

function clm_id_clear: LongInt;
begin SetLength(Entries, 0); Result := CLM_BPA_SUCCESS; end;

function clm_id_shutdown: LongInt;
begin SetLength(Entries, 0); Result := CLM_BPA_SUCCESS; end;

procedure clm_id_gen_reset;
begin GeneratorValue := 0; GeneratorExhausted := False; end;

function clm_id_gen_next: clm_id;
begin
  if GeneratorExhausted then Exit(CLM_ID_NOT_FOUND);
  Result := GeneratorValue;
  if GeneratorValue = High(clm_id) - 1 then GeneratorExhausted := True
  else Inc(GeneratorValue);
end;

end.