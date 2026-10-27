unit bpa;

{$mode objfpc}{$H+}

interface

uses
  base;

const
  CLM_BPA_FAILURE = -1;
  CLM_BPA_SUCCESS = 0;
  CLM_BPA_GET_SUCCESS = 1;
  CLM_BPA_SET_SUCCESS = 2;
  CLM_BPA_TYPE_STRUCT = 0;
  CLM_BPA_TYPE_SIGNED_BYTE = 1;
  CLM_BPA_TYPE_BYTE = 2;
  CLM_BPA_TYPE_SHORT = 3;
  CLM_BPA_TYPE_UNSIGNED_SHORT = 4;
  CLM_BPA_TYPE_INT = 5;
  CLM_BPA_TYPE_UNSIGNED_INT = 6;
  CLM_BPA_TYPE_LONG = 7;
  CLM_BPA_TYPE_UNSIGNED_LONG = 8;
  CLM_BPA_TYPE_FLOAT = 9;
  CLM_BPA_TYPE_DOUBLE = 10;
  CLM_BPA_TYPE_STRING = 11;
  CLM_BPA_TYPE_UUID = 12;
  CLM_BPA_TYPE_DATETIME = 13;
  CLM_BPA_POOL_SLAB_SLOTS = 32;
  CLM_BPA_POOL_MAX_SLOT = 4096;
  CLM_BPA_POOL_CLASSES = 10;

type
  clm_bpa_block_node = record
    id: clm_id;
    kind: clm_byte;
    refcount: clm_ushort;
    block_size: clm_uint;
    encoding: clm_byte;
    length: clm_uint;
    data: Pointer;
  end;
  pclm_bpa_block_node = ^clm_bpa_block_node;

  clm_bpaa_array_node = record
    id: clm_id;
    kind: clm_byte;
    refcount: clm_ushort;
    count: clm_uint;
    capacity: clm_uint;
    block_size: clm_uint;
    encoding: clm_byte;
    index: clm_uint;
    sealed: clm_byte;
    deck_size: clm_uint;
    blocks: PByte;
    strings: ^PAnsiChar;
  end;
  pclm_bpaa_array_node = ^clm_bpaa_array_node;
  clm_bpaa_compare_fn = function(Left, Right: Pointer): LongInt;
  clm_bpaa_predicate_fn = function(Left, Right: Pointer): LongInt;

