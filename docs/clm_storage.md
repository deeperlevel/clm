# CLM Storage

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_bpa](clm_bpa.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

ID-addressed fixed-dimension data grids and texture-atlas-like regions.

## Capabilities and operations

- Create row-major 2D grids, layered grids and atlases.
- Read/write typed fixed-width elements by coordinates or regions.
- Inspect dimension and atlas-frame metadata.
- Use byte-copyable fixed-size values; string and owning object types are unsupported; operations are not implicitly synchronized.

## Detailed module contract

## 1. Purpose

`clm_storage` is an ID-addressed **fixed-dimension**, **row-major** data grid backed by one BPAA allocation with a BPA metadata block. It supports 2D grids, layered 3D grids, and an atlas interpretation of a grid divided into equal-sized rectangular frames. It stores fixed-width elements; it is not an image decoder, serializer, multidimensional object model, or table/query engine.

## 2. Data and coordinate model

- **Column (`x`)**, **row (`y`)**, and **layer (`z`)** indexes are zero-based and supplied in the function signatures as `(layer, row, column)`.
- A 2D storage has exactly one layer (`layer=0`). Dimensions are positive and fixed for the lifetime of a storage.
- A regular grid is **rectangular**: every row has the same column count; every layer has the same row and column dimensions.
- Flat array index is `((layer * rows) + row) * columns + column`.
- All cells are initialized to zero bytes at creation; all cells are immediately committed (`count == capacity`).
- Values are byte-copied, fixed-width BPA element types (`BYTE`, `SHORT`, `INT`, `LONG`, `FLOAT`, `DOUBLE`, `UUID`, `DATETIME`, and `STRUCT` where supported by BPA). `STRUCT` requires a positive explicit element width. `STRING` is **not supported** in this revision; use separately managed strings or indices/IDs in grid records instead. For `STRUCT`, only trivially byte-copyable C-compatible record layouts are safe; pointers within copied records are not deep-copied.
- Dimensions are limited by `clm_int` maximum element count and available memory. Multiplication and allocation size overflow are rejected.

## 3. Construction and lifecycle

> Binding-specific code sample omitted; refer to the language implementation for executable examples.

Pass `struct_size=0` for built-in types; for `STRUCT`, pass `sizeof(MyRecord)`. Duplicate IDs and unsupported dimensions/types fail. The numeric storage ID is a BPA block ID; the internal BPAA allocation receives a separate automatically chosen ID. All internal storage IDs, metadata and backing arrays are **private implementation resources**: never alter or delete them through raw BPA/BPAA APIs. `clm_storage_delete()` releases the backing array and metadata. Like BPA, forced `clm_bpa_shutdown()` invalidates every storage.

### Information snapshot

`clm_storage_info` contains `columns`, `rows`, `layers`, `frame_columns`, `frame_rows`, `element_size`, `type`, `atlas`, and `data_id`. `data_id` is exposed as diagnostic metadata **only**; it is not a supported mutation/ownership API. For ordinary grids `atlas=0` and frame dimensions are zero.

## 4. Cell operations

> Binding-specific code sample omitted; refer to the language implementation for executable examples.

- `_from` reads from a caller-owned C buffer, `_to` writes to one. `set_from` requires the exact element width; `get_to` accepts a destination of at least that width.
- Unsuffixed `get` and `set` copy to/from **already-existing** BPA blocks with matching type and width. No block is created implicitly.
- `fill_from` copies one record to **every** cell. It snapshots the value before filling, so even values borrowed from inside the grid can be used safely.
- Out-of-range coordinates, incorrect widths, missing IDs, incompatible BPA blocks and failed allocations return failure. A failed cell read/write does not change storage.

### Region slicing

> Binding-specific code sample omitted; refer to the language implementation for executable examples.

Creates an independently managed storage with the selected rectangular/volumetric region copied into its own contiguous BPAA backing allocation. Counts must be positive, the entire requested region must fit, and the destination ID must be unused. Slice output is ordinary (non-atlas) storage even when the input is an atlas. Neither source nor destination is a borrowed view. Invalid arguments fail without modifying the source.

## 5. Atlas storage

Atlas is a **coordinate interpretation of a single full grid**: the entire atlas is stored once in one BPAA allocation; frames are rectangles within each layer. It does not separately allocate each frame.

> Binding-specific code sample omitted; refer to the language implementation for executable examples.

Atlas and frame sizes must be positive, and frame dimensions must divide atlas dimensions exactly. `atlas_columns/frame_columns` is the number of frame columns; `atlas_rows/frame_rows` is the number of frame rows. For an image atlas, one element is one already-decoded pixel or packed pixel record; e.g., RGBA8 might be a `STRUCT` with `struct_size=4`. For a tile atlas, one element may be a tile. This library does not decode PNG/JPEG or process color spaces.

### Frame operations

> Binding-specific code sample omitted; refer to the language implementation for executable examples.

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

> Binding-specific code sample omitted; refer to the language implementation for executable examples.

## 8. Conformance tests and limitations

Current limitations: no resizing, no string cells, no strided/padded external frame buffers, no lazy/file-backed storage, no external buffer adoption/borrowing, no multidimensional region assignment, and no threadsafe access. `clm_storage` is not a database; future table semantics, indexes, transactions and schema handling should be built separately.

**Cross-language promise:** consistent coordinate, layout, copy, and lifetime behavior; BPA-backed physical layout is one implementation choice rather than a requirement for ports in other languages.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Create row-major 2D grids, layered grids and atlases.
