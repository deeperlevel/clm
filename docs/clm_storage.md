# CLM Storage — Language-agnostic specification

**One-line description:** ID-addressed, typed sequential and multidimensional storage with explicit allocation, navigation, transformations, and manually managed lifetimes.

CLM — Cross Language Modules · Deeperlevel · https://github.com/deeperlevel/clm · deeparchive@outlook.com  
Public Domain — CC0 1.0 Universal · Provided AS IS · **Don't start from scratch, bro.**

## 1. Purpose and boundaries

`clm_storage` is an unkeyed, position-addressed resource registry. A **storage ID** identifies an entire resource, while individual values are accessed by zero-based indices or coordinates. `clm_container` is the complementary **keyed** store; neither module requires the other. `clm_block` (`Block {x,y,width,height}`) and `clm_block3d` (`Block3D {x,y,z,width,height,depth}`) provide standalone plain-data structures. They do not allocate memory and do not impose dimension validation.

The implementation may use native arrays, lists, vectors, byte buffers, or other facilities. The observable API and its behavioral contracts are shared, *not the implementation strategy*. Python's implementation uses lists internally; `reserved` is **logical capacity**, not proof of preallocated bytes. Compiled implementations should reserve actual capacity where feasible.

This specification covers the initial Python implementation delivered alongside this document. Some discussed capabilities are identified as future extensions at the end rather than falsely claimed as implemented.

## 2. Type IDs and value semantics

| ID | Type | Value contract |
|---|---|---|
| `0x01` | STRING | Unicode text; UTF-8 encoded length <= `size` bytes |
| `0x02` | BYTE | Signed 8-bit integer |
| `0x03` | SHORT | Signed 16-bit integer |
| `0x04` | INTEGER | Signed 32-bit integer |
| `0x05` | LONG_INTEGER | Signed 64-bit integer |
| `0x06` | FLOAT | IEEE-754 binary32 representative value |
| `0x07` | DOUBLE | Host double, normally IEEE-754 binary64 |
| `0xFE` | DYNAMIC | Exactly `size` bytes; stored as an owned immutable copy |
| `0xFF` | OBJECT | Opaque host-language reference; shallow-copy semantics |

For numeric fixed-width types, `size` equals their native byte width. For STRING it is the maximum UTF-8 byte count; for DYNAMIC it is the exact record length. OBJECT's `size` is metadata, not a promise of deep cloning. In the Python implementation, OBJECT accepts any object including `None`. Strings exceeding `size` are rejected **without truncation**. FLOAT rounds to binary32 on insertion. NaN/Infinity are accepted if representable; the standard numeric sort does not guarantee a total order involving NaN. Use an explicit comparator when required.

## 3. Status codes and calling convention

Mutators return one status. Queries returning data use `(status, value)`; counts such as `dispose`, `delete`, and `replace_all` use `(status, number)`. Other languages should express the same contract through result structs or out-parameters.

| Status | Hex | Meaning |
|---|---|---|
| `STORAGE_SUCCESS` | `0x00` | Completed |
| `STORAGE_ERROR_DUPLICATE_ID` | `0x01` | Storage ID exists |
| `STORAGE_ERROR_NOT_FOUND` | `0x02` | Storage ID not registered |
| `STORAGE_ERROR_TYPE_NOT_SUPPORTED` | `0x03` | Unsupported type |
| `STORAGE_ERROR_TYPE_NOT_MATCHED` | `0x04` | Source/destination type or size mismatch |
| `STORAGE_ERROR_INVALID_VALUE` | `0x05` | Value or configuration invalid |
| `STORAGE_ERROR_INVALID_INDEX` | `0x06` | Invalid index/coordinate |
| `STORAGE_ERROR_LOCKED` | `0x07` | Mutation forbidden |
| `STORAGE_ERROR_DIMENSION` | `0x08` | Invalid grid dimensions |
| `STORAGE_ERROR_EMPTY` | `0x09` | No elements available |
| `STORAGE_ERROR_REFERENCES` | `0x0A` | Reference count cannot decrement |
| `STORAGE_ERROR_OVERFLOW` | `0x0B` | Size mismatch rejected by overflow policy |
| `STORAGE_ERROR_INCOMPATIBLE` | `0x0C` | Operation does not apply to resource kind |

