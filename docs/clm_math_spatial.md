# CLM Math Spatial

> **Current interface reference** · Language-neutral capability and behavioral contract · Based on the `math_spatial.h` header supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md). This supplies shared vector values and math result/status conventions; this module does **not** require the numeric-array storage adapter to perform its operations.
- **External libraries / platform facilities (C reference):** No mandatory third-party library is declared by the supplied header. Native mathematical operations and allocation facilities may be used by an implementation.

**Portability rule:** These are reference-implementation dependencies, not a requirement to reproduce a C include structure. Each language implementation should preserve the observable data, coordinate, error, and ownership contracts while using native representations where appropriate.

## Purpose

`clm_math_spatial` provides storage-independent spatial mathematics for tile maps, grids, 2D and 3D worlds, hexagonal maps, spatial queries, and procedural value noise. It bridges discrete cell addressing and continuous positions without requiring a rendering engine, entity registry, or CLM-managed object IDs.

## Capabilities and operations

- **Dense grid addresses:** validate element counts and translate between 3D coordinates and dense one-dimensional indices.
- **Morton codes:** encode/decode integer coordinates as locality-preserving bit-interleaved spatial addresses.
- **Tile projections:** transform between continuous orthogonal or diamond-isometric tile coordinates and screen coordinates, including discrete cell selection.
- **Elevated isometric (2.5D):** project 3D tile-space positions into 2D screen space and invert them on a caller-specified height plane.
- **World-to-cell conversion:** map continuous 3D world positions onto signed integer cells.
- **Hexagonal grids:** convert pointy-top or flat-top axial hex coordinates to/from screen positions and compute hex-step distance.
- **Distance metrics:** compute squared Euclidean, Manhattan, Chebyshev and supported octile distances.
- **Point proximity index:** build an immutable uniform-grid snapshot of 2D/3D point sets and query inclusive circular/spherical neighborhoods.
- **Procedural noise:** evaluate deterministic seeded 2D/3D lattice value noise and normalized fractal Brownian motion (fBm).

## Coordinate and data model

The module uses a small set of **values**, rather than managed CLM resource identifiers:

| Concept | Shared meaning |
|---|---|
| Grid dimensions / dense coordinate | Three nonnegative integer components `(x, y, z)`; dimensions must have nonzero extents. For 2D grids, use depth `1` and cell layer `z = 0`. |
| Signed cell address | Three signed 32-bit integer components; supports cell coordinates on either side of the world origin. |
| Continuous position | Two- or three-component double-precision floating-point vector, using the common math value conventions. |
| Axial hex address | Two signed 32-bit integer components `(q, r)`; implicit cube coordinate `s = -q - r`. |
| Projection configuration | A screen origin, tile width/height, and an orthogonal or diamond-isometric projection mode. |
| Elevated isometric configuration | A screen origin, full ground-diamond width/height and positive screen-pixels-per-height-unit elevation scale. |
| Spatial-index snapshot | An owned, immutable set of copied points, divided into sorted uniform-grid cells. |
| Noise seed | Immutable unsigned 64-bit seed value, with no shared mutable generator state. |

Screen coordinates increase **rightward and downward**. Ground-grid coordinates and height are separate: positive height in elevated-isometric projection moves the image upward on screen. Negative and fractional ground positions are supported where the relevant operation permits them.

## Grid indexing and Morton addressing

- **Element count** multiplies all three strictly positive grid extents and reports an error if the product is not representable.
- **Dense flattening** uses an **X-fastest** layout:

  ```text
  index = x + width * (y + height * z)
  ```

  The complete grid layout and target coordinates must be valid; unflattening requires an index below the validated element count.
- **2D Morton addresses** interleave all 32 bits of both unsigned coordinates into 64 bits and can be reversed exactly.
- **3D Morton addresses** interleave 21 bits per coordinate into a 63-bit result; each component must be at most `0x1FFFFF`, and bit 63 must be clear when decoding.
- A Morton code is a **sparse spatial address**, not a substitute for a dense-array index at arbitrary rectangular extents.

## Tile, world and isometric projection