function clm_bpa_type_size(Kind: clm_byte): clm_uint;
function clm_bpa_hash_id(Id: clm_id): SizeUInt;
function clm_bpa_pool_class_for(Bytes: SizeUInt): SizeUInt;
function clm_bpa_pool_alloc(Bytes: SizeUInt): Pointer;
procedure clm_bpa_pool_free(Value: Pointer; Bytes: SizeUInt);
function clm_bpa_pool_cleanup: LongInt;
function clm_bpa_pool_stats(Bytes: SizeUInt; out Active, Capacity: SizeUInt): LongInt;
function clm_bpa_find_block(Id: clm_id): pclm_bpa_block_node;
function clm_bpaa_find_array(Id: clm_id): pclm_bpaa_array_node;
function clm_bpa_is_existing(Id: clm_id): Boolean;
function clm_bpaa_is_existing(Id: clm_id): Boolean;
function clm_bpa_create_sized(Id: clm_id; Kind: clm_byte; BlockSize: clm_uint): LongInt;
function clm_bpa_create(Id: clm_id; Kind: clm_byte): LongInt;
function clm_bpa_delete(Id: clm_id): LongInt;
function clm_bpa_ref(Id: clm_id): LongInt;
function clm_bpa_unref(Id: clm_id): LongInt;
function clm_bpa_get(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpa_set(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpa_get_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
function clm_bpa_set_from(Id: clm_id; Source: Pointer; Size: clm_uint): LongInt;
function clm_bpa_get_string(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpa_set_string(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpa_get_string_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
function clm_bpa_set_string_from(Id: clm_id; Source: Pointer; Size: clm_uint): LongInt;
function clm_bpa_replace(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpa_borrow(Id: clm_id; out Address: Pointer; out Bytes: SizeUInt): LongInt;
function clm_bpa_copy_block(SourceId, DestinationId: clm_id): LongInt;
function clm_bpa_shutdown: LongInt;

function clm_bpaa_create_sized(Id: clm_id; Kind: clm_byte; Reserved: clm_int; BlockSize: clm_uint; Encoding: clm_byte): LongInt;
function clm_bpaa_create(Id: clm_id; Kind: clm_byte; Reserved: clm_int; Encoding: clm_byte): LongInt;
function clm_bpaa_delete(Id: clm_id): LongInt;
function clm_bpaa_ref(Id: clm_id): LongInt;
function clm_bpaa_unref(Id: clm_id): LongInt;
function clm_bpaa_append_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_insert_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_append_from(Id: clm_id; Source: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_insert_from(Id: clm_id; Index: clm_int; Source: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_get_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_set_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_get_string_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_set_string_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_get_first_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_get_last_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_get_current_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_get_to(Id: clm_id; Index: clm_int; Destination: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_get_first_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_get_last_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_get_current_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_get_string_to(Id: clm_id; Index: clm_int; Destination: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_set_from(Id: clm_id; Index: clm_int; Source: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_set_string_from(Id: clm_id; Index: clm_int; Source: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_draw_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_draw_at_to(Id: clm_id; Index: clm_int; Destination: Pointer; Size: clm_uint): LongInt;
function clm_bpaa_get_count_to(Id: clm_id; out Count: clm_uint): LongInt;
function clm_bpaa_get_capacity_to(Id: clm_id; out Capacity: clm_uint): LongInt;
function clm_bpaa_get_length_to(Id: clm_id; out Length: clm_uint): LongInt;
function clm_bpaa_get_current_index_to(Id: clm_id; out Index: clm_int): LongInt;
function clm_bpaa_is_sealed_to(Id: clm_id; out Sealed: clm_byte): LongInt;
function clm_bpaa_extend(Id: clm_id; AdditionalCapacity: clm_uint): LongInt;
function clm_bpaa_shrink(Id: clm_id; NewCapacity: clm_uint): LongInt;
function clm_bpaa_remove(Id: clm_id; Index: clm_int): LongInt;
function clm_bpaa_remove_first(Id: clm_id): LongInt;
function clm_bpaa_remove_last(Id: clm_id): LongInt;
function clm_bpaa_clear(Id: clm_id): LongInt;
function clm_bpaa_slice(SourceId: clm_id; Start, Count: clm_int; DestinationId: clm_id): LongInt;
function clm_bpaa_slice_borrow(SourceId: clm_id; Start, Count: clm_int; out Buffer: Pointer; out BufferSize: SizeUInt): LongInt;
function clm_bpaa_slice_to(SourceId: clm_id; Start, Count: clm_int; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
function clm_bpaa_advanced_borrow(Id: clm_id; out ArrayNode: pclm_bpaa_array_node): LongInt;
function clm_bpaa_advanced_release(var ArrayNode: pclm_bpaa_array_node): LongInt;
function clm_bpaa_advanced_data(ArrayNode: pclm_bpaa_array_node): Pointer;
function clm_bpaa_advanced_get_raw(ArrayNode: pclm_bpaa_array_node; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_advanced_set_raw(ArrayNode: pclm_bpaa_array_node; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_advanced_insert_raw(ArrayNode: pclm_bpaa_array_node; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_advanced_append_raw(ArrayNode: pclm_bpaa_array_node; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_advanced_remove(ArrayNode: pclm_bpaa_array_node; Index: clm_int): LongInt;
function clm_bpaa_advanced_clear(ArrayNode: pclm_bpaa_array_node): LongInt;
function clm_bpaa_advanced_extend(ArrayNode: pclm_bpaa_array_node; AdditionalCapacity: clm_uint): LongInt;
function clm_bpaa_advanced_shrink(ArrayNode: pclm_bpaa_array_node; NewCapacity: clm_uint): LongInt;
function clm_bpaa_reset(Id: clm_id): LongInt;
function clm_bpaa_move_first(Id: clm_id): LongInt;
function clm_bpaa_move_previous(Id: clm_id): LongInt;
function clm_bpaa_move_next(Id: clm_id): LongInt;
function clm_bpaa_move_last(Id: clm_id): LongInt;
function clm_bpaa_move_to(Id: clm_id; Index: clm_int): LongInt;
function clm_bpaa_seal(Id: clm_id): LongInt;
function clm_bpaa_unseal(Id: clm_id): LongInt;
function clm_bpaa_sort_with(Id: clm_id; Compare: clm_bpaa_compare_fn): LongInt;
function clm_bpaa_sort(Id: clm_id; Predicate: clm_bpaa_predicate_fn): LongInt;
function clm_bpaa_reverse(Id: clm_id): LongInt;
function clm_bpaa_shift_left(Id: clm_id; Index, Positions: clm_uint): LongInt;
function clm_bpaa_shift_right(Id: clm_id; Index, Positions: clm_uint): LongInt;
function clm_bpaa_swap(Id: clm_id; Index1, Index2: clm_int): LongInt;
function clm_bpaa_copy(Id: clm_id; SourceIndex, DestinationIndex: clm_int): LongInt;
function clm_bpaa_shuffle(Id: clm_id): LongInt;
function clm_bpaa_draw_at_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_draw_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
function clm_bpaa_get(Id: clm_id; Index: clm_int; BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
function clm_bpaa_set(Id: clm_id; Index: clm_int; BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
function clm_bpaa_append_string(Id, BlockId: clm_id): LongInt;
function clm_bpaa_insert_string(Id: clm_id; Index: clm_int; BlockId: clm_id): LongInt;
function clm_bpaa_set_string(Id: clm_id; Index: clm_int; BlockId: clm_id): LongInt;
function clm_bpaa_get_string(Id: clm_id; Index: clm_int; BlockId: clm_id): LongInt;
function clm_bpaa_insert(Id: clm_id; Index: clm_int; BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
function clm_bpaa_append(Id, BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
function clm_bpaa_draw(Id, BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
function clm_bpaa_draw_at(Id: clm_id; Index: clm_int; BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
function clm_bpaa_write_u32(DestinationId: clm_id; Value: clm_uint): LongInt;
function clm_bpaa_write_i32(DestinationId: clm_id; Value: clm_int): LongInt;
function clm_bpaa_write_byte(DestinationId: clm_id; Value: clm_byte): LongInt;
function clm_bpaa_get_count(Id, DestinationId: clm_id): LongInt;
function clm_bpaa_get_capacity(Id, DestinationId: clm_id): LongInt;
function clm_bpaa_get_length(Id, DestinationId: clm_id): LongInt;
function clm_bpaa_get_current_index(Id, DestinationId: clm_id): LongInt;
function clm_bpaa_is_sealed(Id, DestinationId: clm_id): LongInt;
function clm_bpaa_get_first(Id, DestinationId: clm_id; ExpectedSize: SizeUInt): LongInt;
function clm_bpaa_get_last(Id, DestinationId: clm_id; ExpectedSize: SizeUInt): LongInt;
function clm_bpaa_get_current(Id, DestinationId: clm_id; ExpectedSize: SizeUInt): LongInt;

implementation

uses
  Math, SysUtils;

type
  TBlockPointers = array of pclm_bpa_block_node;
  TArrayPointers = array of pclm_bpaa_array_node;
  TPoolStat = record
    Active: SizeUInt;
    Capacity: SizeUInt;
  end;

var
  Blocks: TBlockPointers;
  Arrays: TArrayPointers;
  PoolStats: array[0..CLM_BPA_POOL_CLASSES - 1] of TPoolStat;

function clm_bpa_pool_class_for(Bytes: SizeUInt): SizeUInt;
var
  SlotSize: SizeUInt;
  ClassIndex: SizeUInt;
begin
  if (Bytes = 0) or (Bytes > CLM_BPA_POOL_MAX_SLOT) then
    Exit(CLM_BPA_POOL_CLASSES);
  SlotSize := 8;
  ClassIndex := 0;
  while (SlotSize < Bytes) and (ClassIndex + 1 < CLM_BPA_POOL_CLASSES) do
  begin
    SlotSize := SlotSize * 2;
    Inc(ClassIndex);
  end;
  if SlotSize >= Bytes then
    Result := ClassIndex
  else
    Result := CLM_BPA_POOL_CLASSES;
end;

function clm_bpa_pool_alloc(Bytes: SizeUInt): Pointer;
var
  PoolClass: SizeUInt;
begin
  if Bytes = 0 then
    Exit(nil);
  GetMem(Result, Bytes);
  if Result = nil then
    Exit;
  FillChar(Result^, Bytes, 0);
  PoolClass := clm_bpa_pool_class_for(Bytes);
  if PoolClass < CLM_BPA_POOL_CLASSES then
  begin
    Inc(PoolStats[PoolClass].Active);
    if PoolStats[PoolClass].Active > PoolStats[PoolClass].Capacity then
      Inc(PoolStats[PoolClass].Capacity, CLM_BPA_POOL_SLAB_SLOTS);
  end;
end;

procedure clm_bpa_pool_free(Value: Pointer; Bytes: SizeUInt);
var
  PoolClass: SizeUInt;
begin
  if Value = nil then
    Exit;
  PoolClass := clm_bpa_pool_class_for(Bytes);
  if (PoolClass < CLM_BPA_POOL_CLASSES) and (PoolStats[PoolClass].Active > 0) then
    Dec(PoolStats[PoolClass].Active);
  FreeMem(Value);
end;

function clm_bpa_pool_cleanup: LongInt;
var
  Index: SizeInt;
begin
  for Index := 0 to High(PoolStats) do
    if PoolStats[Index].Active <> 0 then
      Exit(CLM_BPA_FAILURE);
  FillChar(PoolStats, SizeOf(PoolStats), 0);
  Result := CLM_BPA_SUCCESS;
end;

function clm_bpa_pool_stats(Bytes: SizeUInt; out Active, Capacity: SizeUInt): LongInt;
var
  PoolClass: SizeUInt;
begin
  PoolClass := clm_bpa_pool_class_for(Bytes);
  if PoolClass >= CLM_BPA_POOL_CLASSES then
    Exit(CLM_BPA_FAILURE);
  Active := PoolStats[PoolClass].Active;
  Capacity := PoolStats[PoolClass].Capacity;
  Result := CLM_BPA_SUCCESS;
end;

function clm_bpa_type_size(Kind: clm_byte): clm_uint;
begin
  case Kind of
    CLM_BPA_TYPE_SIGNED_BYTE, CLM_BPA_TYPE_BYTE: Result := 1;
    CLM_BPA_TYPE_SHORT, CLM_BPA_TYPE_UNSIGNED_SHORT: Result := 2;
    CLM_BPA_TYPE_INT, CLM_BPA_TYPE_UNSIGNED_INT, CLM_BPA_TYPE_FLOAT: Result := 4;
    CLM_BPA_TYPE_LONG, CLM_BPA_TYPE_UNSIGNED_LONG, CLM_BPA_TYPE_DOUBLE,
      CLM_BPA_TYPE_DATETIME: Result := 8;
    CLM_BPA_TYPE_UUID: Result := 16;
  else
    Result := 0;
  end;
end;

function clm_bpa_hash_id(Id: clm_id): SizeUInt;
begin
  Id := Id xor (Id shr 30);
  Id := Id * QWord($BF58476D1CE4E5B9);
  Id := Id xor (Id shr 27);
  Id := Id * QWord($94D049BB133111EB);
  Id := Id xor (Id shr 31);
  Result := SizeUInt(Id);
end;

function clm_bpa_find_block(Id: clm_id): pclm_bpa_block_node;
var
  Index: SizeInt;
begin
  for Index := 0 to High(Blocks) do
    if (Blocks[Index] <> nil) and (Blocks[Index]^.id = Id) then
      Exit(Blocks[Index]);
  Result := nil;
end;

function clm_bpaa_find_array(Id: clm_id): pclm_bpaa_array_node;
var
  Index: SizeInt;
begin
  for Index := 0 to High(Arrays) do
    if (Arrays[Index] <> nil) and (Arrays[Index]^.id = Id) then
      Exit(Arrays[Index]);
  Result := nil;
end;

function clm_bpa_is_existing(Id: clm_id): Boolean;
begin
  Result := clm_bpa_find_block(Id) <> nil;
end;

function clm_bpaa_is_existing(Id: clm_id): Boolean;
begin
  Result := clm_bpaa_find_array(Id) <> nil;
end;

function RegisterBlock(Block: pclm_bpa_block_node): Boolean;
var
  Index: SizeInt;
begin
  for Index := 0 to High(Blocks) do
    if Blocks[Index] = nil then
    begin
      Blocks[Index] := Block;
      Exit(True);
    end;
  Index := Length(Blocks);
  SetLength(Blocks, Index + 1);
  Blocks[Index] := Block;
  Result := True;
end;

function RegisterArray(ArrayNode: pclm_bpaa_array_node): Boolean;
var
  Index: SizeInt;
begin
  for Index := 0 to High(Arrays) do
    if Arrays[Index] = nil then
    begin
      Arrays[Index] := ArrayNode;
      Exit(True);
    end;
  Index := Length(Arrays);
  SetLength(Arrays, Index + 1);
  Arrays[Index] := ArrayNode;
  Result := True;
end;

function clm_bpa_create_sized(Id: clm_id; Kind: clm_byte; BlockSize: clm_uint): LongInt;
var
  Node: pclm_bpa_block_node;
  ResolvedSize: clm_uint;
begin
  if clm_bpa_is_existing(Id) or clm_bpaa_is_existing(Id) then
    Exit(CLM_BPA_FAILURE);
  ResolvedSize := clm_bpa_type_size(Kind);
  if Kind = CLM_BPA_TYPE_STRING then
  begin
    if BlockSize <> 0 then Exit(CLM_BPA_FAILURE);
  end
  else if Kind = CLM_BPA_TYPE_STRUCT then
  begin
    if BlockSize = 0 then Exit(CLM_BPA_FAILURE);
    ResolvedSize := BlockSize;
  end
  else if (ResolvedSize = 0) or ((BlockSize <> 0) and (BlockSize <> ResolvedSize)) then
    Exit(CLM_BPA_FAILURE);
  New(Node);
  FillChar(Node^, SizeOf(Node^), 0);
  Node^.id := Id;
  Node^.kind := Kind;
  Node^.refcount := 1;
  Node^.block_size := ResolvedSize;
  if Kind <> CLM_BPA_TYPE_STRING then
  begin
    Node^.data := clm_bpa_pool_alloc(ResolvedSize);
    if Node^.data = nil then
    begin
      Dispose(Node);
      Exit(CLM_BPA_FAILURE);
    end;
  end;
  RegisterBlock(Node);
  Result := CLM_BPA_SUCCESS;
end;

function clm_bpa_create(Id: clm_id; Kind: clm_byte): LongInt;
begin
  if Kind = CLM_BPA_TYPE_STRUCT then
    Exit(CLM_BPA_FAILURE);
  Result := clm_bpa_create_sized(Id, Kind, 0);
end;

function clm_bpa_delete(Id: clm_id): LongInt;
var
  Index: SizeInt;
  Node: pclm_bpa_block_node;
begin
  Node := clm_bpa_find_block(Id);
  if Node = nil then Exit(CLM_BPA_FAILURE);
  for Index := 0 to High(Blocks) do
    if Blocks[Index] = Node then
    begin
      Blocks[Index] := nil;
      Break;
    end;
  clm_bpa_pool_free(Node^.data, IfThen(Node^.kind = CLM_BPA_TYPE_STRING, SizeUInt(Node^.length) + 1, Node^.block_size));
  Dispose(Node);
  Result := CLM_BPA_SUCCESS;
end;

function clm_bpa_ref(Id: clm_id): LongInt;
var
  Node: pclm_bpa_block_node;
begin
  Node := clm_bpa_find_block(Id);
  if (Node = nil) or (Node^.refcount = High(clm_ushort)) then Exit(CLM_BPA_FAILURE);
  Inc(Node^.refcount);
  Result := CLM_BPA_SUCCESS;
end;

function clm_bpa_unref(Id: clm_id): LongInt;
var
  Node: pclm_bpa_block_node;
begin
  Node := clm_bpa_find_block(Id);
  if (Node = nil) or (Node^.refcount = 0) then Exit(CLM_BPA_FAILURE);
  Dec(Node^.refcount);
  if Node^.refcount = 0 then Result := clm_bpa_delete(Id)
  else Result := CLM_BPA_SUCCESS;
end;

function clm_bpa_get(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var
  Node: pclm_bpa_block_node;
begin
  Node := clm_bpa_find_block(Id);
  if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Buffer = nil) or (BufferSize < Node^.block_size) then Exit(CLM_BPA_FAILURE);
  Move(Node^.data^, Buffer^, Node^.block_size);
  Result := CLM_BPA_GET_SUCCESS;
end;

function clm_bpa_set(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var
  Node: pclm_bpa_block_node;
begin
  Node := clm_bpa_find_block(Id);
  if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Buffer = nil) or (BufferSize <> Node^.block_size) then Exit(CLM_BPA_FAILURE);
  Move(Buffer^, Node^.data^, Node^.block_size);
  Result := CLM_BPA_SET_SUCCESS;
end;

function clm_bpa_get_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpa_get(Id, Destination, Size); end;
function clm_bpa_set_from(Id: clm_id; Source: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpa_set(Id, Source, Size); end;

function BoundedLength(Buffer: Pointer; BufferSize: clm_uint; out ValueLength: clm_uint): Boolean;
var
  Bytes: PAnsiChar;
  Index: SizeUInt;
begin
  Result := False;
  if (Buffer = nil) or (BufferSize = 0) then Exit;
  Bytes := Buffer;
  Index := 0;
  while (Index < BufferSize) and (Bytes[Index] <> #0) do Inc(Index);
  ValueLength := Index;
  Result := True;
end;

function clm_bpa_get_string(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var
  Node: pclm_bpa_block_node;
begin
  Node := clm_bpa_find_block(Id);
  if (Node = nil) or (Node^.kind <> CLM_BPA_TYPE_STRING) or (Buffer = nil) or (BufferSize <= Node^.length) or (Node^.length > High(LongInt)) then Exit(CLM_BPA_FAILURE);
  if Node^.length > 0 then Move(Node^.data^, Buffer^, Node^.length);
  PAnsiChar(Buffer)[Node^.length] := #0;
  Result := Node^.length;
end;

function clm_bpa_set_string(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var
  Node: pclm_bpa_block_node;
  Copy: PAnsiChar;
  ValueLength: clm_uint;
begin
  Node := clm_bpa_find_block(Id);
  if (Node = nil) or (Node^.kind <> CLM_BPA_TYPE_STRING) or not BoundedLength(Buffer, BufferSize, ValueLength) or (ValueLength > High(LongInt)) then Exit(CLM_BPA_FAILURE);
  GetMem(Copy, ValueLength + 1);
  if ValueLength > 0 then Move(Buffer^, Copy^, ValueLength);
  Copy[ValueLength] := #0;
  clm_bpa_pool_free(Node^.data, SizeUInt(Node^.length) + 1);
  Node^.data := Copy;
  Node^.length := ValueLength;
  Result := ValueLength;
end;

function clm_bpa_get_string_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpa_get_string(Id, Destination, Size); end;
function clm_bpa_set_string_from(Id: clm_id; Source: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpa_set_string(Id, Source, Size); end;

function clm_bpa_replace(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var
  Node: pclm_bpa_block_node;
begin
  Node := clm_bpa_find_block(Id);
  if Node = nil then Exit(CLM_BPA_FAILURE);
  if Node^.kind = CLM_BPA_TYPE_STRING then Result := clm_bpa_set_string(Id, Buffer, BufferSize)
  else Result := clm_bpa_set(Id, Buffer, BufferSize);
end;

function clm_bpa_borrow(Id: clm_id; out Address: Pointer; out Bytes: SizeUInt): LongInt;
var
  Node: pclm_bpa_block_node;
begin
  Address := nil;
  Bytes := 0;
  Node := clm_bpa_find_block(Id);
  if Node = nil then Exit(CLM_BPA_FAILURE);
  Address := Node^.data;
  if Node^.kind = CLM_BPA_TYPE_STRING then Bytes := Node^.length + 1 else Bytes := Node^.block_size;
  Result := CLM_BPA_GET_SUCCESS;
end;

function clm_bpa_copy_block(SourceId, DestinationId: clm_id): LongInt;
var
  Source, Destination: pclm_bpa_block_node;
begin
  Source := clm_bpa_find_block(SourceId);
  Destination := clm_bpa_find_block(DestinationId);
  if (Source = nil) or (Destination = nil) or (Source^.kind <> Destination^.kind) then Exit(CLM_BPA_FAILURE);
  if Source^.kind = CLM_BPA_TYPE_STRING then
  begin
    if Source^.data = nil then Exit(CLM_BPA_FAILURE);
    Result := clm_bpa_set_string(DestinationId, Source^.data, Source^.length + 1);
  end
  else if Source^.block_size <> Destination^.block_size then Exit(CLM_BPA_FAILURE)
  else
  begin
    Move(Source^.data^, Destination^.data^, Source^.block_size);
    Result := CLM_BPA_SUCCESS;
  end;
end;

function ArrayResize(Node: pclm_bpaa_array_node; NewCapacity: clm_uint): LongInt;
var
  OldBytes, NewBytes: SizeUInt;
begin
  if Node = nil then Exit(CLM_BPA_FAILURE);
  if Node^.kind = CLM_BPA_TYPE_STRING then
  begin
    OldBytes := SizeUInt(Node^.capacity) * SizeOf(PAnsiChar);
    NewBytes := SizeUInt(NewCapacity) * SizeOf(PAnsiChar);
    if NewBytes = 0 then
    begin
      if Node^.strings <> nil then FreeMem(Node^.strings);
      Node^.strings := nil;
    end
    else
    begin
      ReAllocMem(Node^.strings, NewBytes);
      if NewBytes > OldBytes then FillChar((PByte(Node^.strings) + OldBytes)^, NewBytes - OldBytes, 0);
    end;
  end
  else
  begin
    OldBytes := SizeUInt(Node^.capacity) * Node^.block_size;
    NewBytes := SizeUInt(NewCapacity) * Node^.block_size;
    if NewBytes = 0 then
    begin
      if Node^.blocks <> nil then FreeMem(Node^.blocks);
      Node^.blocks := nil;
    end
    else
    begin
      ReAllocMem(Node^.blocks, NewBytes);
      if NewBytes > OldBytes then FillChar((Node^.blocks + OldBytes)^, NewBytes - OldBytes, 0);
    end;
  end;
  Node^.capacity := NewCapacity;
  Result := CLM_BPA_SUCCESS;
end;

function clm_bpaa_create_sized(Id: clm_id; Kind: clm_byte; Reserved: clm_int; BlockSize: clm_uint; Encoding: clm_byte): LongInt;
var
  Node: pclm_bpaa_array_node;
  ResolvedSize: clm_uint;
begin
  if clm_bpaa_is_existing(Id) or clm_bpa_is_existing(Id) or (Reserved < 0) then Exit(CLM_BPA_FAILURE);
  ResolvedSize := clm_bpa_type_size(Kind);
  if Kind = CLM_BPA_TYPE_STRING then
  begin
    if BlockSize <> 0 then Exit(CLM_BPA_FAILURE);
  end
  else if Kind = CLM_BPA_TYPE_STRUCT then
  begin
    if BlockSize = 0 then Exit(CLM_BPA_FAILURE);
    ResolvedSize := BlockSize;
  end
  else if (ResolvedSize = 0) or ((BlockSize <> 0) and (BlockSize <> ResolvedSize)) then Exit(CLM_BPA_FAILURE);
  New(Node);
  FillChar(Node^, SizeOf(Node^), 0);
  Node^.id := Id;
  Node^.kind := Kind;
  Node^.refcount := 1;
  Node^.block_size := ResolvedSize;
  Node^.encoding := Encoding;
  Node^.capacity := Reserved;
  if Reserved > 0 then
    if ArrayResize(Node, Reserved) <> CLM_BPA_SUCCESS then
    begin
      Dispose(Node);
      Exit(CLM_BPA_FAILURE);
    end;
  RegisterArray(Node);
  Result := CLM_BPA_SUCCESS;
end;

function clm_bpaa_create(Id: clm_id; Kind: clm_byte; Reserved: clm_int; Encoding: clm_byte): LongInt;
begin
  if Kind = CLM_BPA_TYPE_STRUCT then Exit(CLM_BPA_FAILURE);
  Result := clm_bpaa_create_sized(Id, Kind, Reserved, 0, Encoding);
end;

function clm_bpaa_delete(Id: clm_id): LongInt;
var
  Node: pclm_bpaa_array_node;
  Index: SizeInt;
begin
  Node := clm_bpaa_find_array(Id);
  if Node = nil then Exit(CLM_BPA_FAILURE);
  for Index := 0 to High(Arrays) do
    if Arrays[Index] = Node then
    begin
      Arrays[Index] := nil;
      Break;
    end;
  if Node^.kind = CLM_BPA_TYPE_STRING then
    for Index := 0 to Node^.count - 1 do
    begin
      clm_bpa_pool_free(Node^.strings[Index], StrLen(Node^.strings[Index]) + 1);
      Node^.strings[Index] := nil;
    end;
  if Node^.strings <> nil then FreeMem(Node^.strings);
  if Node^.blocks <> nil then FreeMem(Node^.blocks);
  Dispose(Node);
  Result := CLM_BPA_SUCCESS;
end;

function clm_bpaa_ref(Id: clm_id): LongInt;
var Node: pclm_bpaa_array_node;
begin
  Node := clm_bpaa_find_array(Id);
  if (Node = nil) or (Node^.refcount = High(clm_ushort)) then Exit(CLM_BPA_FAILURE);
  Inc(Node^.refcount); Result := CLM_BPA_SUCCESS;
end;

function clm_bpaa_unref(Id: clm_id): LongInt;
var Node: pclm_bpaa_array_node;
begin
  Node := clm_bpaa_find_array(Id);
  if (Node = nil) or (Node^.refcount = 0) then Exit(CLM_BPA_FAILURE);
  Dec(Node^.refcount);
  if Node^.refcount = 0 then Result := clm_bpaa_delete(Id) else Result := CLM_BPA_SUCCESS;
end;

function EnsureCapacity(Node: pclm_bpaa_array_node; Needed: clm_uint): LongInt;
var NewCapacity: clm_uint;
begin
  if Needed <= Node^.capacity then Exit(CLM_BPA_SUCCESS);
  if Node^.sealed <> 0 then Exit(CLM_BPA_FAILURE);
  NewCapacity := Node^.capacity;
  if NewCapacity = 0 then NewCapacity := 4;
  while NewCapacity < Needed do
  begin
    if NewCapacity > High(clm_uint) div 2 then begin NewCapacity := Needed; Break; end;
    NewCapacity := NewCapacity * 2;
  end;
  Result := ArrayResize(Node, NewCapacity);
end;

function clm_bpaa_insert_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var
  Node: pclm_bpaa_array_node;
  Copy: Pointer;
  LengthValue: clm_uint;
  I: clm_uint;
begin
  Node := clm_bpaa_find_array(Id);
  if (Node = nil) or (Index < 0) or (clm_uint(Index) > Node^.count) or (Node^.count = High(clm_uint)) then Exit(CLM_BPA_FAILURE);
  if Node^.kind = CLM_BPA_TYPE_STRING then
  begin
    if not BoundedLength(Buffer, BufferSize, LengthValue) then Exit(CLM_BPA_FAILURE);
    GetMem(Copy, LengthValue + 1);
    if LengthValue > 0 then Move(Buffer^, Copy^, LengthValue);
    PAnsiChar(Copy)[LengthValue] := #0;
  end
  else
  begin
    if (Buffer = nil) or (BufferSize <> Node^.block_size) then Exit(CLM_BPA_FAILURE);
    GetMem(Copy, Node^.block_size);
    Move(Buffer^, Copy^, Node^.block_size);
  end;
  if EnsureCapacity(Node, Node^.count + 1) <> CLM_BPA_SUCCESS then
  begin
    if Node^.kind = CLM_BPA_TYPE_STRING then clm_bpa_pool_free(Copy, LengthValue + 1) else FreeMem(Copy);
    Exit(CLM_BPA_FAILURE);
  end;
  for I := Node^.count downto clm_uint(Index) + 1 do
    if Node^.kind = CLM_BPA_TYPE_STRING then Node^.strings[I] := Node^.strings[I - 1]
    else Move((Node^.blocks + SizeUInt(I - 1) * Node^.block_size)^, (Node^.blocks + SizeUInt(I) * Node^.block_size)^, Node^.block_size);
  if Node^.kind = CLM_BPA_TYPE_STRING then Node^.strings[Index] := Copy
  else Move(Copy^, (Node^.blocks + SizeUInt(Index) * Node^.block_size)^, Node^.block_size);
  if Node^.kind <> CLM_BPA_TYPE_STRING then FreeMem(Copy);
  Inc(Node^.count);
  Node^.index := Index;
  Node^.deck_size := Node^.count;
  Result := CLM_BPA_SUCCESS;
end;

function clm_bpaa_append_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin
  Node := clm_bpaa_find_array(Id);
  if (Node = nil) or (Node^.count > High(clm_int)) then Exit(CLM_BPA_FAILURE);
  Result := clm_bpaa_insert_raw(Id, Node^.count, Buffer, BufferSize);
end;

function clm_bpaa_append_from(Id: clm_id; Source: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_append_raw(Id, Source, Size); end;
function clm_bpaa_insert_from(Id: clm_id; Index: clm_int; Source: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_insert_raw(Id, Index, Source, Size); end;

function clm_bpaa_get_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin
  Node := clm_bpaa_find_array(Id);
  if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Index < 0) or (clm_uint(Index) >= Node^.count) or (Buffer = nil) or (BufferSize < Node^.block_size) then Exit(CLM_BPA_FAILURE);
  Move((Node^.blocks + SizeUInt(Index) * Node^.block_size)^, Buffer^, Node^.block_size);
  Result := CLM_BPA_GET_SUCCESS;
end;

function clm_bpaa_set_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin
  Node := clm_bpaa_find_array(Id);
  if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Index < 0) or (clm_uint(Index) >= Node^.count) or (Buffer = nil) or (BufferSize <> Node^.block_size) then Exit(CLM_BPA_FAILURE);
  Move(Buffer^, (Node^.blocks + SizeUInt(Index) * Node^.block_size)^, Node^.block_size);
  Node^.deck_size := Node^.count;
  Result := CLM_BPA_SET_SUCCESS;
end;

function clm_bpaa_get_string_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Node: pclm_bpaa_array_node; Len: SizeUInt;
begin
  Node := clm_bpaa_find_array(Id);
  if (Node = nil) or (Node^.kind <> CLM_BPA_TYPE_STRING) or (Index < 0) or (clm_uint(Index) >= Node^.count) or (Buffer = nil) then Exit(CLM_BPA_FAILURE);
  Len := StrLen(Node^.strings[Index]);
  if (BufferSize <= Len) or (Len > High(LongInt)) then Exit(CLM_BPA_FAILURE);
  Move(Node^.strings[Index]^, Buffer^, Len + 1);
  Result := Len;
end;

function clm_bpaa_set_string_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Node: pclm_bpaa_array_node; Len: clm_uint; Copy: Pointer;
begin
  Node := clm_bpaa_find_array(Id);
  if (Node = nil) or (Node^.kind <> CLM_BPA_TYPE_STRING) or (Index < 0) or (clm_uint(Index) >= Node^.count) or not BoundedLength(Buffer, BufferSize, Len) or (Len > High(LongInt)) then Exit(CLM_BPA_FAILURE);
  GetMem(Copy, Len + 1);
  if Len > 0 then Move(Buffer^, Copy^, Len);
  PAnsiChar(Copy)[Len] := #0;
  clm_bpa_pool_free(Node^.strings[Index], StrLen(Node^.strings[Index]) + 1);
  Node^.strings[Index] := Copy;
  Node^.deck_size := Node^.count;
  Result := Len;
end;

function clm_bpaa_get_first_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
begin Result := clm_bpaa_get_raw(Id, 0, Buffer, BufferSize); end;
function clm_bpaa_get_last_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.count = 0) then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_get_raw(Id, Node^.count - 1, Buffer, BufferSize); end;
function clm_bpaa_get_current_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_get_raw(Id, Node^.index, Buffer, BufferSize); end;
function clm_bpaa_get_to(Id: clm_id; Index: clm_int; Destination: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_get_raw(Id, Index, Destination, Size); end;
function clm_bpaa_get_first_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_get_first_raw(Id, Destination, Size); end;
function clm_bpaa_get_last_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_get_last_raw(Id, Destination, Size); end;
function clm_bpaa_get_current_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_get_current_raw(Id, Destination, Size); end;
function clm_bpaa_get_string_to(Id: clm_id; Index: clm_int; Destination: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_get_string_raw(Id, Index, Destination, Size); end;
function clm_bpaa_set_from(Id: clm_id; Index: clm_int; Source: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_set_raw(Id, Index, Source, Size); end;
function clm_bpaa_set_string_from(Id: clm_id; Index: clm_int; Source: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_set_string_raw(Id, Index, Source, Size); end;
function clm_bpaa_draw_to(Id: clm_id; Destination: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_draw_raw(Id, Destination, Size); end;
function clm_bpaa_draw_at_to(Id: clm_id; Index: clm_int; Destination: Pointer; Size: clm_uint): LongInt;
begin Result := clm_bpaa_draw_at_raw(Id, Index, Destination, Size); end;
function clm_bpaa_get_count_to(Id: clm_id; out Count: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) then Exit(CLM_BPA_FAILURE); Count := Node^.count; Result := CLM_BPA_GET_SUCCESS; end;
function clm_bpaa_get_capacity_to(Id: clm_id; out Capacity: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) then Exit(CLM_BPA_FAILURE); Capacity := Node^.capacity; Result := CLM_BPA_GET_SUCCESS; end;
function clm_bpaa_get_length_to(Id: clm_id; out Length: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind <> CLM_BPA_TYPE_STRING) or (Node^.index >= Node^.count) then Exit(CLM_BPA_FAILURE); Length := StrLen(Node^.strings[Node^.index]); Result := CLM_BPA_GET_SUCCESS; end;
function clm_bpaa_get_current_index_to(Id: clm_id; out Index: clm_int): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if Node = nil then Exit(CLM_BPA_FAILURE); Index := Node^.index; Result := CLM_BPA_GET_SUCCESS; end;
function clm_bpaa_is_sealed_to(Id: clm_id; out Sealed: clm_byte): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if Node = nil then Exit(CLM_BPA_FAILURE); Sealed := Node^.sealed; Result := CLM_BPA_GET_SUCCESS; end;

function clm_bpaa_extend(Id: clm_id; AdditionalCapacity: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.sealed <> 0) or (AdditionalCapacity = 0) or (AdditionalCapacity > High(clm_uint) - Node^.capacity) then Exit(CLM_BPA_FAILURE); Result := ArrayResize(Node, Node^.capacity + AdditionalCapacity); end;
function clm_bpaa_shrink(Id: clm_id; NewCapacity: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.sealed <> 0) or (NewCapacity >= Node^.capacity) or (NewCapacity < Node^.count) then Exit(CLM_BPA_FAILURE); Result := ArrayResize(Node, NewCapacity); end;

function clm_bpaa_remove(Id: clm_id; Index: clm_int): LongInt;
var Node: pclm_bpaa_array_node; I: clm_uint;
begin
  Node := clm_bpaa_find_array(Id);
  if (Node = nil) or (Index < 0) or (clm_uint(Index) >= Node^.count) then Exit(CLM_BPA_FAILURE);
  if Node^.kind = CLM_BPA_TYPE_STRING then
  begin
    clm_bpa_pool_free(Node^.strings[Index], StrLen(Node^.strings[Index]) + 1);
    for I := Index to Node^.count - 2 do Node^.strings[I] := Node^.strings[I + 1];
    Node^.strings[Node^.count - 1] := nil;
  end
  else
    Move((Node^.blocks + (SizeUInt(Index) + 1) * Node^.block_size)^, (Node^.blocks + SizeUInt(Index) * Node^.block_size)^, SizeUInt(Node^.count - clm_uint(Index) - 1) * Node^.block_size);
  Dec(Node^.count);
  if Node^.count = 0 then Node^.index := 0 else if Node^.index > clm_uint(Index) then Dec(Node^.index) else if Node^.index >= Node^.count then Node^.index := Node^.count - 1;
  Node^.deck_size := Node^.count;
  Result := CLM_BPA_SUCCESS;
end;
function clm_bpaa_remove_first(Id: clm_id): LongInt; begin Result := clm_bpaa_remove(Id, 0); end;
function clm_bpaa_remove_last(Id: clm_id): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.count = 0) then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_remove(Id, Node^.count - 1); end;
function clm_bpaa_clear(Id: clm_id): LongInt;
var Node: pclm_bpaa_array_node; I: clm_uint;
begin
  Node := clm_bpaa_find_array(Id); if Node = nil then Exit(CLM_BPA_FAILURE);
  if Node^.kind = CLM_BPA_TYPE_STRING then for I := 0 to Node^.count - 1 do begin clm_bpa_pool_free(Node^.strings[I], StrLen(Node^.strings[I]) + 1); Node^.strings[I] := nil; end
  else if (Node^.blocks <> nil) and (Node^.capacity > 0) then FillChar(Node^.blocks^, SizeUInt(Node^.capacity) * Node^.block_size, 0);
  Node^.count := 0; Node^.index := 0; Node^.deck_size := 0; Result := CLM_BPA_SUCCESS;
end;

function clm_bpaa_advanced_borrow(Id: clm_id; out ArrayNode: pclm_bpaa_array_node): LongInt;
begin ArrayNode := clm_bpaa_find_array(Id); if (ArrayNode = nil) or (ArrayNode^.refcount = High(clm_ushort)) then begin ArrayNode := nil; Exit(CLM_BPA_FAILURE); end; Inc(ArrayNode^.refcount); Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_advanced_release(var ArrayNode: pclm_bpaa_array_node): LongInt;
var Id: clm_id;
begin if ArrayNode = nil then Exit(CLM_BPA_FAILURE); Id := ArrayNode^.id; Dec(ArrayNode^.refcount); ArrayNode := nil; if clm_bpaa_find_array(Id) <> nil then begin if clm_bpaa_find_array(Id)^.refcount = 0 then Result := clm_bpaa_delete(Id) else Result := CLM_BPA_SUCCESS; end else Result := CLM_BPA_FAILURE; end;
function clm_bpaa_advanced_data(ArrayNode: pclm_bpaa_array_node): Pointer;
begin if (ArrayNode = nil) or (ArrayNode^.kind = CLM_BPA_TYPE_STRING) then Result := nil else Result := ArrayNode^.blocks; end;
function clm_bpaa_advanced_get_raw(ArrayNode: pclm_bpaa_array_node; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
begin if ArrayNode = nil then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_get_raw(ArrayNode^.id, Index, Buffer, BufferSize); end;
function clm_bpaa_advanced_set_raw(ArrayNode: pclm_bpaa_array_node; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
begin if ArrayNode = nil then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_set_raw(ArrayNode^.id, Index, Buffer, BufferSize); end;
function clm_bpaa_advanced_insert_raw(ArrayNode: pclm_bpaa_array_node; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
begin if ArrayNode = nil then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_insert_raw(ArrayNode^.id, Index, Buffer, BufferSize); end;
function clm_bpaa_advanced_append_raw(ArrayNode: pclm_bpaa_array_node; Buffer: Pointer; BufferSize: clm_uint): LongInt;
begin if ArrayNode = nil then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_append_raw(ArrayNode^.id, Buffer, BufferSize); end;
function clm_bpaa_advanced_remove(ArrayNode: pclm_bpaa_array_node; Index: clm_int): LongInt;
begin if ArrayNode = nil then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_remove(ArrayNode^.id, Index); end;
function clm_bpaa_advanced_clear(ArrayNode: pclm_bpaa_array_node): LongInt;
begin if ArrayNode = nil then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_clear(ArrayNode^.id); end;
function clm_bpaa_advanced_extend(ArrayNode: pclm_bpaa_array_node; AdditionalCapacity: clm_uint): LongInt;
begin if ArrayNode = nil then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_extend(ArrayNode^.id, AdditionalCapacity); end;
function clm_bpaa_advanced_shrink(ArrayNode: pclm_bpaa_array_node; NewCapacity: clm_uint): LongInt;
begin if ArrayNode = nil then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_shrink(ArrayNode^.id, NewCapacity); end;

function clm_bpaa_slice(SourceId: clm_id; Start, Count: clm_int; DestinationId: clm_id): LongInt;
var Source, Destination: pclm_bpaa_array_node; I: clm_int; Buffer: Pointer;
begin
  Source := clm_bpaa_find_array(SourceId);
  if (Source = nil) or (Start < 0) or (Count < 0) or (clm_uint(Start) > Source^.count) or (clm_uint(Count) > Source^.count - clm_uint(Start)) or clm_bpaa_is_existing(DestinationId) then Exit(CLM_BPA_FAILURE);
  if clm_bpaa_create_sized(DestinationId, Source^.kind, Count, IfThen(Source^.kind = CLM_BPA_TYPE_STRUCT, Source^.block_size, 0), Source^.encoding) <> CLM_BPA_SUCCESS then Exit(CLM_BPA_FAILURE);
  Destination := clm_bpaa_find_array(DestinationId);
  for I := 0 to Count - 1 do
    if Source^.kind = CLM_BPA_TYPE_STRING then begin if clm_bpaa_append_raw(DestinationId, Source^.strings[Start + I], StrLen(Source^.strings[Start + I]) + 1) <> CLM_BPA_SUCCESS then begin clm_bpaa_delete(DestinationId); Exit(CLM_BPA_FAILURE); end; end
    else begin Buffer := Source^.blocks + SizeUInt(Start + I) * Source^.block_size; if clm_bpaa_append_raw(DestinationId, Buffer, Source^.block_size) <> CLM_BPA_SUCCESS then begin clm_bpaa_delete(DestinationId); Exit(CLM_BPA_FAILURE); end; end;
  Destination^.deck_size := Destination^.count; Result := CLM_BPA_SUCCESS;
end;
function clm_bpaa_slice_borrow(SourceId: clm_id; Start, Count: clm_int; out Buffer: Pointer; out BufferSize: SizeUInt): LongInt;
var Node: pclm_bpaa_array_node;
begin Buffer := nil; BufferSize := 0; Node := clm_bpaa_find_array(SourceId); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Start < 0) or (Count < 0) or (clm_uint(Start) > Node^.count) or (clm_uint(Count) > Node^.count - clm_uint(Start)) then Exit(CLM_BPA_FAILURE); if Count > 0 then begin Buffer := Node^.blocks + SizeUInt(Start) * Node^.block_size; BufferSize := SizeUInt(Count) * Node^.block_size; end; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_slice_to(SourceId: clm_id; Start, Count: clm_int; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
var Node: pclm_bpaa_array_node; Bytes: SizeUInt;
begin Node := clm_bpaa_find_array(SourceId); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Start < 0) or (Count < 0) or (clm_uint(Start) > Node^.count) or (clm_uint(Count) > Node^.count - clm_uint(Start)) then Exit(CLM_BPA_FAILURE); if Count = 0 then Exit(CLM_BPA_SUCCESS); Bytes := SizeUInt(Count) * Node^.block_size; if (Buffer = nil) or (BufferSize < Bytes) then Exit(CLM_BPA_FAILURE); Move((Node^.blocks + SizeUInt(Start) * Node^.block_size)^, Buffer^, Bytes); Result := CLM_BPA_SUCCESS; end;

function clm_bpaa_reset(Id: clm_id): LongInt; var Node: pclm_bpaa_array_node; begin Node := clm_bpaa_find_array(Id); if Node = nil then Exit(CLM_BPA_FAILURE); Node^.index := 0; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_move_first(Id: clm_id): LongInt; begin Result := clm_bpaa_reset(Id); end;
function clm_bpaa_move_previous(Id: clm_id): LongInt; var Node: pclm_bpaa_array_node; begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.index = 0) then Exit(CLM_BPA_FAILURE); Dec(Node^.index); Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_move_next(Id: clm_id): LongInt; var Node: pclm_bpaa_array_node; begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.index + 1 >= Node^.count) then Exit(CLM_BPA_FAILURE); Inc(Node^.index); Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_move_last(Id: clm_id): LongInt; var Node: pclm_bpaa_array_node; begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.count = 0) then Exit(CLM_BPA_FAILURE); Node^.index := Node^.count - 1; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_move_to(Id: clm_id; Index: clm_int): LongInt; var Node: pclm_bpaa_array_node; begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Index < 0) or (clm_uint(Index) >= Node^.count) then Exit(CLM_BPA_FAILURE); Node^.index := Index; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_seal(Id: clm_id): LongInt; var Node: pclm_bpaa_array_node; begin Node := clm_bpaa_find_array(Id); if Node = nil then Exit(CLM_BPA_FAILURE); Node^.sealed := 1; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_unseal(Id: clm_id): LongInt; var Node: pclm_bpaa_array_node; begin Node := clm_bpaa_find_array(Id); if Node = nil then Exit(CLM_BPA_FAILURE); Node^.sealed := 0; Result := CLM_BPA_SUCCESS; end;

function clm_bpaa_swap(Id: clm_id; Index1, Index2: clm_int): LongInt;
var Node: pclm_bpaa_array_node; Temp: Pointer;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Index1 < 0) or (Index2 < 0) or (clm_uint(Index1) >= Node^.count) or (clm_uint(Index2) >= Node^.count) then Exit(CLM_BPA_FAILURE); if Index1 = Index2 then Exit(CLM_BPA_SUCCESS); GetMem(Temp, Node^.block_size); Move((Node^.blocks + SizeUInt(Index1) * Node^.block_size)^, Temp^, Node^.block_size); Move((Node^.blocks + SizeUInt(Index2) * Node^.block_size)^, (Node^.blocks + SizeUInt(Index1) * Node^.block_size)^, Node^.block_size); Move(Temp^, (Node^.blocks + SizeUInt(Index2) * Node^.block_size)^, Node^.block_size); FreeMem(Temp); Node^.deck_size := Node^.count; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_copy(Id: clm_id; SourceIndex, DestinationIndex: clm_int): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (SourceIndex < 0) or (DestinationIndex < 0) or (clm_uint(SourceIndex) >= Node^.count) or (clm_uint(DestinationIndex) >= Node^.count) then Exit(CLM_BPA_FAILURE); Move((Node^.blocks + SizeUInt(SourceIndex) * Node^.block_size)^, (Node^.blocks + SizeUInt(DestinationIndex) * Node^.block_size)^, Node^.block_size); Node^.deck_size := Node^.count; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_reverse(Id: clm_id): LongInt;
var Node: pclm_bpaa_array_node; L, R: clm_uint;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) then Exit(CLM_BPA_FAILURE); if Node^.count > 1 then begin L := 0; R := Node^.count - 1; while L < R do begin clm_bpaa_swap(Id, L, R); Inc(L); Dec(R); end; end; Node^.index := 0; Node^.deck_size := Node^.count; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_shift_left(Id: clm_id; Index, Positions: clm_uint): LongInt;
var Node: pclm_bpaa_array_node; LengthValue, Shift: clm_uint; Temp: Pointer; Bytes: SizeUInt;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Index >= Node^.count) then Exit(CLM_BPA_FAILURE); LengthValue := Node^.count - Index; Shift := Positions mod LengthValue; if Shift = 0 then Exit(CLM_BPA_SUCCESS); Bytes := SizeUInt(Shift) * Node^.block_size; GetMem(Temp, Bytes); Move((Node^.blocks + SizeUInt(Index) * Node^.block_size)^, Temp^, Bytes); Move((Node^.blocks + SizeUInt(Index + Shift) * Node^.block_size)^, (Node^.blocks + SizeUInt(Index) * Node^.block_size)^, SizeUInt(LengthValue - Shift) * Node^.block_size); Move(Temp^, (Node^.blocks + SizeUInt(Index + LengthValue - Shift) * Node^.block_size)^, Bytes); FreeMem(Temp); Node^.index := 0; Node^.deck_size := Node^.count; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_shift_right(Id: clm_id; Index, Positions: clm_uint): LongInt;
var Node: pclm_bpaa_array_node; LengthValue, Shift: clm_uint; Temp: Pointer; Bytes: SizeUInt;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Index >= Node^.count) then Exit(CLM_BPA_FAILURE); LengthValue := Node^.count - Index; Shift := Positions mod LengthValue; if Shift = 0 then Exit(CLM_BPA_SUCCESS); Bytes := SizeUInt(Shift) * Node^.block_size; GetMem(Temp, Bytes); Move((Node^.blocks + SizeUInt(Index + LengthValue - Shift) * Node^.block_size)^, Temp^, Bytes); Move((Node^.blocks + SizeUInt(Index) * Node^.block_size)^, (Node^.blocks + SizeUInt(Index + Shift) * Node^.block_size)^, SizeUInt(LengthValue - Shift) * Node^.block_size); Move(Temp^, (Node^.blocks + SizeUInt(Index) * Node^.block_size)^, Bytes); FreeMem(Temp); Node^.index := 0; Node^.deck_size := Node^.count; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_shuffle(Id: clm_id): LongInt;
var Node: pclm_bpaa_array_node; I: clm_uint;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) then Exit(CLM_BPA_FAILURE); for I := Node^.count downto 2 do clm_bpaa_swap(Id, I - 1, Random(I)); Node^.index := 0; Node^.deck_size := Node^.count; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_sort_with(Id: clm_id; Compare: clm_bpaa_compare_fn): LongInt;
var Node: pclm_bpaa_array_node; I, J: clm_uint;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or not Assigned(Compare) then Exit(CLM_BPA_FAILURE); for I := 0 to Node^.count - 2 do for J := 0 to Node^.count - I - 2 do if Compare(Node^.blocks + SizeUInt(J) * Node^.block_size, Node^.blocks + SizeUInt(J + 1) * Node^.block_size) > 0 then clm_bpaa_swap(Id, J, J + 1); Node^.index := 0; Node^.deck_size := Node^.count; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_sort(Id: clm_id; Predicate: clm_bpaa_predicate_fn): LongInt;
var Node: pclm_bpaa_array_node; I, J: clm_uint;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or not Assigned(Predicate) then Exit(CLM_BPA_FAILURE); for I := 0 to Node^.count - 2 do for J := 0 to Node^.count - I - 2 do if Predicate(Node^.blocks + SizeUInt(J) * Node^.block_size, Node^.blocks + SizeUInt(J + 1) * Node^.block_size) > 0 then clm_bpaa_swap(Id, J, J + 1); Node^.index := 0; Node^.deck_size := Node^.count; Result := CLM_BPA_SUCCESS; end;
function clm_bpaa_draw_at_raw(Id: clm_id; Index: clm_int; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Node: pclm_bpaa_array_node; Last: clm_uint;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.kind = CLM_BPA_TYPE_STRING) or (Buffer = nil) or (BufferSize < Node^.block_size) or (Index < 0) or (clm_uint(Index) >= Node^.deck_size) then Exit(CLM_BPA_FAILURE); Last := Node^.deck_size - 1; if clm_uint(Index) <> Last then clm_bpaa_swap(Id, Index, Last); Move((Node^.blocks + SizeUInt(Last) * Node^.block_size)^, Buffer^, Node^.block_size); Dec(Node^.deck_size); Result := CLM_BPA_GET_SUCCESS; end;
function clm_bpaa_draw_raw(Id: clm_id; Buffer: Pointer; BufferSize: clm_uint): LongInt;
var Node: pclm_bpaa_array_node;
begin Node := clm_bpaa_find_array(Id); if (Node = nil) or (Node^.count = 0) then Exit(CLM_BPA_FAILURE); if Node^.deck_size = 0 then clm_bpaa_shuffle(Id); Result := clm_bpaa_draw_at_raw(Id, 0, Buffer, BufferSize); end;

function clm_bpaa_get(Id: clm_id; Index: clm_int; BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
var A: pclm_bpaa_array_node; B: pclm_bpa_block_node;
begin A := clm_bpaa_find_array(Id); B := clm_bpa_find_block(BlockId); if (A = nil) or (B = nil) or (Index < 0) or (clm_uint(Index) >= A^.count) or (A^.kind = CLM_BPA_TYPE_STRING) or (B^.kind = CLM_BPA_TYPE_STRING) or (A^.block_size <> B^.block_size) or (ExpectedSize <> A^.block_size) or ((A^.kind <> CLM_BPA_TYPE_STRUCT) and (A^.kind <> B^.kind)) then Exit(CLM_BPA_FAILURE); Move((A^.blocks + SizeUInt(Index) * A^.block_size)^, B^.data^, A^.block_size); Result := CLM_BPA_GET_SUCCESS; end;
function clm_bpaa_set(Id: clm_id; Index: clm_int; BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
var A: pclm_bpaa_array_node; B: pclm_bpa_block_node;
begin A := clm_bpaa_find_array(Id); B := clm_bpa_find_block(BlockId); if (A = nil) or (B = nil) or (Index < 0) or (clm_uint(Index) >= A^.count) or (A^.kind = CLM_BPA_TYPE_STRING) or (B^.kind = CLM_BPA_TYPE_STRING) or (A^.block_size <> B^.block_size) or (ExpectedSize <> A^.block_size) or ((A^.kind <> CLM_BPA_TYPE_STRUCT) and (A^.kind <> B^.kind)) then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_set_raw(Id, Index, B^.data, A^.block_size); end;
function clm_bpaa_get_string(Id: clm_id; Index: clm_int; BlockId: clm_id): LongInt;
var A: pclm_bpaa_array_node; B: pclm_bpa_block_node;
begin A := clm_bpaa_find_array(Id); B := clm_bpa_find_block(BlockId); if (A = nil) or (B = nil) or (A^.kind <> CLM_BPA_TYPE_STRING) or (B^.kind <> CLM_BPA_TYPE_STRING) or (Index < 0) or (clm_uint(Index) >= A^.count) then Exit(CLM_BPA_FAILURE); Result := clm_bpa_set_string(BlockId, A^.strings[Index], StrLen(A^.strings[Index]) + 1); if Result >= 0 then Result := CLM_BPA_GET_SUCCESS; end;
function clm_bpaa_set_string(Id: clm_id; Index: clm_int; BlockId: clm_id): LongInt;
var B: pclm_bpa_block_node;
begin B := clm_bpa_find_block(BlockId); if (B = nil) or (B^.kind <> CLM_BPA_TYPE_STRING) then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_set_string_raw(Id, Index, B^.data, B^.length + 1); if Result >= 0 then Result := CLM_BPA_SET_SUCCESS; end;
function clm_bpaa_insert_string(Id: clm_id; Index: clm_int; BlockId: clm_id): LongInt;
var B: pclm_bpa_block_node;
begin B := clm_bpa_find_block(BlockId); if (B = nil) or (B^.kind <> CLM_BPA_TYPE_STRING) then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_insert_raw(Id, Index, B^.data, B^.length + 1); end;
function clm_bpaa_append_string(Id, BlockId: clm_id): LongInt;
var A: pclm_bpaa_array_node;
begin A := clm_bpaa_find_array(Id); if A = nil then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_insert_string(Id, A^.count, BlockId); end;

function clm_bpaa_insert(Id: clm_id; Index: clm_int; BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
var A: pclm_bpaa_array_node; B: pclm_bpa_block_node;
begin
  A := clm_bpaa_find_array(Id); B := clm_bpa_find_block(BlockId);
  if (A = nil) or (B = nil) or (A^.kind = CLM_BPA_TYPE_STRING) or (B^.kind = CLM_BPA_TYPE_STRING) or
     ((A^.kind <> CLM_BPA_TYPE_STRUCT) and (A^.kind <> B^.kind)) or
     (A^.block_size <> B^.block_size) or (ExpectedSize <> A^.block_size) then Exit(CLM_BPA_FAILURE);
  Result := clm_bpaa_insert_raw(Id, Index, B^.data, B^.block_size);
end;

function clm_bpaa_append(Id, BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
var A: pclm_bpaa_array_node;
begin A := clm_bpaa_find_array(Id); if (A = nil) or (A^.count > High(clm_int)) then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_insert(Id, A^.count, BlockId, ExpectedSize); end;

function clm_bpaa_draw(Id, BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
var A: pclm_bpaa_array_node; B: pclm_bpa_block_node;
begin
  A := clm_bpaa_find_array(Id); B := clm_bpa_find_block(BlockId);
  if (A = nil) or (B = nil) or (A^.kind = CLM_BPA_TYPE_STRING) or (B^.kind = CLM_BPA_TYPE_STRING) or
     ((A^.kind <> CLM_BPA_TYPE_STRUCT) and (A^.kind <> B^.kind)) or (A^.block_size <> B^.block_size) or
     (ExpectedSize <> A^.block_size) then Exit(CLM_BPA_FAILURE);
  Result := clm_bpaa_draw_raw(Id, B^.data, B^.block_size);
end;

function clm_bpaa_draw_at(Id: clm_id; Index: clm_int; BlockId: clm_id; ExpectedSize: SizeUInt): LongInt;
var A: pclm_bpaa_array_node; B: pclm_bpa_block_node;
begin
  A := clm_bpaa_find_array(Id); B := clm_bpa_find_block(BlockId);
  if (A = nil) or (B = nil) or (A^.kind = CLM_BPA_TYPE_STRING) or (B^.kind = CLM_BPA_TYPE_STRING) or
     ((A^.kind <> CLM_BPA_TYPE_STRUCT) and (A^.kind <> B^.kind)) or (A^.block_size <> B^.block_size) or
     (ExpectedSize <> A^.block_size) then Exit(CLM_BPA_FAILURE);
  Result := clm_bpaa_draw_at_raw(Id, Index, B^.data, B^.block_size);
end;

function clm_bpaa_write_u32(DestinationId: clm_id; Value: clm_uint): LongInt;
var B: pclm_bpa_block_node;
begin B := clm_bpa_find_block(DestinationId); if (B = nil) or (B^.kind <> CLM_BPA_TYPE_UNSIGNED_INT) then Exit(CLM_BPA_FAILURE); Move(Value, B^.data^, SizeOf(Value)); Result := CLM_BPA_GET_SUCCESS; end;
function clm_bpaa_write_i32(DestinationId: clm_id; Value: clm_int): LongInt;
var B: pclm_bpa_block_node;
begin B := clm_bpa_find_block(DestinationId); if (B = nil) or (B^.kind <> CLM_BPA_TYPE_INT) then Exit(CLM_BPA_FAILURE); Move(Value, B^.data^, SizeOf(Value)); Result := CLM_BPA_GET_SUCCESS; end;
function clm_bpaa_write_byte(DestinationId: clm_id; Value: clm_byte): LongInt;
var B: pclm_bpa_block_node;
begin B := clm_bpa_find_block(DestinationId); if (B = nil) or (B^.kind <> CLM_BPA_TYPE_BYTE) then Exit(CLM_BPA_FAILURE); Move(Value, B^.data^, SizeOf(Value)); Result := CLM_BPA_GET_SUCCESS; end;

function clm_bpaa_get_count(Id, DestinationId: clm_id): LongInt;
var V: clm_uint;
begin if clm_bpaa_get_count_to(Id, V) <> CLM_BPA_GET_SUCCESS then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_write_u32(DestinationId, V); end;
function clm_bpaa_get_capacity(Id, DestinationId: clm_id): LongInt;
var V: clm_uint;
begin if clm_bpaa_get_capacity_to(Id, V) <> CLM_BPA_GET_SUCCESS then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_write_u32(DestinationId, V); end;
function clm_bpaa_get_length(Id, DestinationId: clm_id): LongInt;
var V: clm_uint;
begin if clm_bpaa_get_length_to(Id, V) <> CLM_BPA_GET_SUCCESS then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_write_u32(DestinationId, V); end;
function clm_bpaa_get_current_index(Id, DestinationId: clm_id): LongInt;
var V: clm_int;
begin if clm_bpaa_get_current_index_to(Id, V) <> CLM_BPA_GET_SUCCESS then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_write_i32(DestinationId, V); end;
function clm_bpaa_is_sealed(Id, DestinationId: clm_id): LongInt;
var V: clm_byte;
begin if clm_bpaa_is_sealed_to(Id, V) <> CLM_BPA_GET_SUCCESS then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_write_byte(DestinationId, V); end;
function clm_bpaa_get_first(Id, DestinationId: clm_id; ExpectedSize: SizeUInt): LongInt;
begin Result := clm_bpaa_get(Id, 0, DestinationId, ExpectedSize); end;
function clm_bpaa_get_last(Id, DestinationId: clm_id; ExpectedSize: SizeUInt): LongInt;
var A: pclm_bpaa_array_node;
begin A := clm_bpaa_find_array(Id); if (A = nil) or (A^.count = 0) then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_get(Id, A^.count - 1, DestinationId, ExpectedSize); end;
function clm_bpaa_get_current(Id, DestinationId: clm_id; ExpectedSize: SizeUInt): LongInt;
var A: pclm_bpaa_array_node;
begin A := clm_bpaa_find_array(Id); if (A = nil) or (A^.index >= A^.count) then Exit(CLM_BPA_FAILURE); Result := clm_bpaa_get(Id, A^.index, DestinationId, ExpectedSize); end;

function clm_bpa_shutdown: LongInt;
var I: SizeInt;
begin
  for I := 0 to High(Arrays) do if Arrays[I] <> nil then clm_bpaa_delete(Arrays[I]^.id);
  for I := 0 to High(Blocks) do if Blocks[I] <> nil then clm_bpa_delete(Blocks[I]^.id);
  SetLength(Arrays, 0); SetLength(Blocks, 0);
  Result := clm_bpa_pool_cleanup;
end;

initialization
  Randomize;

end.