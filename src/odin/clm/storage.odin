package clm

import "core:mem"

// CC0 1.0. Fixed-width, byte-copyable row-major grids backed by BPA/BPAA.
// Coordinates are zero-based, in layer/row/column order. Resources are not thread-safe.
/*
BPA-backed fixed-dimension grid and atlas storage.

Coordinates are zero-based and row-major: columns vary fastest, then rows,
then layers. Payloads are fixed-width byte-copyable BPA values; strings and
owning object types are not supported. Operations are not thread-safe.
*/
CLM_STORAGE_MAGIC :: u64(0x434c4d53544f5247)

/*
Dimensions and element metadata for an existing storage resource.
*/
clm_storage_info :: struct {
	// Number of elements along the column (x), row (y), and layer (z) axes.
	columns, rows, layers: u32,
	// Atlas frame dimensions in elements; zero for non-atlas storage.
	frame_columns, frame_rows: u32,
	// Byte width of every stored value.
	element_size: u32,
	// BPA value type used for each element.
	type: u8,
	// Nonzero when the storage is configured as an atlas.
	atlas: u8,
	// Internal BPAA array ID; managed by clm_storage.
	data_id: idkey,
}

clm_storage_internal_metadata :: struct {
	magic: u64,
	info: clm_storage_info,
}

clm_storage_internal_read :: proc(id: idkey, out: ^clm_storage_internal_metadata) -> i32 {
	block := clm_bpa_find_block(id)
	if out == nil || block == nil || block.type != CLM_BPA_TYPE_STRUCT ||
	   block.block_size != size_of(clm_storage_internal_metadata) || block.data == nil {
		return CLM_BPA_FAILURE
	}
	mem.copy_non_overlapping(out, block.data, size_of(clm_storage_internal_metadata))
	if out.magic != CLM_STORAGE_MAGIC {
		return CLM_BPA_FAILURE
	}
	array := clm_bpaa_find_array(out.info.data_id)
	if array == nil || array.type != out.info.type || array.block_size != out.info.element_size {
		return CLM_BPA_FAILURE
	}
	return CLM_BPA_SUCCESS
}

clm_storage_internal_new :: proc(
	id: idkey, type: u8, struct_size: int, columns, rows, layers, frame_columns, frame_rows: u32, atlas: bool,
) -> i32 {
	if columns == 0 || rows == 0 || layers == 0 || clm_bpa_is_existing(id) || clm_bpaa_is_existing(id) ||
	   (atlas && (frame_columns == 0 || frame_rows == 0 || columns % frame_columns != 0 || rows % frame_rows != 0)) ||
	   (!atlas && (frame_columns != 0 || frame_rows != 0)) || type == CLM_BPA_TYPE_STRING {
		return CLM_BPA_FAILURE
	}
	width := clm_bpa_type_size(type)
	if type == CLM_BPA_TYPE_STRUCT {
		if struct_size <= 0 || u64(struct_size) > u64(max(u32)) {
			return CLM_BPA_FAILURE
		}
		width = u32(struct_size)
	} else if width == 0 || (struct_size != 0 && struct_size != int(width)) {
		return CLM_BPA_FAILURE
	}
	if u64(columns)*u64(rows) > u64(max(i32))/u64(layers) {
		return CLM_BPA_FAILURE
	}
	total := u64(columns)*u64(rows)*u64(layers)
	if total > u64(max(int))/u64(width) {
		return CLM_BPA_FAILURE
	}
	data_id := max(idkey)
	for data_id == id || clm_bpaa_is_existing(data_id) {
		if data_id == 0 {
			return CLM_BPA_FAILURE
		}
		data_id -= 1
	}
	if clm_bpaa_create_sized(data_id, type, i32(total), width, 0) != CLM_BPA_SUCCESS {
		return CLM_BPA_FAILURE
	}
	array := clm_bpaa_find_array(data_id)
	if array == nil {
		_ = clm_bpaa_delete(data_id)
		return CLM_BPA_FAILURE
	}
	array.count = u32(total)
	array.deck_size = array.count
	if clm_bpa_create_sized(id, CLM_BPA_TYPE_STRUCT, u32(size_of(clm_storage_internal_metadata))) != CLM_BPA_SUCCESS {
		_ = clm_bpaa_delete(data_id)
		return CLM_BPA_FAILURE
	}
	metadata := clm_storage_internal_metadata{
		magic = CLM_STORAGE_MAGIC,
		info = {columns, rows, layers, frame_columns, frame_rows, width, type, u8(atlas), data_id},
	}
	if clm_bpa_set(id, &metadata, size_of(metadata)) != CLM_BPA_SET_SUCCESS {
		_ = clm_bpaa_delete(data_id)
		_ = clm_bpa_delete(id)
		return CLM_BPA_FAILURE
	}
	return CLM_BPA_SUCCESS
}