Orthogonal tile projection maps the tile grid directly into screen-space tile dimensions. Diamond-isometric projection uses the **full width and height of a diamond** and maps the origin tile to the screen origin. With tile dimensions `w` and `h`, a tile position `(x, y)` projects relative to the origin as:

```text
screen_x = (x - y) * w / 2
screen_y = (x + y) * h / 2
```

The inverse projection returns **fractional** tile coordinates rather than rounding them. Cell selection floors both components, including toward negative infinity for negative values, and returns layer `z = 0`.

**Elevated isometric (2.5D)** uses a 3D source position `(x, y, z)` but a 2D screen destination:

```text
screen_x = origin_x + (x - y) * w / 2
screen_y = origin_y + (x + y) * h / 2 - z * elevation_scale
```

The layout's tile dimensions and elevation scale must be finite and positive. At height zero, this matches ordinary diamond-isometric projection. The inverse requires the caller to supply the **known horizontal height plane**; one 2D screen point cannot uniquely reveal a 3D position. A cell-selection variant floors X, Y and supplied height into a signed tile/layer address. **No perspective, raycasting, terrain lookup, occlusion, or visibility resolution is implied.**

**World-to-cell** conversion applies `floor((world - origin) / cell_size)` independently to X/Y/Z, with a finite positive cell size and representable signed integer output.

## Hexagonal grids

Hex addresses use axial integer coordinates `(q, r)` with an implicit cube coordinate. Conversion supports **pointy-top** and **flat-top** hexes. The supplied radius is the center-to-corner distance in screen units, and the projection uses the right/down screen convention.

Screen-to-hex conversion selects the nearest axial hex using **cube rounding**; ties at boundaries are deterministic. Hex distance reports the exact shortest number of hex steps in an unsigned 64-bit result without signed-overflow intermediate calculations.

## Distances and pathfinding semantics

| Metric | Behavior |
|---|---|
| Squared Euclidean | Sum of squared coordinate differences; useful for comparing distances without a square root. |
| Manhattan | Sum of absolute axis differences. |
| Chebyshev | Maximum absolute axis difference. |
| Octile (2D only) | Assumes cost `1` per orthogonal step and `sqrt(2)` per diagonal step. |

The 3D distance operation does **not** define octile distance: requesting it, or supplying an unknown metric, yields a not-a-number result. The 2D operation likewise yields not-a-number for an unknown metric. These distance functions do not perform their own input validation; callers must supply representable values and arithmetic.

**Do not use squared Euclidean distance as an admissible heuristic for unit-cost A\* without first verifying the search cost model.** The module supplies mathematical distance functions, not a pathfinding solver.

## Immutable point index and radius queries

The point index is an **owned snapshot** built from a collection of 2D or 3D positions. It copies input points and organizes them in a sorted uniform grid, with a finite positive cell size and signed 32-bit cell addresses. Empty input is valid; repeated/coincident positions are retained as distinct entries.

- Building or rebuilding typically costs **O(N log N)** time and **O(N)** memory. A failed replacement preserves an existing snapshot.
- A snapshot is immutable once built. Concurrent **read queries** are permitted; rebuilding or destroying it must be synchronized against readers.
- Radius queries are **inclusive** (points on the circle/sphere boundary qualify). For a 2D snapshot, the query center's Z coordinate is ignored.
- Results are **original zero-based input-array indices**, not CLM object IDs. Ordering is deterministic: by cell/address and then original input index.
- A count-only query is supported without an output index buffer. If the provided result buffer is too small, the operation reports a dimension/capacity error, reports the required count, and writes no result indices.
- Point coordinates are finite with magnitude at most `1e100`, and indexed cell coordinates must fit signed 32-bit integers. Query centers have the same magnitude restriction; radius is finite in `[0, 1e100]`.
- Typical query work is **O(C log N + K)** for visited cells `C` and candidate points `K`; broad queries may scan **O(N)** points rather than enumerating huge empty ranges. Dense cells and worst cases are not constant-time operations.

This index is a **point-proximity utility**, not an object registry, physics broadphase contract, or a replacement for [clm_spatial_manager](clm_spatial_manager.md).

## Deterministic value noise and fractal Brownian motion

