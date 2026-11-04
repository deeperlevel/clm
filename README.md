# Cross-Language-Modules (CLM)

**Reusable, composable foundational modules with consistent observable capabilities across programming languages.**

CLM is a language-neutral project. Each language implementation can use native data structures, error handling, platform APIs and allocation strategies while preserving the published behavior of each module. CLM is not tied to SPIder, a specific language runtime or a single source repository.

## Repositories

- [CLM — language-neutral architecture, contracts and documentation](https://github.com/deeperlevel/clm)
- [CLM for C — reference implementation](https://github.com/deeperlevel/clm-c)

The main repository is intended to own the shared specifications; implementation-specific source belongs in the corresponding language repositories.

## Design principles

- **Module boundaries:** Each module has a specific responsibility and explicit dependencies.
- **Cross-language parity:** Portable behavior and results matter more than identical signatures or internal representations.
- **ID-oriented contracts where appropriate:** Resource identifiers have documented ownership and lifetime semantics; not every capability requires an ID registry.
- **Implementation flexibility:** Third-party codecs, graphics APIs, TLS backends and memory strategies may differ across language implementations.
- **Explicit capability status:** A declared API does not guarantee every backend is implemented.

## Documentation

- [Browse the complete module index](docs/MODULES.md)
- [Read the distinction between modules and internal headers](docs/IMPLEMENTATION_HEADERS.md)

## Module catalog

This catalog describes **90 current language-neutral module interfaces** as represented by the C header snapshot of **2026-11-05**. Every module links to a corresponding document in [`docs/`](docs/MODULES.md). The C implementation is one reference, not the required implementation architecture for other languages.

### Core identifiers, platform and utilities

| Module | Responsibility |
|---|---|
| [`clm_base`](docs/clm_base.md) | CLM’s foundational vocabulary of numeric IDs, numeric ranges, scalar types and small coordinate/size/rectangle-like value structures. |
| [`clm_clp`](docs/clm_clp.md) | A command-line parser that turns option names, flags and values into a structured lookup representation. |
| [`clm_console`](docs/clm_console.md) | Console text input/output, formatted object display and optional terminal styling. |
| [`clm_crypto`](docs/clm_crypto.md) | Cryptographic primitives for authenticated encryption, key material and secure random operations. |
| [`clm_datetime`](docs/clm_datetime.md) | Date, time and timestamp values and formatting |
| [`clm_gen`](docs/clm_gen.md) | General-purpose generation of random values, identifiers and textual tokens. |
| [`clm_hash`](docs/clm_hash.md) | Computes supported message digests and provides hash formatting/conversion utilities. |
| [`clm_id`](docs/clm_id.md) | A named-ID registry associating human-readable names with numeric CLM identifiers. |
| [`clm_platform`](docs/clm_platform.md) | Exposes consistent descriptors of the current operating system, architecture and runtime platform. |
| [`clm_uuid`](docs/clm_uuid.md) | UUID validation, generation and formatting |

### Memory, resources and collections

| Module | Responsibility |
|---|---|
| [`clm_arena`](docs/clm_arena.md) | clm_arena manages ID-addressed, growable bump arenas: contiguous regions of memory in which allocations are identified by stable byte… |
| [`clm_bpa`](docs/clm_bpa.md) | Block Pool Allocation: ID-addressed, typed blocks and arrays. |
| [`clm_bytes`](docs/clm_bytes.md) | Binary-safe, owned, growable byte buffers and non-owning byte views, independent of text encoding. |
| [`clm_compress`](docs/clm_compress.md) | Compression and decompression of bytes, streams and in-memory ZIP archives using interoperable standard encodings. |
| [`clm_container`](docs/clm_container.md) | ID-based service registration and resolution, with explicit dependency validation and instance lifetimes. |
| [`clm_dictionary`](docs/clm_dictionary.md) | ID-keyed dictionary resources for typed and heterogeneous values. |
| [`clm_ecs`](docs/clm_ecs.md) | An entity-component-system registry with reusable blueprints, entity instances, components and systems. |
| [`clm_managed`](docs/clm_managed.md) | A policy-driven coordinator for housekeeping and resource cleanup through configurable subsystem hooks. |
| [`clm_resource`](docs/clm_resource.md) | Shared resource kinds and reference lifecycles |
| [`clm_storage`](docs/clm_storage.md) | ID-addressed fixed-dimension data grids and texture-atlas-like regions. |
| [`clm_stream`](docs/clm_stream.md) | Reusable source/sink adapters and incremental data pumps for transferring binary data. |
| [`clm_table`](docs/clm_table.md) | ID-based mutable tabular collection with rows, columns and string-valued cells. |
| [`clm_tree`](docs/clm_tree.md) | ID-based hierarchical representation of objects, arrays and values. |

### Files and paths

| Module | Responsibility |
|---|---|
| [`clm_directory`](docs/clm_directory.md) | Filesystem directory creation, traversal, querying, copying and removal. |
| [`clm_file`](docs/clm_file.md) | File-oriented filesystem operations for reading, writing, inspection and metadata management. |
| [`clm_path`](docs/clm_path.md) | Platform-aware filesystem path inspection and manipulation without directly performing file I/O. |

### Text, structured data and templating

| Module | Responsibility |
|---|---|
| [`clm_string`](docs/clm_string.md) | Managed UTF-8/text-oriented string blocks and arrays with editing and encoding utilities. |
| [`clm_string_dsv`](docs/clm_string_dsv.md) | Delimited-separated-value parsing and serialization into CLM tables. |
| [`clm_string_env`](docs/clm_string_env.md) | Reads and writes environment-file-style key/value text into CLM structured data. |
| [`clm_string_format`](docs/clm_string_format.md) | Shared low-level parsing and formatting utilities used by CLM structured-text format modules. |
| [`clm_string_ini`](docs/clm_string_ini.md) | INI text parsing and serialization into CLM trees. |
| [`clm_string_json`](docs/clm_string_json.md) | JSON parsing and writing through CLM’s common structured-tree model. |
| [`clm_string_markup`](docs/clm_string_markup.md) | Markup-to-tree conversion for XML- and HTML-like text |
| [`clm_string_toml`](docs/clm_string_toml.md) | TOML text parsing and serialization into CLM trees. |
| [`clm_string_yaml`](docs/clm_string_yaml.md) | YAML text parsing and serialization into CLM trees. |
| [`clm_stringtify`](docs/clm_stringtify.md) | Converts ID-based CLM-managed objects into readable text for display and diagnostics. |
| [`clm_template`](docs/clm_template.md) | Compiles and renders lightweight text templates using named data contexts. |

### Concurrency and dispatch

| Module | Responsibility |
|---|---|
| [`clm_async`](docs/clm_async.md) | A cooperative scheduler for stackless tasks. |
| [`clm_async_pool`](docs/clm_async_pool.md) | A multiworker execution layer built on cooperative task schedulers. |
| [`clm_channel`](docs/clm_channel.md) | Bounded concurrent message channels for exchanging integer IDs between producers and consumers. |
| [`clm_dispatch`](docs/clm_dispatch.md) | Dispatches commands, events, queries and exceptions through registered handlers indexed by message ID. |

### Networking, transport and HTTP

| Module | Responsibility |
|---|---|
| [`clm_http_utils`](docs/clm_http_utils.md) | Transport-independent HTTP message and framing utilities, including HTTP/1.x syntax, chunked coding, QUIC variable-length integers and normalized decoded HTTP/3 headers. |
| [`clm_multipart`](docs/clm_multipart.md) | Parses and generates MIME multipart bodies with explicit boundary handling. |
| [`clm_net`](docs/clm_net.md) | TCP/UDP networking primitives for addresses, sockets and readiness-based I/O. |
| [`clm_net_quic`](docs/clm_net_quic.md) | Socket-independent QUIC connection and stream engine |
| [`clm_net_web`](docs/clm_net_web.md) | Shared HTTP server application layer for HTTP/1.1 and HTTP/2, with a separate HTTP/3 transport facade that reuses the same routes, middleware and handler abstractions. |
| [`clm_net_web_client`](docs/clm_net_web_client.md) | Asynchronous HTTP client with selectable protocol |
| [`clm_net_web_client_h1`](docs/clm_net_web_client_h1.md) | HTTP/1.1-only asynchronous HTTP client facade |
| [`clm_net_web_client_h2`](docs/clm_net_web_client_h2.md) | HTTP/2-only asynchronous HTTPS client facade |
| [`clm_net_web_client_h3`](docs/clm_net_web_client_h3.md) | HTTP/3-only asynchronous HTTPS client facade |
| [`clm_net_web_h1`](docs/clm_net_web_h1.md) | HTTP/1.1-only web server facade |
| [`clm_net_web_h2`](docs/clm_net_web_h2.md) | HTTP/2-only HTTPS web server facade |
| [`clm_net_web_h3`](docs/clm_net_web_h3.md) | HTTP/3-only UDP web server facade |
| [`clm_net_websocket`](docs/clm_net_websocket.md) | Protocol-aware WebSocket client connection |
| [`clm_tls`](docs/clm_tls.md) | TLS-encrypted client/server sessions layered over connected TCP sockets. |
| [`clm_ua`](docs/clm_ua.md) | Heuristic parsing of HTTP User-Agent strings. |
| [`clm_url`](docs/clm_url.md) | URI, URL and IRI parsing, reconstruction and normalization utilities. |
| [`clm_websocket`](docs/clm_websocket.md) | WebSocket frame and message codec |

### Mathematics and simulation

| Module | Responsibility |
|---|---|
| [`clm_math`](docs/clm_math.md) | Scalar convenience mathematics forming the lightweight high-level layer above shared math conventions. |
| [`clm_math_calculus`](docs/clm_math_calculus.md) | Numerical differentiation, integration and differential equations |
| [`clm_math_collision2d`](docs/clm_math_collision2d.md) | Two-dimensional intersection and sweep queries |
| [`clm_math_collision3d`](docs/clm_math_collision3d.md) | Three-dimensional intersection and sweep queries |
| [`clm_math_core`](docs/clm_math_core.md) | Shared math result codes and storage-adapter contract for ID-addressed numeric arrays. |
| [`clm_math_geometry`](docs/clm_math_geometry.md) | Foundational 2D geometry and polygon queries. |
| [`clm_math_interpolation`](docs/clm_math_interpolation.md) | Interpolation and easing for scalars and stored numeric sequences. |
| [`clm_math_matrix`](docs/clm_math_matrix.md) | Matrix operations and practical 2D transforms using numeric storage IDs. |
| [`clm_math_number`](docs/clm_math_number.md) | Integer number theory and modular arithmetic utilities. |
| [`clm_math_pathfinding2d`](docs/clm_math_pathfinding2d.md) | A* routing in two-dimensional spaces |
| [`clm_math_pathfinding3d`](docs/clm_math_pathfinding3d.md) | A* routing in three-dimensional spaces |
| [`clm_math_physics2d`](docs/clm_math_physics2d.md) | Small 2D motion helpers suitable for lightweight simulations and games. |
| [`clm_math_physics3d`](docs/clm_math_physics3d.md) | Lightweight three-dimensional motion calculations |
| [`clm_math_quaternion`](docs/clm_math_quaternion.md) | Quaternion-based 3D orientation and vector rotation helpers. |
| [`clm_math_random`](docs/clm_math_random.md) | Deterministic seeded pseudorandom operations for simulations and reproducible calculations. |
| [`clm_math_spatial`](docs/clm_math_spatial.md) | Grid and Morton addressing, tile/isometric/hex projections, spatial distances, immutable point-radius indexing, and deterministic 2D/3D value noise. |
| [`clm_math_vector`](docs/clm_math_vector.md) | ID-based vector arithmetic and geometric operations. |

### Windowing, UI, drawing and media

| Module | Responsibility |
|---|---|
| [`clm_audio`](docs/clm_audio.md) | Audio playback with independent transport controls |
| [`clm_box`](docs/clm_box.md) | Editable three-dimensional box shapes in spatial managers |
| [`clm_camera`](docs/clm_camera.md) | Reusable camera configuration resources |
| [`clm_drawing`](docs/clm_drawing.md) | Color values and drawing-related primitives |
| [`clm_font`](docs/clm_font.md) | Font source assets and face selection |
| [`clm_image`](docs/clm_image.md) | CPU-resident raster image data |
| [`clm_image_png`](docs/clm_image_png.md) | PNG encoding/decoding for CLM images |
| [`clm_lighting`](docs/clm_lighting.md) | Lights and light-collection descriptions |
| [`clm_panel`](docs/clm_panel.md) | Interactive and drawable rectangular regions attached to surfaces |
| [`clm_renderer`](docs/clm_renderer.md) | Backend-selectable drawing on surfaces and panels |
| [`clm_scene`](docs/clm_scene.md) | Scene identity and description (provisional graphics interface) |
| [`clm_shader`](docs/clm_shader.md) | Stored shader source or binary assets |
| [`clm_spatial_manager`](docs/clm_spatial_manager.md) | Invisible 3D geometry registry and query system |
| [`clm_sphere`](docs/clm_sphere.md) | Editable 3D sphere shapes in spatial managers |
| [`clm_surface`](docs/clm_surface.md) | Cross-platform drawable surfaces and input/event lifecycle |
| [`clm_texture`](docs/clm_texture.md) | Reusable texture source data and image-to-texture preparation |

**Historical/renamed entries:** [`clm_depend`](docs/clm_depend.md), [`clm_format`](docs/clm_format.md), [`clm_formats`](docs/clm_formats.md), [`clm_ini`](docs/clm_ini.md), [`clm_json`](docs/clm_json.md), [`clm_math_collision`](docs/clm_math_collision.md), [`clm_math_pathfinding`](docs/clm_math_pathfinding.md), [`clm_toml`](docs/clm_toml.md), [`clm_yaml`](docs/clm_yaml.md). These are retained as compatibility/migration references and are not counted as current modules.

**Backend-specific headers:** See [`docs/IMPLEMENTATION_HEADERS.md`](docs/IMPLEMENTATION_HEADERS.md).

## Cross-language conformance

The module documents describe capability and behavioral expectations rather than binding every language to C signatures. Implementations should document supported features, deviations, platform requirements, error outcomes and resource-lifetime behavior. Experimental or graphics-only stubs, such as the current `clm_scene` reference, should not be presented as a finalized CLM-wide contract.

## License

The supplied CLM C headers state that CLM-authored code is dedicated to the public domain under **CC0 1.0 Universal**, except where a file states otherwise. Bundled `ext/` and `vendor/` code retains its original third-party license. Check each repository's authoritative license file for the full terms.