/*
Creates a three-dimensional row-major storage grid.
The values are zero-initialized. Use CLM_BPA_TYPE_STRUCT with a nonzero
struct_size for custom fixed-width elements; strings are unsupported.
id: Unique storage ID across BPA blocks and BPAA arrays.
type: BPA type for each element.
struct_size: Byte width for STRUCT; pass zero for built-in types.
columns: Number of columns; must be nonzero.
rows: Number of rows; must be nonzero.
layers: Number of layers; must be nonzero.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid dimensions/type/ID.
*/
clm_storage_new :: proc(id: idkey, type: u8, struct_size: int, columns, rows, layers: u32) -> i32 {
	return clm_storage_internal_new(id, type, struct_size, columns, rows, layers, 0, 0, false)
}

/*
Creates a two-dimensional row-major grid (one layer).
id: Unique storage ID.
type: BPA type for each element.
struct_size: Byte width for STRUCT; pass zero for built-in types.
columns: Number of columns; must be nonzero.
rows: Number of rows; must be nonzero.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid dimensions/type/ID.
*/
clm_storage_new_2d :: proc(id: idkey, type: u8, struct_size: int, columns, rows: u32) -> i32 {
	return clm_storage_new(id, type, struct_size, columns, rows, 1)
}

/*
Creates a two-dimensional atlas whose grid dimensions are divisible by the frame size.
Frames are addressed by frame row and frame column, independently of element
row/column coordinates.
id: Unique storage ID.
type: BPA type for each element.
struct_size: Byte width for STRUCT; pass zero for built-in types.
columns: Total atlas width in elements.
rows: Total atlas height in elements.
frame_columns: Frame width in elements.
frame_rows: Frame height in elements.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid dimensions/type/ID.
*/
clm_storage_atlas_new :: proc(id: idkey, type: u8, struct_size: int, columns, rows, frame_columns, frame_rows: u32) -> i32 {
	return clm_storage_internal_new(id, type, struct_size, columns, rows, 1, frame_columns, frame_rows, true)
}

/*
Creates a layered atlas; each layer contains the same frame grid.
id: Unique storage ID.
type: BPA type for each element.
struct_size: Byte width for STRUCT; pass zero for built-in types.
columns: Total atlas width in elements.
rows: Total atlas height in elements.
layers: Number of atlas layers.
frame_columns: Frame width in elements.
frame_rows: Frame height in elements.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid dimensions/type/ID.
*/
clm_storage_atlas_new_3d :: proc(id: idkey, type: u8, struct_size: int, columns, rows, layers, frame_columns, frame_rows: u32) -> i32 {
	return clm_storage_internal_new(id, type, struct_size, columns, rows, layers, frame_columns, frame_rows, true)
}

/*
Copies dimensions and element metadata to the caller.
id: Storage ID.
info: Receives metadata; must not be nil.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/resource.
*/
clm_storage_get_info :: proc(id: idkey, info: ^clm_storage_info) -> i32 {
	metadata: clm_storage_internal_metadata
	if info == nil || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS {
		return CLM_BPA_FAILURE
	}
	info^ = metadata.info
	return CLM_BPA_GET_SUCCESS
}