## 4. Lifecycle, metadata, and ownership

- `clm_storage_create(id, type=DYNAMIC, size=default, reserved=0)` creates an empty **linear** storage. `size` is mandatory for non-default custom record widths. For default DYNAMIC, `size=1`.
- `clm_storage_get_info(id) -> (status, StorageInfo)` yields a read-only metadata snapshot.
- `clm_storage_count(id) -> (status, integer)` reports committed entries.
- `clm_storage_reserve(id, count)` increases reported reserved capacity without committing elements; never shrinks.
- `clm_storage_resize(id, count)` changes committed length; new values are initialized to zero/empty/`None`/zero bytes. Shrinking discards trailing elements.
- `clm_storage_clear(id)` removes committed entries; capacity remains available.
- `clm_storage_destroy(id)` **immediately** removes a resource, overriding positive references (explicit force destroy, consistent with current Container behavior).
- `clm_storage_reference(id)` increments the manual count.
- `clm_storage_unreference(id)` decrements the manual count, returning an error if already zero; **never** frees the resource directly.
- `clm_storage_dispose() -> (status, disposed_count)` removes **all and only** resources with zero references, including grid resources, irrespective of lock state.
- `clm_storage_lock(id)` and `clm_storage_unlock(id)` toggle API mutation prevention. This is **not** a concurrency mutex.

`StorageInfo` fields: `id`, `type`, `committed`, `reserved`, `references`, `index`, `size`, `locked`, `spent`, `kind`, `width`, `height`, `depth`, `rows`, `columns`, `layers`. Resources start with `references=1`, `index=-1`, `spent=0`, `locked=false`. A storage ID may be reused after destruction. Old IDs must not be assumed valid after destruction and re-creation; generation handles are not part of this version.

OBJECT entries hold shallow references. `destroy`, `clear`, or `dispose` release the storage's references, **not necessarily the resources owned by those objects**. Native ports need their own explicit destructor/ownership policy; this module does not invoke arbitrary user destructors.

## 5. Linear access, cursor, and mutations

Linear storage is sequential and does not support caller-defined entry keys. Valid indices are `0 .. committed-1`. The per-storage cursor is a separate numeric index. Reading/manipulating an explicit index **never navigates the cursor**; insert/remove preserve its numeric position if valid, otherwise clamp to the last index; clearing resets it to `-1`.

- Navigation: `clm_storage_move_first`, `move_last`, `move_next`, `move_previous`, `iterate` (= first), `iterate_backward` (= last), `set_index(id,index)`, `get_index`, `get(id,index=None)`, `is_first`, `is_last`.
- `clm_storage_append(id,value)` adds to the end.
- `clm_storage_insert(id,index,value)` inserts before index; `index==count` is allowed.
- `clm_storage_replace(id,index,value)` overwrites one element.
- `clm_storage_remove(id,index)` removes one element and shifts the suffix left.
- `clm_storage_swap(id,a,b)` swaps two positions.
- `clm_storage_copy(id,source_index,destination_index,length)` overwrites **within** the same storage. Source and destination ranges must be fully in bounds. Overlaps have snapshot / `memmove` behavior; no automatic extension.
- `clm_storage_replace_all(id,old,new) -> (status,count)` replaces all matching values.
- `clm_storage_delete(id,value) -> (status,count)` removes all matching values.
- `clm_storage_add_sampling(id,value,count,index=None)` inserts `count` repeats, appending by default. Mutable OBJECTs repeat the **same object reference**, not clones.

## 6. Transfers and transformations

`clm_storage_clone(source,destination)` replaces destination values with the entire source; `clm_storage_append_clone(source,destination)` appends; `clm_storage_slice(source,destination,start,count)` replaces destination with a selected positional slice. `clm_storage_filter(source,predicate,destination)` replaces destination with values matching a predicate, preserving order. The destination **must already exist, be unlocked, and have the same type, size and linear kind**. Operations preserve destination resource ID, reference count, and lock state. Source/destination may be the same for these linear operations (read snapshot first). They do not automatically clone mutable OBJECT data.

