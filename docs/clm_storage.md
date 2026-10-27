# CLM — Storage

## 1. Purpose

`clm_storage` is an ID-addressed **fixed-dimension**, **row-major** data grid backed by one BPAA allocation with a BPA metadata block. It supports 2D grids, layered 3D grids, and an atlas interpretation of a grid divided into equal-sized rectangular frames. It stores fixed-width elements; it is not an image decoder, serializer, multidimensional object model, or table/query engine.

This C implementation is header-only: include `clm/storage.h`. As with BPA itself, compile `#define CLM_BPA_IMPLEMENTATION` in **exactly one C translation unit**, before the first inclusion of `storage.h` or `bpa.h`. Other files include normally. No `CLM_STORAGE_IMPLEMENTATION` macro is required.

## 2. Data and coordinate model

- **Column (`x`)**, **row (`y`)**, and **layer (`z`)** indexes are zero-based and supplied in the function signatures as `(layer, row, column)`.
- A 2D storage has exactly one layer (`layer=0`). Dimensions are positive and fixed for the lifetime of a storage.
- A regular grid is **rectangular**: every row has the same column count; every layer has the same row and column dimensions.
- Flat array index is `((layer * rows) + row) * columns + column`.
- All cells are initialized to zero bytes at creation; all cells are immediately committed (`count == capacity`).
- Values are byte-copied, fixed-width BPA element types (`BYTE`, `SHORT`, `INT`, `LONG`, `FLOAT`, `DOUBLE`, `UUID`, `DATETIME`, and `STRUCT` where supported by BPA). `STRUCT` requires a positive explicit element width. `STRING` is **not supported** in this revision; use separately managed strings or indices/IDs in grid records instead. For `STRUCT`, only trivially byte-copyable C-compatible record layouts are safe; pointers within copied records are not deep-copied.
- Dimensions are limited by `clm_int` maximum element count and available memory. Multiplication and allocation size overflow are rejected.

## 3. Construction and lifecycle

```c
clm_storage_new(id, type, struct_size, columns, rows, layers);
clm_storage_new_2d(id, type, struct_size, columns, rows);
clm_storage_delete(id);
clm_storage_get_info(id, &info);
```

Pass `struct_size=0` for built-in types; for `STRUCT`, pass `sizeof(MyRecord)`. Duplicate IDs and unsupported dimensions/types fail. The numeric storage ID is a BPA block ID; the internal BPAA allocation receives a separate automatically chosen ID. All internal storage IDs, metadata and backing arrays are **private implementation resources**: never alter or delete them through raw BPA/BPAA APIs. `clm_storage_delete()` releases the backing array and metadata. Like BPA, forced `clm_bpa_shutdown()` invalidates every storage.

### Information snapshot

`clm_storage_info` contains `columns`, `rows`, `layers`, `frame_columns`, `frame_rows`, `element_size`, `type`, `atlas`, and `data_id`. `data_id` is exposed as diagnostic metadata **only**; it is not a supported mutation/ownership API. For ordinary grids `atlas=0` and frame dimensions are zero.

## 4. Cell operations

```c
clm_storage_set_from(storage_id, layer, row, column, &value, sizeof(value));
clm_storage_get_to(storage_id, layer, row, column, &value, sizeof(value));

clm_storage_set(storage_id, layer, row, column, precreated_source_bpa_block_id);
clm_storage_get(storage_id, layer, row, column, precreated_destination_bpa_block_id);

clm_storage_fill_from(storage_id, &value, sizeof(value));
```

- `_from` reads from a caller-owned C buffer, `_to` writes to one. `set_from` requires the exact element width; `get_to` accepts a destination of at least that width.
- Unsuffixed `get` and `set` copy to/from **already-existing** BPA blocks with matching type and width. No block is created implicitly.
- `fill_from` copies one record to **every** cell. It snapshots the value before filling, so even values borrowed from inside the grid can be used safely.
- Out-of-range coordinates, incorrect widths, missing IDs, incompatible BPA blocks and failed allocations return failure. A failed cell read/write does not change storage.

### Region slicing

```c
clm_storage_slice(source_id,
                  start_layer, start_row, start_column,
                  layer_count, row_count, column_count,
                  new_destination_id);
```

Creates an independently managed storage with the selected rectangular/volumetric region copied into its own contiguous BPAA backing allocation. Counts must be positive, the entire requested region must fit, and the destination ID must be unused. Slice output is ordinary (non-atlas) storage even when the input is an atlas. Neither source nor destination is a borrowed view. Invalid arguments fail without modifying the source.