/*
Deletes the storage resource and its internally managed BPAA array.
id: Storage ID.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the ID is invalid.
*/
clm_storage_delete :: proc(id: idkey) -> i32 {
	metadata: clm_storage_internal_metadata
	if clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS {
		return CLM_BPA_FAILURE
	}
	a := clm_bpaa_delete(metadata.info.data_id)
	b := clm_bpa_delete(id)
	return CLM_BPA_SUCCESS if a == CLM_BPA_SUCCESS && b == CLM_BPA_SUCCESS else CLM_BPA_FAILURE
}

clm_storage_internal_index :: proc(info: ^clm_storage_info, layer, row, column: u32, index: ^i32) -> i32 {
	if info == nil || index == nil || layer >= info.layers || row >= info.rows || column >= info.columns {
		return CLM_BPA_FAILURE
	}
	index^ = i32((u64(layer)*u64(info.rows)+u64(row))*u64(info.columns)+u64(column))
	return CLM_BPA_SUCCESS
}

/*
Copies one element into caller-owned memory.
Coordinates are zero-based in layer, row, column order.
id: Storage ID.
layer: Zero-based layer coordinate.
row: Zero-based row coordinate.
column: Zero-based column coordinate.
buffer: Receives the element bytes.
buffer_size: Destination capacity; must be at least the element width.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
*/
clm_storage_get_to :: proc(id: idkey, layer, row, column: u32, buffer: rawptr, buffer_size: int) -> i32 {
	metadata: clm_storage_internal_metadata
	index: i32
	if buffer == nil || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
	   clm_storage_internal_index(&metadata.info, layer, row, column, &index) != CLM_BPA_SUCCESS ||
	   buffer_size < int(metadata.info.element_size) {
		return CLM_BPA_FAILURE
	}
	return clm_bpaa_get_raw(metadata.info.data_id, index, buffer, metadata.info.element_size)
}

/*
Replaces one element from caller-owned bytes.
Coordinates are zero-based in layer, row, column order.
id: Storage ID.
layer: Zero-based layer coordinate.
row: Zero-based row coordinate.
column: Zero-based column coordinate.
value: Source element bytes.
value_size: Source byte count; must exactly equal the element width.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
*/
clm_storage_set_from :: proc(id: idkey, layer, row, column: u32, value: rawptr, value_size: int) -> i32 {
	metadata: clm_storage_internal_metadata
	index: i32
	if value == nil || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
	   clm_storage_internal_index(&metadata.info, layer, row, column, &index) != CLM_BPA_SUCCESS ||
	   value_size != int(metadata.info.element_size) {
		return CLM_BPA_FAILURE
	}
	return clm_bpaa_set_raw(metadata.info.data_id, index, value, metadata.info.element_size)
}

/*
Copies one element into a compatible BPA destination block.
id: Storage ID.
layer: Zero-based layer coordinate.
row: Zero-based row coordinate.
column: Zero-based column coordinate.
destination_block_id: Existing BPA block with matching type and size.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
*/
clm_storage_get :: proc(id: idkey, layer, row, column: u32, destination_block_id: idkey) -> i32 {
	metadata: clm_storage_internal_metadata
	index: i32
	if clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
	   clm_storage_internal_index(&metadata.info, layer, row, column, &index) != CLM_BPA_SUCCESS {
		return CLM_BPA_FAILURE
	}
	return clm_bpaa_get(metadata.info.data_id, index, destination_block_id, int(metadata.info.element_size))
}

/*
Copies a compatible BPA source block into one element.
id: Storage ID.
layer: Zero-based layer coordinate.
row: Zero-based row coordinate.
column: Zero-based column coordinate.
source_block_id: Existing BPA block with matching type and size.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/coordinates.
*/
clm_storage_set :: proc(id: idkey, layer, row, column: u32, source_block_id: idkey) -> i32 {
	metadata: clm_storage_internal_metadata
	index: i32
	if clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS ||
	   clm_storage_internal_index(&metadata.info, layer, row, column, &index) != CLM_BPA_SUCCESS {
		return CLM_BPA_FAILURE
	}
	return clm_bpaa_set(metadata.info.data_id, index, source_block_id, int(metadata.info.element_size))
}