- `clm_storage_shuffle(id,rng=None)`: Fisher–Yates through host's shuffle (optional injected RNG in Python).
- `clm_storage_reverse(id)`.
- `clm_storage_sort(id,comparator=None,descending=False)`; `clm_storage_sort_desc(id,comparator=None)`. Sorting OBJECT requires an explicit comparator.
- `clm_storage_spend(id,index) -> (status,value)`: swaps the chosen **available** entry with the last available entry and increments `spent`; O(1) swap for fixed-size records.
- `clm_storage_draw(id,rng=None) -> (status,value)`: picks a random available entry uniformly and spends it.
- `clm_storage_reset_spent(id)`: marks all entries as available again.

The spent partition is `[available | spent]`, with `available=count-spent`. A draw never repeats a spent position before reset. Mutations, sorting, reverse, and shuffle reset `spent=0`. Spending modifies ordering but keeps committed count unchanged. Repeated draw on exhausted storage yields `STORAGE_ERROR_EMPTY`.

**Failure atomicity:** Validate IDs, bounds, values, types and locks before altering ordinary operations. Python comparator or predicate exceptions are propagated; filter/sort construct results before mutating destination. Native allocation failures need explicit error handling.

## 7. Multidimensional layout

A **Block** stores a two-dimensional fixed-size grid. A **Block3D** stores a three-dimensional grid. An **Atlas** stores a grid of Blocks. An **Atlas3D** stores a 3D grid of Block3Ds. All grid resources contain default-valued committed cells at creation; they are not manually extended through linear append.

### Constructors

```text
clm_storage_block_create(id, width, height, size=default, storage_type=BYTE)
clm_storage_block3d_create(id, width, height, depth, size=default, storage_type=BYTE)
clm_storage_atlas_create(id, width, height, rows, columns, size=default, storage_type=BYTE)
clm_storage_atlas3d_create(id, width, height, depth, rows, columns, layers,
                          size=default, storage_type=BYTE)
```

`width,height,depth` describe **cells within a block**. `columns,rows,layers` describe **the number of blocks in the atlas**. All must be strictly positive. For a 2D Atlas, `depth=layers=1`. Example: `width=20,height=20,rows=20,columns=20` means 400 blocks and 160,000 total cells.

Each Block uses row-major layout: `cell_index=(z*height+y)*width+x`. Each Atlas stores its blocks contiguously in row-major block order: `block_index=(layer*rows+row)*columns+column`, multiplied by `width*height*depth`, plus cell index. Python uses one logical list for the full grid (not one Python list per block).

### Cell access

- `clm_storage_block_get(id,x,y)` / `clm_storage_block_set(id,x,y,value)`.
- `clm_storage_block3d_get(id,x,y,z)` / `clm_storage_block3d_set(id,x,y,z,value)`.
- `clm_storage_grid_get(id,x,y,z=0,column=0,row=0,layer=0)` and `clm_storage_grid_set(id,x,y,value,z=0,column=0,row=0,layer=0)` for all grid kinds. `column,row,layer` select an Atlas's **block**, while `x,y,z` select a cell within that block.

Grid accesses do not navigate the linear cursor. Values are validated against their declared type and size. Grid mutations obey lock state. The `Block` and `Block3D` dataclasses themselves remain simple geometry records and are **not** storage handles.

### Block transfers

```text
clm_storage_atlas_get(atlas_id, column, row, destination_block_id, overflow=CANCEL)
clm_storage_atlas_set(atlas_id, column, row, source_block_id, overflow=CANCEL)
clm_storage_atlas3d_get(atlas3d_id, column, row, layer, destination_block3d_id, overflow=CANCEL)
clm_storage_atlas3d_set(atlas3d_id, column, row, layer, source_block3d_id, overflow=CANCEL)
```

