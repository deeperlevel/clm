# CLM Base

## 1. Purpose

`clm_base` is the dependency-free foundation for CLM source bundles. It standardizes **numeric identifiers and fundamental fixed-size spatial value structures**. It does **not** allocate resources, manage name mappings, reserve IDs, or search registry collisions: those belong to `clm_id` and the owning modules.

## 2. Type contract

- Public alias: **`id_key`**.
- Default representation: **unsigned 64-bit integer** (`0` through `18,446,744,073,709,551,615`).
- Optional build/source configuration: **unsigned 32-bit integer** (`0` through `4,294,967,295`).
- Python: alias `id_key = int`, with configured range validation, not a wrapper class.
- All CLM public APIs that take or return **resource identity or keyed entity identity** should use `id_key` rather than platform-specific integer spellings.
- IDs and sequential element indices are distinct concepts; choosing an ID width does not mandate the same width for offsets or capacities.
- Configure once for an application. **All compiled dependent CLM modules must use the same alias width**; changing widths requires rebuilding the application and CLM libraries. Persisted binary IDs and native ABIs must agree on width.

## 3. API

### `make_id(value) -> id_key`

Canonical operation: `clm_base_make_id(value)`; languages may expose idiomatic `ClmBase.MakeId(value)` or `MakeId(value)`.

- Accept an **integer** representable by the widest documented input type (unsigned 64-bit). Some implementations additionally offer a signed overload/helper.
- Reject negative, non-integral, or out-of-range input **before** conversion when the language can observe those conditions.
- Return the identical numeric value as `id_key` on success; never clamp, wrap, saturate, or truncate.
- On error: **fail fast**, via the platform's documented exception, panic, or fatal error. Neither `0` nor the maximum ID is reserved as an error sentinel.
- Does not check `clm_id_is_used`, register, map, generate, or reserve the ID.

### Constants

- `ID_KEY_MIN`: `0`.
- `ID_KEY_MAX`: maximum of the configured unsigned integer width.

## 4. Examples

| Case | 64-bit default | 32-bit configuration |
|---|---|---|
| `make_id(1001)` | `1001` | `1001` |
| `make_id(0)` | `0` | `0` |
| `make_id(0xFFFFFFFF)` | Valid | Valid |
| `make_id(0x100000000)` | Valid | **Error** |
| `make_id(0xFFFFFFFFFFFFFFFF)` | Valid | **Error** |
| `make_id(-1)` | **Error** | **Error** |
| `make_id("1001")` | **Error** | **Error** |

`make_id()` checks **numeric type/range**, not resource collision. To check collisions, call `clm_id_is_used(id)` separately after constructing an ID.

## 5. Portability considerations

- **C:** `typedef uint64_t id_key;` or `uint32_t`. The unsigned `uint64_t` argument checks positive range. Due to C implicit conversions, negative *signed* arguments must be passed through the provided signed helper or prevalidated; never call the unsigned variant on negative input.
- **C++:** `using id_key = std::uint64_t;` (or `uint32_t`), constrained integral template accepting signed inputs and throwing `std::out_of_range` on negative/out-of-range values.
- **D:** `alias id_key = ulong;` or `uint`; signed and unsigned overloads.
- **Go:** `type IDKey = uint64` or `uint32`; idiomatic `MakeId` and signed helper. Go does not export lowercase identifiers to other packages.
- **C#:** `global using IdKey = System.UInt64;` or `UInt32`; `checked` narrowing conversion, `MakeIdSigned` for signed inputs; requires a C# compiler supporting global using aliases.
- **Odin:** `id_key :: u64` or `u32`; explicit conversion after range validation.
- **Pascal:** `id_key = UInt64` or `UInt32`; throws `ERangeError` on invalid values; Free Pascal syntax provided.
- **Python:** `id_key = int`, rejects booleans, non-integers, negatives, and IDs greater than `(1 << ID_KEY_BITS) - 1`.

Unsigned source conversions can lose signedness **before a function sees the input**. Explicit signed helpers and language-level checked conversions are included to avoid silently accepting negative IDs. C# and Pascal source syntax may need minor adaptation for older compiler versions.

## 6. Conformance tests

Every language port should validate `0`, `1001`, maximum, rejection of overrange positive inputs under the 32-bit configuration, rejection of negative inputs via signed entrypoints, and correctness of the return type. There should be no collision checks or registry imports in `clm_base`.

## 7. Spatial value structures

`clm_base` owns the twelve **mutable, independent, plain-value** structures below. No inheritance, resource registration, or dynamic arrays are involved. Each field is explicit, and the listed order is normative for constructors and native struct/record layouts.

| Structure | Ordered fields | Field type |
|---|---|---|
| `Position` | `x, y` | signed 32-bit integer |
| `PositionF` | `x, y` | 32-bit float |
| `Position3D` | `x, y, z` | signed 32-bit integer |
| `Position3DF` | `x, y, z` | 32-bit float |
| `Size` | `width, height` | signed 32-bit integer |
| `SizeF` | `width, height` | 32-bit float |
| `Size3D` | `width, height, depth` | signed 32-bit integer |
| `Size3DF` | `width, height, depth` | 32-bit float |
| `Block` | `x, y, width, height` | signed 32-bit integer |
| `BlockF` | `x, y, width, height` | 32-bit float |
| `Block3D` | `x, y, z, width, height, depth` | signed 32-bit integer |
| `Block3DF` | `x, y, z, width, height, depth` | 32-bit float |

- The `F` suffix designates **IEEE 754 binary32** in compiled implementations. Python's built-in `float` is typically binary64, so annotations do not enforce binary32; convert explicitly at binary interchange boundaries.
- Native integer structures use exactly signed 32-bit values (`int32_t`, `int32`, `Int32`, etc.), not configurable `id_key`. Python's `int` annotation does not enforce range.
- Fields are public/mutable and may contain zero or negative values. Geometric validity belongs to specialized operations.
- `Block` denotes spatial origin and extent. Circle, Sphere, and advanced shapes belong to `clm_shapes` rather than `clm_base`.
- Go intentionally capitalizes exported field names (`X`, `Y`, `Width`, etc.), since lowercase fields cannot be accessed from another package; this is an idiomatic spelling difference, not a semantic difference.
- C and C++ structures are plain structs; C# uses mutable structs, Pascal uses records, D and Odin use structs, Python uses dataclasses.

### Construction examples

```python
from clm_base import Block, Position3DF, Block3DF
rectangle = Block(10, 20, 100, 200)
position = Position3DF(1.0, 2.5, 3.0)
volume = Block3DF(0.0, 0.0, 0.0, 10.0, 20.0, 30.0)
```

```c
Block rectangle = {10, 20, 100, 200};
Position3DF position = {1.0f, 2.5f, 3.0f};
```

Existing `clm_block` and `clm_block3d` Python modules should re-export `Block` and `Block3D` from `clm_base` to prevent duplicate class identities.