// Snapshot input before modifying cells, including when the source aliases the grid.
/*
Replaces every element with a copy of the same value.
id: Storage ID.
value: Source element bytes.
value_size: Source byte count; must exactly equal the element width.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/resource.
*/
clm_storage_fill_from :: proc(id: idkey, value: rawptr, value_size: int) -> i32 {
	metadata: clm_storage_internal_metadata
	if value == nil || clm_storage_internal_read(id, &metadata) != CLM_BPA_SUCCESS || value_size != int(metadata.info.element_size) {
		return CLM_BPA_FAILURE
	}
	snapshot, allocation_error := make([]u8, value_size)
	if allocation_error != nil {
		return CLM_BPA_FAILURE
	}
	defer delete(snapshot)
	mem.copy_non_overlapping(raw_data(snapshot), value, value_size)
	total := metadata.info.columns*metadata.info.rows*metadata.info.layers
	for index in 0..<total {
		if clm_bpaa_set_raw(metadata.info.data_id, i32(index), raw_data(snapshot), metadata.info.element_size) != CLM_BPA_SET_SUCCESS {
			return CLM_BPA_FAILURE
		}
	}
	return CLM_BPA_SUCCESS
}

/*
Copies a contiguous rectangular or volumetric region into new storage.
The destination preserves the source element type and width, but is a regular
grid rather than an atlas.
source_id: Source storage ID.
layer: Starting zero-based layer.
row: Starting zero-based row.
column: Starting zero-based column.
layers: Number of layers to copy.
rows: Number of rows to copy.
columns: Number of columns to copy.
destination_id: New unique ID for the copied storage.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid bounds/IDs/allocation.
*/
clm_storage_slice :: proc(source_id: idkey, layer, row, column, layers, rows, columns: u32, destination_id: idkey) -> i32 {
	source: clm_storage_internal_metadata
	if clm_storage_internal_read(source_id, &source) != CLM_BPA_SUCCESS ||
	   layers == 0 || rows == 0 || columns == 0 ||
	   layer >= source.info.layers || row >= source.info.rows || column >= source.info.columns ||
	   layers > source.info.layers-layer || rows > source.info.rows-row || columns > source.info.columns-column {
		return CLM_BPA_FAILURE
	}
	if clm_storage_new(destination_id, source.info.type, int(source.info.element_size), columns, rows, layers) != CLM_BPA_SUCCESS {
		return CLM_BPA_FAILURE
	}
	buffer, allocation_error := make([]u8, int(source.info.element_size))
	if allocation_error != nil {
		_ = clm_storage_delete(destination_id)
		return CLM_BPA_FAILURE
	}
	defer delete(buffer)
	for z in 0..<layers {
		for y in 0..<rows {
			for x in 0..<columns {
				if clm_storage_get_to(source_id, layer+z, row+y, column+x, raw_data(buffer), len(buffer)) != CLM_BPA_GET_SUCCESS ||
				   clm_storage_set_from(destination_id, z, y, x, raw_data(buffer), len(buffer)) != CLM_BPA_SET_SUCCESS {
					_ = clm_storage_delete(destination_id)
					return CLM_BPA_FAILURE
				}
			}
		}
	}
	return CLM_BPA_SUCCESS
}

// Resolves and validates an atlas frame. Internal helper shared by frame APIs.
clm_storage_atlas_internal_info :: proc(id: idkey, layer, frame_row, frame_column: u32, info: ^clm_storage_info) -> bool {
	if clm_storage_get_info(id, info) != CLM_BPA_GET_SUCCESS || info.atlas == 0 ||
	   info.frame_rows == 0 || info.frame_columns == 0 || layer >= info.layers {
		return false
	}
	return frame_row < info.rows/info.frame_rows && frame_column < info.columns/info.frame_columns
}