These **copy** the selected block, not return views. The source and target must share element type and size. When the source is larger along any dimension than the destination:

| Overflow policy | Value | Contract |
|---|---|---|
| `STORAGE_ATLAS_OVERFLOW_IGNORE` | `0x01` | Copy only overlapping cells; preserve destination dimensions and untouched cells |
| `STORAGE_ATLAS_OVERFLOW_EXTEND` | `0x02` | Grow destination's per-block dimensions to at least source dimensions; retain previously addressable cells |
| `STORAGE_ATLAS_OVERFLOW_CANCEL` | `0x03` | Reject without mutation (default) |

A destination larger than its source retains cells outside the copied region. Invalid source coordinates always fail independently of overflow policy. For `atlas_set`, `EXTEND` may grow **all** blocks' dimensions because blocks in one atlas have uniform shape.

### Resizing, redefining, cloning

```text
clm_storage_atlas_resize(id,width,height)                 # each block's dimensions
clm_storage_atlas_redefine(id,rows,columns)               # block-grid dimensions
clm_storage_atlas3d_resize(id,width,height,depth)
clm_storage_atlas3d_redefine(id,rows,columns,layers)
clm_storage_atlas_clone(source,destination)
clm_storage_atlas3d_clone(source,destination)
```

Resize and redefine preserve cells whose **exact block coordinate and local cell coordinate** still exist. They discard out-of-range cells and default-initialize newly addressable cells. Cloning copies dimensions and values into an existing same-kind destination, preserving the destination's resource ID and references. No automatic deep cloning of OBJECT values.

### Atlas3D slicing

`clm_storage_atlas3d_slice(source_atlas3d, destination_atlas, layer, depth=0, overflow=CANCEL)` copies one **block-grid layer** and one **local z-depth plane within every selected 3D block**, producing a 2D Atlas. Source and destination element types and sizes must match. Overflow policy applies to *both* per-block width/height and block-grid rows/columns.

### Destruction

`clm_storage_block_destroy`, `clm_storage_block3d_destroy`, `clm_storage_atlas_destroy`, and `clm_storage_atlas3d_destroy` check the resource kind and then destroy the resource. The generic `clm_storage_destroy` can destroy any kind.

## 8. Examples

```python
from clm_storage import *

DECK, DRAWN = 1001, 1002
clm_storage_create(DECK, STORAGE_TYPE_INTEGER, reserved=52)
clm_storage_create(DRAWN, STORAGE_TYPE_INTEGER)
for card in range(52):
    clm_storage_append(DECK, card)
clm_storage_shuffle(DECK)
status, card = clm_storage_draw(DECK)

WORLD, REGION = 2001, 2002
clm_storage_atlas_create(WORLD, width=16, height=16, rows=8, columns=8)
clm_storage_block_create(REGION, width=16, height=16)
clm_storage_block_set(REGION, 3, 4, 7)
clm_storage_atlas_set(WORLD, 2, 1, REGION)

clm_storage_unreference(DECK)
clm_storage_unreference(DRAWN)
clm_storage_unreference(WORLD)
clm_storage_unreference(REGION)
clm_storage_dispose()
```

## 9. Constraints, future work, and open contracts

- **No automatic memory tracing**: reference counts are manual; external raw pointers and ownership cycles cannot be discovered.
- **Not thread-safe**: the Python implementation and resource registry require caller-side synchronization, particularly with future `clm_managed` worker collection.
- **No guaranteed physical preallocation in Python**: logical `reserved` is a portability-contract hint; C/C++ ports should implement actual reservation.
- **No implicit deep object ownership**: copied objects share references, and disposal does not invoke custom destructors.
- **No automatic arbitrary struct packing**: DYNAMIC accepts fixed-size bytes, with field layout defined by the caller.
- **Not yet implemented**: layer insertion, view/non-copy block access, allocator controls, generation-tagged handles, and total-order floating-point comparator semantics. Grid-specific coordinate cursors, atlas navigation, and 2D-to-3D layer assignment are implemented.
- **Future specification questions**: should `destroy` reject positive references in a stricter version; how to expose allocation failures and deep-object policies consistently in C, Go, and Python.

