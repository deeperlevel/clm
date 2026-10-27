unit dictionary;

{$mode objfpc}{$H+}

interface

uses
  base;

type
  Pclm_id = ^clm_id;
  clm_dictionary_metadata = record
    magic: QWord;
    slots_id, values_id, keys_id: clm_id;
    value_size, count, iterator: clm_uint;
    value_type: clm_byte;
    reserved: array[0..2] of clm_byte;
  end;

function clm_dictionary_new(Id: clm_id; ValueType: clm_byte; ValueSize: SizeUInt): LongInt;
function clm_dictionary_delete(Id: clm_id): LongInt;
function clm_dictionary_set_from(Id, Key: clm_id; Value: Pointer; ValueSize: SizeUInt): LongInt;
function clm_dictionary_set(Id, Key, SourceBlockId: clm_id): LongInt;
function clm_dictionary_get_to(Id, Key: clm_id; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
function clm_dictionary_get(Id, Key, DestinationBlockId: clm_id): LongInt;
function clm_dictionary_contains(Id, Key: clm_id): Boolean;
function clm_dictionary_count(Id: clm_id): clm_int;
function clm_dictionary_remove(Id, Key: clm_id): LongInt;
function clm_dictionary_iterate_reset(Id: clm_id): LongInt;
function clm_dictionary_iterate_next(Id: clm_id; out Key: clm_id): LongInt;
function clm_dictionary_clear(Id: clm_id): LongInt;
function clm_dictionary_add_from(Id, Key: clm_id; Value: Pointer; ValueSize: SizeUInt): LongInt;
function clm_dictionary_update_from(Id, Key: clm_id; Value: Pointer; ValueSize: SizeUInt): LongInt;
function clm_dictionary_keys_to(Id: clm_id; Buffer: Pclm_id; Capacity: SizeUInt): LongInt;

implementation

uses
  bpa;

type
  TDictionaryBytes = array of Byte;
  TDictionaryValue = record
    Key: clm_id;
    Bytes: TDictionaryBytes;
  end;
  TDictionary = record
    Id: clm_id;
    ValueType: clm_byte;
    ValueSize: SizeUInt;
    Iterator: SizeInt;
    Values: array of TDictionaryValue;
  end;

var
  Dictionaries: array of TDictionary;

function FindDictionary(Id: clm_id): SizeInt;
var I: SizeInt;
begin
  for I := 0 to High(Dictionaries) do
    if Dictionaries[I].Id = Id then Exit(I);
  Result := -1;
end;

function FindKey(const Dict: TDictionary; Key: clm_id): SizeInt;
var I: SizeInt;
begin
  for I := 0 to High(Dict.Values) do
    if Dict.Values[I].Key = Key then Exit(I);
  Result := -1;
end;

function clm_dictionary_new(Id: clm_id; ValueType: clm_byte; ValueSize: SizeUInt): LongInt;
var ItemSize: clm_uint; I: SizeInt;
begin
  if (FindDictionary(Id) >= 0) or clm_bpa_is_existing(Id) or clm_bpaa_is_existing(Id) then Exit(CLM_BPA_FAILURE);
  ItemSize := clm_bpa_type_size(ValueType);
  if ValueType = CLM_BPA_TYPE_STRUCT then begin
    if (ValueSize = 0) or (ValueSize > High(clm_uint)) then Exit(CLM_BPA_FAILURE);
    ItemSize := ValueSize;
  end else if ValueType = CLM_BPA_TYPE_STRING then begin
    if ValueSize <> 0 then Exit(CLM_BPA_FAILURE);
    ItemSize := 0;
  end else if (ItemSize = 0) or ((ValueSize <> 0) and (ValueSize <> ItemSize)) then Exit(CLM_BPA_FAILURE);
  if clm_bpa_create_sized(Id, CLM_BPA_TYPE_STRUCT, SizeOf(clm_dictionary_metadata)) <> CLM_BPA_SUCCESS then Exit(CLM_BPA_FAILURE);
  I := Length(Dictionaries);
  SetLength(Dictionaries, I + 1);
  Dictionaries[I].Id := Id;
  Dictionaries[I].ValueType := ValueType;
  Dictionaries[I].ValueSize := ItemSize;
  Dictionaries[I].Iterator := 0;
  Result := CLM_BPA_SUCCESS;
end;

function clm_dictionary_delete(Id: clm_id): LongInt;
var I, Last: SizeInt;
begin
  I := FindDictionary(Id);
  if I < 0 then Exit(CLM_BPA_FAILURE);
  Last := High(Dictionaries);
  if I <> Last then Dictionaries[I] := Dictionaries[Last];
  SetLength(Dictionaries, Last);
  Result := clm_bpa_delete(Id);
end;

function clm_dictionary_contains(Id, Key: clm_id): Boolean;
var I: SizeInt;
begin
  I := FindDictionary(Id);
  Result := (I >= 0) and (FindKey(Dictionaries[I], Key) >= 0);
end;

function clm_dictionary_set_from(Id, Key: clm_id; Value: Pointer; ValueSize: SizeUInt): LongInt;
var D, V: SizeInt; N: SizeUInt;
begin
  D := FindDictionary(Id);
  if (D < 0) or (Value = nil) then Exit(CLM_BPA_FAILURE);
  if Dictionaries[D].ValueType = CLM_BPA_TYPE_STRING then begin
    N := 0;
    while (N < ValueSize) and (PAnsiChar(Value)[N] <> #0) do Inc(N);
    if N = ValueSize then Exit(CLM_BPA_FAILURE);
    ValueSize := N + 1;
  end else if ValueSize <> Dictionaries[D].ValueSize then Exit(CLM_BPA_FAILURE);
  V := FindKey(Dictionaries[D], Key);
  if V < 0 then begin
    V := Length(Dictionaries[D].Values);
    SetLength(Dictionaries[D].Values, V + 1);
    Dictionaries[D].Values[V].Key := Key;
  end;
  SetLength(Dictionaries[D].Values[V].Bytes, ValueSize);
  if ValueSize > 0 then Move(Value^, Dictionaries[D].Values[V].Bytes[0], ValueSize);
  Dictionaries[D].Iterator := 0;
  Result := CLM_BPA_SET_SUCCESS;
end;

function clm_dictionary_set(Id, Key, SourceBlockId: clm_id): LongInt;
var D: SizeInt; Block: pclm_bpa_block_node; N: SizeUInt;
begin
  D := FindDictionary(Id);
  Block := clm_bpa_find_block(SourceBlockId);
  if (D < 0) or (Block = nil) or (Block^.kind <> Dictionaries[D].ValueType) then Exit(CLM_BPA_FAILURE);
  if Block^.kind = CLM_BPA_TYPE_STRING then N := Block^.length + 1 else N := Block^.block_size;
  Result := clm_dictionary_set_from(Id, Key, Block^.data, N);
end;

function clm_dictionary_get_to(Id, Key: clm_id; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
var D, V: SizeInt; N: SizeUInt;
begin
  D := FindDictionary(Id);
  if D < 0 then Exit(CLM_BPA_FAILURE);
  V := FindKey(Dictionaries[D], Key);
  if (V < 0) or (Buffer = nil) then Exit(CLM_BPA_FAILURE);
  N := Length(Dictionaries[D].Values[V].Bytes);
  if BufferSize < N then Exit(CLM_BPA_FAILURE);
  if N > 0 then Move(Dictionaries[D].Values[V].Bytes[0], Buffer^, N);
  Result := CLM_BPA_GET_SUCCESS;
end;

function clm_dictionary_get(Id, Key, DestinationBlockId: clm_id): LongInt;
var D, V: SizeInt; Block: pclm_bpa_block_node; Bytes: TDictionaryBytes;
begin
  D := FindDictionary(Id);
  Block := clm_bpa_find_block(DestinationBlockId);
  if (D < 0) or (Block = nil) or (Block^.kind <> Dictionaries[D].ValueType) then Exit(CLM_BPA_FAILURE);
  V := FindKey(Dictionaries[D], Key);
  if V < 0 then Exit(CLM_BPA_FAILURE);
  Bytes := Dictionaries[D].Values[V].Bytes;
  if Length(Bytes) = 0 then Exit(CLM_BPA_FAILURE);
  if Block^.kind = CLM_BPA_TYPE_STRING then Result := clm_bpa_set_string(DestinationBlockId, @Bytes[0], Length(Bytes))
  else Result := clm_bpa_set(DestinationBlockId, @Bytes[0], Length(Bytes));
  if Result >= 0 then Result := CLM_BPA_GET_SUCCESS;
end;

function clm_dictionary_count(Id: clm_id): clm_int;
var I: SizeInt;
begin I := FindDictionary(Id); if I < 0 then Exit(CLM_BPA_FAILURE); Result := Length(Dictionaries[I].Values); end;

function clm_dictionary_remove(Id, Key: clm_id): LongInt;
var D, V, I: SizeInt;
begin
  D := FindDictionary(Id); if D < 0 then Exit(CLM_BPA_FAILURE);
  V := FindKey(Dictionaries[D], Key); if V < 0 then Exit(CLM_BPA_FAILURE);
  for I := V to High(Dictionaries[D].Values) - 1 do Dictionaries[D].Values[I] := Dictionaries[D].Values[I + 1];
  SetLength(Dictionaries[D].Values, Length(Dictionaries[D].Values) - 1);
  Dictionaries[D].Iterator := 0;
  Result := CLM_BPA_SUCCESS;
end;

function clm_dictionary_iterate_reset(Id: clm_id): LongInt;
var I: SizeInt;
begin I := FindDictionary(Id); if I < 0 then Exit(CLM_BPA_FAILURE); Dictionaries[I].Iterator := 0; Result := CLM_BPA_SUCCESS; end;

function clm_dictionary_iterate_next(Id: clm_id; out Key: clm_id): LongInt;
var I: SizeInt;
begin
  I := FindDictionary(Id);
  if (I < 0) or (Dictionaries[I].Iterator >= Length(Dictionaries[I].Values)) then Exit(CLM_BPA_FAILURE);
  Key := Dictionaries[I].Values[Dictionaries[I].Iterator].Key;
  Inc(Dictionaries[I].Iterator);
  Result := CLM_BPA_GET_SUCCESS;
end;

function clm_dictionary_clear(Id: clm_id): LongInt;
var I: SizeInt;
begin I := FindDictionary(Id); if I < 0 then Exit(CLM_BPA_FAILURE); SetLength(Dictionaries[I].Values, 0); Dictionaries[I].Iterator := 0; Result := CLM_BPA_SUCCESS; end;
function clm_dictionary_add_from(Id, Key: clm_id; Value: Pointer; ValueSize: SizeUInt): LongInt;
begin if clm_dictionary_contains(Id, Key) then Exit(CLM_BPA_FAILURE); Result := clm_dictionary_set_from(Id, Key, Value, ValueSize); end;
function clm_dictionary_update_from(Id, Key: clm_id; Value: Pointer; ValueSize: SizeUInt): LongInt;
begin if not clm_dictionary_contains(Id, Key) then Exit(CLM_BPA_FAILURE); Result := clm_dictionary_set_from(Id, Key, Value, ValueSize); end;

function clm_dictionary_keys_to(Id: clm_id; Buffer: Pclm_id; Capacity: SizeUInt): LongInt;
var D, I: SizeInt;
begin
  D := FindDictionary(Id);
  if (D < 0) or (Capacity < Length(Dictionaries[D].Values)) or ((Length(Dictionaries[D].Values) > 0) and (Buffer = nil)) then Exit(CLM_BPA_FAILURE);
  for I := 0 to High(Dictionaries[D].Values) do Buffer[I] := Dictionaries[D].Values[I].Key;
  Result := CLM_BPA_GET_SUCCESS;
end;

end.