// Atlas buffers are tightly packed row-major bytes; the snapshot makes overlap safe.
/*
Copies one atlas frame to a tightly packed row-major caller buffer.
id: Atlas storage ID.
layer: Zero-based layer.
frame_row: Zero-based frame row within the atlas.
frame_column: Zero-based frame column within the atlas.
buffer: Receives the frame bytes, with no row padding.
buffer_size: Destination capacity; must fit the entire frame.
Returns: CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input/bounds.
*/
clm_storage_atlas_get_frame_to :: proc(id: idkey, layer, frame_row, frame_column: u32, buffer: rawptr, buffer_size: int) -> i32 {
	info: clm_storage_info
	if buffer == nil || !clm_storage_atlas_internal_info(id, layer, frame_row, frame_column, &info) {
		return CLM_BPA_FAILURE
	}
	needed := int(info.frame_columns)*int(info.frame_rows)*int(info.element_size)
	if buffer_size < needed {
		return CLM_BPA_FAILURE
	}
	temporary, allocation_error := make([]u8, needed)
	if allocation_error != nil {
		return CLM_BPA_FAILURE
	}
	defer delete(temporary)
	for y in 0..<info.frame_rows {
		for x in 0..<info.frame_columns {
			offset := (int(y)*int(info.frame_columns)+int(x))*int(info.element_size)
			if clm_storage_get_to(id, layer, frame_row*info.frame_rows+y, frame_column*info.frame_columns+x,
				raw_data(temporary[offset:]), int(info.element_size)) != CLM_BPA_GET_SUCCESS {
				return CLM_BPA_FAILURE
			}
		}
	}
	mem.copy(buffer, raw_data(temporary), needed)
	return CLM_BPA_GET_SUCCESS
}

/*
Copies a tightly packed row-major buffer into one atlas frame.
id: Atlas storage ID.
layer: Zero-based layer.
frame_row: Zero-based frame row within the atlas.
frame_column: Zero-based frame column within the atlas.
buffer: Frame bytes in row-major order, with no row padding.
buffer_size: Source byte count; must exactly equal the frame size.
Returns: CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/bounds.
*/
clm_storage_atlas_set_frame_from :: proc(id: idkey, layer, frame_row, frame_column: u32, buffer: rawptr, buffer_size: int) -> i32 {
	info: clm_storage_info
	if buffer == nil || !clm_storage_atlas_internal_info(id, layer, frame_row, frame_column, &info) {
		return CLM_BPA_FAILURE
	}
	needed := int(info.frame_columns)*int(info.frame_rows)*int(info.element_size)
	if buffer_size != needed {
		return CLM_BPA_FAILURE
	}
	temporary, allocation_error := make([]u8, needed)
	if allocation_error != nil {
		return CLM_BPA_FAILURE
	}
	defer delete(temporary)
	mem.copy_non_overlapping(raw_data(temporary), buffer, needed)
	for y in 0..<info.frame_rows {
		for x in 0..<info.frame_columns {
			offset := (int(y)*int(info.frame_columns)+int(x))*int(info.element_size)
			if clm_storage_set_from(id, layer, frame_row*info.frame_rows+y, frame_column*info.frame_columns+x,
				raw_data(temporary[offset:]), int(info.element_size)) != CLM_BPA_SET_SUCCESS {
				return CLM_BPA_FAILURE
			}
		}
	}
	return CLM_BPA_SET_SUCCESS
}

/*
Copies an atlas frame into a new regular 2D storage resource.
id: Atlas storage ID.
layer: Zero-based layer.
frame_row: Zero-based frame row within the atlas.
frame_column: Zero-based frame column within the atlas.
destination_id: New unique ID for the frame storage.
Returns: CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/bounds/ID.
*/
clm_storage_atlas_slice_frame :: proc(id: idkey, layer, frame_row, frame_column: u32, destination_id: idkey) -> i32 {
	info: clm_storage_info
	if !clm_storage_atlas_internal_info(id, layer, frame_row, frame_column, &info) {
		return CLM_BPA_FAILURE
	}
	return clm_storage_slice(id, layer, frame_row*info.frame_rows, frame_column*info.frame_columns,
		1, info.frame_rows, info.frame_columns, destination_id)
}