A noise configuration is an immutable **64-bit seed**. Any seed is valid; noise hashing does not depend on the storage-based random generator and requires no global mutable random state.

- **Value noise:** 2D and 3D lattice-based, quintic-interpolated values in `[-1, 1]`. All sampled lattice coordinates must stay within `[−2³¹, 2³¹ − 1)`; nonfinite inputs are rejected.
- **Normalized fBm:** combines value-noise octaves and returns a result in `[-1, 1]`. Number of octaves is **1–32**, lacunarity is **at least 1**, and gain is in **[0, 1]**; all settings must be finite. Each octave scales sample coordinates by lacunarity and must remain representable.

Seeded evaluation must be repeatable for the same seed, coordinates and configuration **within an implementation**. Exact bitwise agreement across languages or floating-point environments is **not asserted by the supplied interface alone**; if cross-language sample parity becomes a requirement, CLM should specify a reference hash, interpolation and rounding procedure with test vectors.

## Validation, failures and ownership

Most operations return an explicit math outcome. The reference interface distinguishes:

| Outcome | Meaning in this module |
|---|---|
| Success | Operation completed and its outputs are valid. |
| Value error | Null/invalid inputs or nonfinite coordinates/settings where validation is required. |
| Dimension error | Zero/invalid grid extents, out-of-range indices, unrepresentable signed cell addresses, invalid or insufficient result buffers, or layout overflow. |
| Domain error | Invalid scale/radius constraints or noise settings (where the operation specifies a domain restriction). |
| Allocation error | Memory unavailable for an operation that allocates, notably point-index construction. |

**Output preservation:** Failed status-returning operations leave writable outputs unchanged **except** a radius query with insufficient result capacity, which updates its required-count output but does not write partial indices. Output buffers and required-count storage must not alias where disallowed by the reference contract. Some scalar distance and encoding functions return their result directly rather than reporting a status.

**Ownership:** A point-index snapshot owns a copy of its input points and must be explicitly released. Successful replacement transfers the reference to the new snapshot; failure leaves the previous snapshot intact. Callers must avoid making uncontrolled copies of an owning reference. Simple coordinates, metrics, projections and noise seeds are value data.

## Cross-language interoperability

Language ports may use native vectors, integer types, error objects, owned collections and synchronization mechanisms. They should retain the following **observable** conventions:

- Coordinate orientation, layout formulas, floor behavior, signed cell range and hex rounding rules.
- Defined Morton bit capacities and reversible encoding/decoding.
- The distinction between dense indices, sparse Morton addresses, input-array indices and CLM-managed object IDs.
- Numeric validation, supported distance metrics and meaningful failure categories.
- Point-index snapshot ownership, stable input indices, inclusive radius boundary, deterministic result order and count-only/capacity-failure behavior.
- Repeatable noise behavior and normalized output ranges, without assuming unpromised bit-identical results across floating-point runtimes.

No renderer, graphics API, scene system or storage adapter is necessary to use these operations.

## Example use cases

- Select a tile beneath a pointer in orthogonal, diamond-isometric or layered 2.5D views.
- Map negative world coordinates to chunks and grid cells without rounding toward zero.
- Address sparse voxel regions with Morton codes while using dense row-major indices for individual arrays.
- Find nearby points of interest or agents in a small, read-mostly set without a full object spatial manager.
- Construct deterministic hex-map distance rules or terrain-height/value-noise inputs for procedural world generation.

## Related CLM modules

- [clm_math_core](clm_math_core.md) — Shared mathematical value and status conventions (**direct dependency**).
- [clm_math_pathfinding2d](clm_math_pathfinding2d.md) and [clm_math_pathfinding3d](clm_math_pathfinding3d.md) — Path searches that may use compatible grid concepts (**related, not required dependencies**).
- [clm_math_geometry](clm_math_geometry.md) — Other geometric primitives and predicates (**related**).
- [clm_spatial_manager](clm_spatial_manager.md) — ID-based 3D shape registry and spatial queries, with a different responsibility and ownership model (**related**).
- [clm_math_random](clm_math_random.md) — Separate pseudorandom operations; this module's seeded noise is independent (**related**).