## 5. Atlas storage

Atlas is a **coordinate interpretation of a single full grid**: the entire atlas is stored once in one BPAA allocation; frames are rectangles within each layer. It does not separately allocate each frame.

```c
clm_storage_atlas_new(id, type, struct_size,
                      atlas_columns, atlas_rows, frame_columns, frame_rows);
clm_storage_atlas_new_3d(id, type, struct_size,
                         atlas_columns, atlas_rows, layers,
                         frame_columns, frame_rows);
```

Atlas and frame sizes must be positive, and frame dimensions must divide atlas dimensions exactly. `atlas_columns/frame_columns` is the number of frame columns; `atlas_rows/frame_rows` is the number of frame rows. For an image atlas, one element is one already-decoded pixel or packed pixel record; e.g., RGBA8 might be a `STRUCT` with `struct_size=4`. For a tile atlas, one element may be a tile. This library does not decode PNG/JPEG or process color spaces.

### Frame operations

```c
clm_storage_atlas_get_frame_to(id, layer, frame_row, frame_column,
                               destination_buffer, buffer_byte_capacity);
clm_storage_atlas_set_frame_from(id, layer, frame_row, frame_column,
                                 source_buffer, exact_buffer_byte_size);
clm_storage_atlas_slice_frame(id, layer, frame_row, frame_column,
                              new_storage_id);
```

Frame copy buffers are tightly packed in **row-major** order without padding. Their required byte count is `frame_columns * frame_rows * element_size`. `get_frame_to` accepts at least the required byte capacity; `set_frame_from` requires exactly that byte count. Both copy the frame **row by row** using the full atlas row stride, and they snapshot their input/output so overlap with atlas backing memory is safe. `slice_frame` creates an ordinary independently owned one-layer 2D storage object holding the requested frame. Normal cell get/set functions work on atlas coordinates too.

Atlas operations do not promise zero-copy access to an individual frame, because rows of a frame typically are **not contiguous** inside the larger atlas. For whole-atlas transfer, use ordinary cell operations or external application-managed data-loading loops; a bulk import/export API can be added later.

## 6. Results

| Return | Meaning |
|---|---|
| `CLM_BPA_SUCCESS` (0) | Create, delete, fill or slice successful |
| `CLM_BPA_GET_SUCCESS` (1) | Cell, info or frame read successful |
| `CLM_BPA_SET_SUCCESS` (2) | Cell or frame write successful |
| `CLM_BPA_FAILURE` (-1) | Invalid resource, unsupported type/dimensions, bad bounds/size or allocation failure |

## 7. Example

```c
#define CLM_BPA_IMPLEMENTATION
#include "clm/storage.h"

int main(void) {
    enum { GRID = 10, ATLAS = 11, FRAME = 12 };
    clm_int number = 42, result = 0;
    if (clm_storage_new(GRID, CLM_BPA_TYPE_INT, 0, 8, 6, 3) != CLM_BPA_SUCCESS) return 1;
    clm_storage_set_from(GRID, 2, 1, 4, &number, sizeof(number));
    clm_storage_get_to(GRID, 2, 1, 4, &result, sizeof(result));
    clm_storage_delete(GRID);

    if (clm_storage_atlas_new(ATLAS, CLM_BPA_TYPE_BYTE, 0, 256, 128, 64, 64) != CLM_BPA_SUCCESS) return 1;
    clm_storage_atlas_slice_frame(ATLAS, 1, 2, FRAME);
    clm_storage_delete(FRAME);
    clm_storage_delete(ATLAS);
    clm_bpa_shutdown();
    return result == 42 ? 0 : 1;
}
```

## 8. Conformance tests and limitations

The accompanying `tests/test_storage.c` exercises 2D and 3D coordinate mapping, zero initialization, read/write, bounds, widths, duplicate IDs, region copies and ownership independence, atlas frame extraction and replacement, frame slicing, bulk fill, managed block transfer, invalid dimensions, and cleanup. Compile with GCC or Clang; sanitizers are recommended. Tests are not a proof of correctness under allocation failure or concurrent access.

Current limitations: no resizing, no string cells, no strided/padded external frame buffers, no lazy/file-backed storage, no external buffer adoption/borrowing, no multidimensional region assignment, and no threadsafe access. `clm_storage` is not a database; future table semantics, indexes, transactions and schema handling should be built separately.

**Cross-language promise:** consistent coordinate, layout, copy, and lifetime behavior; C's BPA-backed physical layout is an implementation choice rather than a requirement for ports in other languages.
