unit storage;

{$mode objfpc}{$H+}

interface

uses
  base;

type
  clm_storage_info = record
    columns, rows, layers: clm_uint;
    frame_columns, frame_rows: clm_uint;
    element_size: clm_uint;
    kind, atlas: clm_byte;
    data_id: clm_id; { Storage-owned ID, not a BPA/BPAA block or array ID. }
  end;

function clm_storage_new(Id: clm_id; Kind: clm_byte; StructSize: SizeUInt; Columns, Rows, Layers: clm_uint): LongInt;
function clm_storage_new_2d(Id: clm_id; Kind: clm_byte; StructSize: SizeUInt; Columns, Rows: clm_uint): LongInt;
function clm_storage_atlas_new(Id: clm_id; Kind: clm_byte; StructSize: SizeUInt; Columns, Rows, FrameColumns, FrameRows: clm_uint): LongInt;
function clm_storage_atlas_new_3d(Id: clm_id; Kind: clm_byte; StructSize: SizeUInt; Columns, Rows, Layers, FrameColumns, FrameRows: clm_uint): LongInt;
function clm_storage_get_info(Id: clm_id; out Info: clm_storage_info): LongInt;
function clm_storage_delete(Id: clm_id): LongInt;
function clm_storage_get_to(Id: clm_id; Layer, Row, Column: clm_uint; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
function clm_storage_set_from(Id: clm_id; Layer, Row, Column: clm_uint; Value: Pointer; ValueSize: SizeUInt): LongInt;
function clm_storage_get(Id: clm_id; Layer, Row, Column: clm_uint; DestinationBlockId: clm_id): LongInt;
function clm_storage_set(Id: clm_id; Layer, Row, Column: clm_uint; SourceBlockId: clm_id): LongInt;
function clm_storage_fill_from(Id: clm_id; Value: Pointer; ValueSize: SizeUInt): LongInt;
function clm_storage_slice(SourceId: clm_id; Layer, Row, Column, Layers, Rows, Columns: clm_uint; DestinationId: clm_id): LongInt;
function clm_storage_atlas_get_frame_to(Id: clm_id; Layer, FrameRow, FrameColumn: clm_uint; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
function clm_storage_atlas_set_frame_from(Id: clm_id; Layer, FrameRow, FrameColumn: clm_uint; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
function clm_storage_atlas_slice_frame(Id: clm_id; Layer, FrameRow, FrameColumn: clm_uint; DestinationId: clm_id): LongInt;

implementation

uses
  bpa;

type
  TStorageBytes = array of Byte;
  TStorage = record
    Info: clm_storage_info;
    Data: TStorageBytes;
  end;

var
  Storages: array of TStorage;

function FindStorage(Id: clm_id): SizeInt;
var I: SizeInt;
begin
  for I := 0 to High(Storages) do if Storages[I].Info.data_id = Id then Exit(I);
  Result := -1;
end;

function ElementSize(Kind: clm_byte; StructSize: SizeUInt): clm_uint;
begin
  if Kind = CLM_BPA_TYPE_STRUCT then begin
    if (StructSize = 0) or (StructSize > High(clm_uint)) then Exit(0);
    Exit(StructSize);
  end;
  if Kind = CLM_BPA_TYPE_STRING then Exit(0);
  if (StructSize <> 0) and (StructSize <> clm_bpa_type_size(Kind)) then Exit(0);
  Result := clm_bpa_type_size(Kind);
end;

function CreateStorage(Id: clm_id; Kind: clm_byte; StructSize: SizeUInt; Columns, Rows, Layers, FrameColumns, FrameRows: clm_uint; IsAtlas: Boolean): LongInt;
var ItemSize: clm_uint; Total: QWord; I: SizeInt;
begin
  if (FindStorage(Id) >= 0) or clm_bpa_is_existing(Id) or clm_bpaa_is_existing(Id) or (Columns = 0) or (Rows = 0) or (Layers = 0) then Exit(CLM_BPA_FAILURE);
  if IsAtlas then begin
    if (FrameColumns = 0) or (FrameRows = 0) or (Columns mod FrameColumns <> 0) or (Rows mod FrameRows <> 0) then Exit(CLM_BPA_FAILURE);
  end else if (FrameColumns <> 0) or (FrameRows <> 0) then Exit(CLM_BPA_FAILURE);
  ItemSize := ElementSize(Kind, StructSize);
  if ItemSize = 0 then Exit(CLM_BPA_FAILURE);
  Total := QWord(Columns) * Rows * Layers;
  if (Total = 0) or (Total > High(LongInt)) or
     (Total > QWord(High(SizeInt)) div ItemSize) then Exit(CLM_BPA_FAILURE);
  I := Length(Storages);
  SetLength(Storages, I + 1);
  Storages[I].Info.columns := Columns;
  Storages[I].Info.rows := Rows;
  Storages[I].Info.layers := Layers;
  Storages[I].Info.frame_columns := FrameColumns;
  Storages[I].Info.frame_rows := FrameRows;
  Storages[I].Info.element_size := ItemSize;
  Storages[I].Info.kind := Kind;
  Storages[I].Info.atlas := Ord(IsAtlas);
  Storages[I].Info.data_id := Id;
  SetLength(Storages[I].Data, SizeInt(Total * ItemSize));
  FillChar(Storages[I].Data[0], Length(Storages[I].Data), 0);
  Result := CLM_BPA_SUCCESS;
end;

function clm_storage_new(Id: clm_id; Kind: clm_byte; StructSize: SizeUInt; Columns, Rows, Layers: clm_uint): LongInt;
begin Result := CreateStorage(Id, Kind, StructSize, Columns, Rows, Layers, 0, 0, False); end;
function clm_storage_new_2d(Id: clm_id; Kind: clm_byte; StructSize: SizeUInt; Columns, Rows: clm_uint): LongInt;
begin Result := clm_storage_new(Id, Kind, StructSize, Columns, Rows, 1); end;
function clm_storage_atlas_new(Id: clm_id; Kind: clm_byte; StructSize: SizeUInt; Columns, Rows, FrameColumns, FrameRows: clm_uint): LongInt;
begin Result := CreateStorage(Id, Kind, StructSize, Columns, Rows, 1, FrameColumns, FrameRows, True); end;
function clm_storage_atlas_new_3d(Id: clm_id; Kind: clm_byte; StructSize: SizeUInt; Columns, Rows, Layers, FrameColumns, FrameRows: clm_uint): LongInt;
begin Result := CreateStorage(Id, Kind, StructSize, Columns, Rows, Layers, FrameColumns, FrameRows, True); end;

function clm_storage_get_info(Id: clm_id; out Info: clm_storage_info): LongInt;
var I: SizeInt;
begin I := FindStorage(Id); if I < 0 then Exit(CLM_BPA_FAILURE); Info := Storages[I].Info; Result := CLM_BPA_GET_SUCCESS; end;
function clm_storage_delete(Id: clm_id): LongInt;
var I, Last: SizeInt;
begin I := FindStorage(Id); if I < 0 then Exit(CLM_BPA_FAILURE); Last := High(Storages); if I <> Last then Storages[I] := Storages[Last]; SetLength(Storages, Last); Result := CLM_BPA_SUCCESS; end;

function ElementIndex(const Info: clm_storage_info; Layer, Row, Column: clm_uint; out Index: SizeUInt): Boolean;
begin
  Result := (Layer < Info.layers) and (Row < Info.rows) and (Column < Info.columns);
  if Result then Index := (SizeUInt(Layer) * Info.rows + Row) * Info.columns + Column;
end;

function clm_storage_get_to(Id: clm_id; Layer, Row, Column: clm_uint; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
var I: SizeInt; Element: SizeUInt;
begin
  I := FindStorage(Id);
  if I < 0 then Exit(CLM_BPA_FAILURE);
  if (Buffer = nil) or (BufferSize < Storages[I].Info.element_size) then Exit(CLM_BPA_FAILURE);
  if not ElementIndex(Storages[I].Info, Layer, Row, Column, Element) then Exit(CLM_BPA_FAILURE);
  Move(Storages[I].Data[Element * Storages[I].Info.element_size], Buffer^, Storages[I].Info.element_size);
  Result := CLM_BPA_GET_SUCCESS;
end;

function clm_storage_set_from(Id: clm_id; Layer, Row, Column: clm_uint; Value: Pointer; ValueSize: SizeUInt): LongInt;
var I: SizeInt; Element: SizeUInt;
begin
  I := FindStorage(Id);
  if I < 0 then Exit(CLM_BPA_FAILURE);
  if (Value = nil) or (ValueSize <> Storages[I].Info.element_size) then Exit(CLM_BPA_FAILURE);
  if not ElementIndex(Storages[I].Info, Layer, Row, Column, Element) then Exit(CLM_BPA_FAILURE);
  Move(Value^, Storages[I].Data[Element * Storages[I].Info.element_size], ValueSize);
  Result := CLM_BPA_SET_SUCCESS;
end;

function clm_storage_get(Id: clm_id; Layer, Row, Column: clm_uint; DestinationBlockId: clm_id): LongInt;
var I: SizeInt; Block: pclm_bpa_block_node; Element: SizeUInt;
begin
  I := FindStorage(Id);
  if I < 0 then Exit(CLM_BPA_FAILURE);
  Block := clm_bpa_find_block(DestinationBlockId);
  if Block = nil then Exit(CLM_BPA_FAILURE);
  if (Block^.data = nil) or (Block^.block_size <> Storages[I].Info.element_size) then Exit(CLM_BPA_FAILURE);
  if not ElementIndex(Storages[I].Info, Layer, Row, Column, Element) then Exit(CLM_BPA_FAILURE);
  Move(Storages[I].Data[Element * Storages[I].Info.element_size], Block^.data^, Block^.block_size);
  Result := CLM_BPA_GET_SUCCESS;
end;

function clm_storage_set(Id: clm_id; Layer, Row, Column: clm_uint; SourceBlockId: clm_id): LongInt;
var Block: pclm_bpa_block_node;
begin
  Block := clm_bpa_find_block(SourceBlockId);
  if Block = nil then Exit(CLM_BPA_FAILURE);
  Result := clm_storage_set_from(Id, Layer, Row, Column, Block^.data, Block^.block_size);
end;

function clm_storage_fill_from(Id: clm_id; Value: Pointer; ValueSize: SizeUInt): LongInt;
var
  I: SizeInt;
  Offset, ItemSize: SizeUInt;
  Snapshot: array of Byte;
begin
  I := FindStorage(Id);
  if I < 0 then Exit(CLM_BPA_FAILURE);
  ItemSize := Storages[I].Info.element_size;
  if (Value = nil) or (ValueSize <> ItemSize) then Exit(CLM_BPA_FAILURE);
  SetLength(Snapshot, ItemSize);
  Move(Value^, Snapshot[0], ItemSize);
  Offset := 0;
  while Offset < SizeUInt(Length(Storages[I].Data)) do
  begin
    Move(Snapshot[0], Storages[I].Data[Offset], ItemSize);
    Inc(Offset, ItemSize);
  end;
  Result := CLM_BPA_SUCCESS;
end;

function clm_storage_slice(SourceId: clm_id; Layer, Row, Column, Layers, Rows, Columns: clm_uint; DestinationId: clm_id): LongInt;
var
  S, D: SizeInt;
  Z, Y: clm_uint;
  SrcOffset, DstOffset, Bytes: SizeUInt;
  SourceInfo: clm_storage_info;
  SourceData: TStorageBytes;
begin
  S := FindStorage(SourceId);
  if S < 0 then Exit(CLM_BPA_FAILURE);
  SourceInfo := Storages[S].Info;
  if (Layers = 0) or (Rows = 0) or (Columns = 0) or
     (Layer >= SourceInfo.layers) or (Row >= SourceInfo.rows) or
     (Column >= SourceInfo.columns) or
     (Layers > SourceInfo.layers - Layer) or
     (Rows > SourceInfo.rows - Row) or
     (Columns > SourceInfo.columns - Column) then Exit(CLM_BPA_FAILURE);
  { Preserve source backing through possible Storages dynamic-array relocation. }
  SourceData := Storages[S].Data;
  if clm_storage_new(DestinationId, SourceInfo.kind, SourceInfo.element_size,
                     Columns, Rows, Layers) <> CLM_BPA_SUCCESS then Exit(CLM_BPA_FAILURE);
  D := FindStorage(DestinationId);
  Bytes := SizeUInt(Columns) * SourceInfo.element_size;
  for Z := 0 to Layers - 1 do
    for Y := 0 to Rows - 1 do
    begin
      SrcOffset := ((SizeUInt(Layer + Z) * SourceInfo.rows + Row + Y) *
                    SourceInfo.columns + Column) * SourceInfo.element_size;
      DstOffset := (SizeUInt(Z) * Rows + Y) * Columns * SourceInfo.element_size;
      Move(SourceData[SrcOffset], Storages[D].Data[DstOffset], Bytes);
    end;
  Result := CLM_BPA_SUCCESS;
end;

function FrameInfo(Id: clm_id; Layer, FrameRow, FrameColumn: clm_uint; out StorageIndex: SizeInt): Boolean;
var Info: clm_storage_info;
begin
  StorageIndex := FindStorage(Id);
  Result := StorageIndex >= 0;
  if not Result then Exit;
  Info := Storages[StorageIndex].Info;
  Result := (Info.atlas <> 0) and (Layer < Info.layers) and (FrameRow < Info.rows div Info.frame_rows) and (FrameColumn < Info.columns div Info.frame_columns);
end;

function clm_storage_atlas_get_frame_to(Id: clm_id; Layer, FrameRow, FrameColumn: clm_uint; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
var I: SizeInt; Y: clm_uint; Stride, Needed, Offset: SizeUInt;
begin
  if (Buffer = nil) or not FrameInfo(Id, Layer, FrameRow, FrameColumn, I) then Exit(CLM_BPA_FAILURE);
  Stride := SizeUInt(Storages[I].Info.frame_columns) * Storages[I].Info.element_size;
  Needed := Stride * Storages[I].Info.frame_rows;
  if BufferSize < Needed then Exit(CLM_BPA_FAILURE);
  for Y := 0 to Storages[I].Info.frame_rows - 1 do begin
    Offset := ((SizeUInt(Layer) * Storages[I].Info.rows + FrameRow * Storages[I].Info.frame_rows + Y) * Storages[I].Info.columns + FrameColumn * Storages[I].Info.frame_columns) * Storages[I].Info.element_size;
    Move(Storages[I].Data[Offset], (PByte(Buffer) + SizeUInt(Y) * Stride)^, Stride);
  end;
  Result := CLM_BPA_GET_SUCCESS;
end;

function clm_storage_atlas_set_frame_from(Id: clm_id; Layer, FrameRow, FrameColumn: clm_uint; Buffer: Pointer; BufferSize: SizeUInt): LongInt;
var I: SizeInt; Y: clm_uint; Stride, Needed, Offset: SizeUInt;
begin
  if (Buffer = nil) or not FrameInfo(Id, Layer, FrameRow, FrameColumn, I) then Exit(CLM_BPA_FAILURE);
  Stride := SizeUInt(Storages[I].Info.frame_columns) * Storages[I].Info.element_size;
  Needed := Stride * Storages[I].Info.frame_rows;
  if BufferSize <> Needed then Exit(CLM_BPA_FAILURE);
  for Y := 0 to Storages[I].Info.frame_rows - 1 do begin
    Offset := ((SizeUInt(Layer) * Storages[I].Info.rows + FrameRow * Storages[I].Info.frame_rows + Y) * Storages[I].Info.columns + FrameColumn * Storages[I].Info.frame_columns) * Storages[I].Info.element_size;
    Move((PByte(Buffer) + SizeUInt(Y) * Stride)^, Storages[I].Data[Offset], Stride);
  end;
  Result := CLM_BPA_SET_SUCCESS;
end;

function clm_storage_atlas_slice_frame(Id: clm_id; Layer, FrameRow, FrameColumn: clm_uint; DestinationId: clm_id): LongInt;
var I: SizeInt; Info: clm_storage_info;
begin
  if not FrameInfo(Id, Layer, FrameRow, FrameColumn, I) then Exit(CLM_BPA_FAILURE);
  Info := Storages[I].Info;
  Result := clm_storage_slice(Id, Layer, FrameRow * Info.frame_rows, FrameColumn * Info.frame_columns, 1, Info.frame_rows, Info.frame_columns, DestinationId);
end;

end.