The provided Python tests exercise linear storage, types, cursor behavior, copying, references, spending, 2D/3D cells, transfers, resizing, cloning and 3D layer slicing. Run them with `python -m unittest -v test_clm_storage test_clm_block test_clm_block3d`.


## Multidimensional cursor navigation (extension)

All Block, Block3D, Atlas, and Atlas3D resources use the existing `index` cursor.
The cursor is a flattened row-major index and is initialized to `-1`.
New operations include:

- `clm_storage_grid_set_position(id,x,y,z=0,column=0,row=0,layer=0)`
- `clm_storage_grid_get_position(id)` → `(status,(x,y,z,column,row,layer))`
- `clm_storage_grid_get_current(id)` → `(status,value)`
- `clm_storage_{block|block3d|atlas|atlas3d}_move_{first|last|next|previous}(id)`
- `clm_storage_{block|block3d|atlas|atlas3d}_set_position(...)` and `_get_position(id)`

`x,y,z` address cells **inside** one block, while `column,row,layer`
select a block within an atlas. For a 2D resource, `z=0` and `layer=0`.
The traversal order is x, y, local depth, atlas column, atlas row, atlas layer.
Explicitly indexed grid reads/writes do not move the cursor; invalid positions
return the existing invalid-index status. The cursor is not an iterator snapshot.

### Reverse Atlas3D slicing

`clm_storage_atlas3d_set_slice(source_atlas2d, destination_atlas3d, layer, depth=0, overflow=CANCEL)`
copies a 2D atlas into the selected atlas-grid layer and local block depth plane.
It preserves all other 3D layers/planes. Type and record-size compatibility are
mandatory; CANCEL rejects oversized sources, IGNORE clips, and EXTEND grows the
destination block/grid dimensions before copying. This is the counterpart of
`clm_storage_atlas3d_slice(...)` and does not swap `layer` with `depth`.

### Eligible-resource count

`clm_storage_pending_count()` returns the number of zero-reference storage
resources, useful for CLM Managed's optional threshold scheduler. It does not
free any resource. Python's `reserved` represents logical rather than guaranteed
physical allocation, and OBJECT copying remains shallow.


### Python return contract

`clm_storage_pending_count() -> integer` returns a plain nonnegative integer (not a `(status, count)` tuple). It counts registered resources with `references == 0`, including empty resources; it does not collect, mutate or change their reference counts. New resources start with one reference and are not pending until unreferenced. Explicit destruction removes a resource from the registry immediately. The count is a point-in-time snapshot and requires external synchronization if registries are shared across threads.

### Implemented multidimensional API details

`grid_set_position(id,x,y,z=0,column=0,row=0,layer=0)` sets a **cell** cursor.
`grid_get_position(id)` returns `(status,(x,y,z,column,row,layer))`;
`grid_get_current(id)` returns `(status,value)`. Unpositioned cursors return
`STORAGE_ERROR_INVALID_INDEX` without changing the underlying data.
Kind-specific position getters return only applicable coordinates:
Block `(x,y)`, Block3D `(x,y,z)`, Atlas `(x,y,column,row)`, and
Atlas3D `(x,y,z,column,row,layer)`. Wrong-kind operations return
`STORAGE_ERROR_INCOMPATIBLE`. Kind-specific first/last/next/previous
operations use row-major cell order and preserve their current position on
out-of-range movement. Explicit coordinate read/write does not move cursors.

`atlas3d_set_slice(source_atlas2d,destination_atlas3d,layer,depth=0,overflow=CANCEL)`
copies one complete 2D atlas into the selected grid layer and per-block z plane.
The destination atlas3d must exist, match type and element size, and be unlocked.
Only the target layer/plane is overwritten; other planes are preserved, except
that `EXTEND` can increase block width/height and atlas rows/columns.
`CANCEL` makes no modification if the source exceeds destination size; `IGNORE`
clips; `EXTEND` grows the destination to fit. The selected layer/depth must
already exist; overflow policy does not implicitly create target planes.
