# Cross-Language Modules (CLM)

**Reusable, composable foundational modules with consistent observable capabilities across programming languages.**

CLM is designed as a language-neutral collection of building blocks for applications, services, tools, and games. Implementations may use native language facilities, platform APIs, and memory-management strategies while preserving the documented behavior of each module.

> **Development status:** CLM is currently an internal TST technology. This documentation is a technical working reference, not a public release or availability announcement.

## Design principles

- **Clear module boundaries:** Each capability has a defined responsibility and explicit dependencies.
- **Cross-language parity:** Behavioral contracts and observable results matter more than identical syntax or internal representations.
- **Identifiers where useful:** Resource-oriented modules may use IDs with documented ownership and lifetime rules; other modules need not.
- **Implementation independence:** Graphics, networking, storage, cryptography, and platform integration may vary between implementations.
- **Explicit capability status:** An interface definition does not guarantee support in every language or on every platform.
- **Composable foundations:** Applications can select only the capabilities they need without adopting an entire application framework or game engine.

## Documentation

- [Module index and language-neutral overviews](docs/modules/README.md) — purposes, capabilities, and intended behavior.
- [Implementation-specific references](docs/IMPLEMENTATIONS.md) — links to language bindings and their API documentation.
- [Documentation audit](docs/DOCUMENTATION_AUDIT.md) — snapshot coverage and qualifications.

## Module catalog

The catalog lists documented CLM capabilities based on the current reference-implementation snapshot. A listed interface is not necessarily a separately distributable or universally implemented module. Consult each module overview for its contract and status.

### Core and runtime (9)

| Module | Responsibility |
|---|---|
| [`clm_arena`](docs/modules/arena.md) | ID-addressed bump allocation with explicit, checked borrow leases. |
| [`clm_bpa`](docs/modules/bpa.md) | Process-global ID-indexed storage for typed blocks and growable arrays. |
| [`clm_clp`](docs/modules/clp.md) | Command-line tokenization and option-to-value mapping. |
| [`clm_gen`](docs/modules/gen.md) | Cryptographically randomized identifiers, ranges, text and passwords. |
| [`clm_id`](docs/modules/id.md) | Name-to-ID registration and ID lookup. |
| [`clm_managed`](docs/modules/managed.md) | Optional coordinator for application-owned deferred cleanup routines. |
| [`clm_platform`](docs/modules/platform.md) | Compile-target identity and portable environment constants. |
| [`clm_resource`](docs/modules/resource.md) | Shared resource statuses and resource-lifecycle entry points. |
| [`clm_types`](docs/modules/types.md) | Fixed-width CLM scalar aliases and plain data geometry types. |

### Collections and data (13)

| Module | Responsibility |
|---|---|
| [`clm_async`](docs/modules/async.md) | Cooperative task scheduling and task lifecycle notifications. |
| [`clm_async_pool`](docs/modules/async_pool.md) | Worker pools layered over cooperative schedulers. |
| [`clm_box`](docs/modules/box.md) | Oriented box shapes registered with a spatial manager. |
| [`clm_bytes`](docs/modules/bytes.md) | Binary-safe, growable buffers and non-owning byte-range views. |
| [`clm_channel`](docs/modules/channel.md) | Concurrent ID channels with buffered and rendezvous transfers. |
| [`clm_container`](docs/modules/container.md) | Explicit service registration, resolution, and scoped instances. |
| [`clm_dictionary`](docs/modules/dictionary.md) | ID-keyed dictionaries backed by typed BPA arrays. |
| [`clm_dispatch`](docs/modules/dispatch.md) | Command, event, and query dispatch with service resolution. |
| [`clm_ecs`](docs/modules/ecs.md) | Entity blueprints, component storage, and systems. |
| [`clm_storage`](docs/modules/storage.md) | ID-addressed fixed-dimension data storage and region management. |
| [`clm_stream`](docs/modules/stream.md) | Byte sources, sinks, adapters, and incremental copying. |
| [`clm_table`](docs/modules/table.md) | ID-keyed tabular rows, columns, and cell values. |
| [`clm_tree`](docs/modules/tree.md) | Nested object, array, and scalar values backed by CLM storage. |

### Files and serialization (19)

| Module | Responsibility |
|---|---|
| [`clm_directory`](docs/modules/directory.md) | Directory creation, traversal, inspection, copying, and removal. |
| [`clm_file`](docs/modules/file.md) | Path IDs reference UTF-8 BPA strings; open-file IDs identify independent native resources. _as accepts raw UTF-8 paths, _to exports caller-owned data, and _from consumes caller-owned. |
| [`clm_hash`](docs/modules/hash.md) | Deterministic message digests, HMAC and digest-to-text utilities. |
| [`clm_path`](docs/modules/path.md) | Filesystem-path API using UTF-8 strings referenced by CLM IDs. |
| [`clm_re`](docs/modules/re.md) | Portable reusable byte-oriented regular expressions for CLM. |
| [`clm_string`](docs/modules/string.md) | ID-backed strings, text encodings, and string operations. |
| [`clm_string_base64`](docs/modules/string_base64.md) | Base64 encoding and decoding. |
| [`clm_string_dsv`](docs/modules/string_dsv.md) | Delimited-record parsing and generation. |
| [`clm_string_env`](docs/modules/string_env.md) | Environment-style key/value parsing and generation. |
| [`clm_string_format`](docs/modules/string_format.md) | Formatted string construction and format-specific helpers. |
| [`clm_string_ini`](docs/modules/string_ini.md) | INI sections and key/value parsing and generation. |
| [`clm_string_json`](docs/modules/string_json.md) | JSON parsing, escaping, and tree serialization. |
| [`clm_string_markup`](docs/modules/string_markup.md) | Markup parsing and markup-oriented tree conversion. |
| [`clm_string_toml`](docs/modules/string_toml.md) | TOML document parsing and generation. |
| [`clm_string_yaml`](docs/modules/string_yaml.md) | YAML document parsing and generation. |
| [`clm_stringtify`](docs/modules/stringtify.md) | Human-readable formatting of CLM values and collections. |
| [`clm_template`](docs/modules/template.md) | Template parsing, expression evaluation, and rendering. |
| [`clm_url`](docs/modules/url.md) | The archive provides declarations for this module; consult the C interface and header comments for its supported behavior. |
| [`clm_uuid`](docs/modules/uuid.md) | UUID generation, parsing, formatting, and field inspection. |

### Math and geometry (28)

| Module | Responsibility |
|---|---|
| [`clm_camera`](docs/modules/camera.md) | Camera view and projection configuration. |
| [`clm_geometry2d`](docs/modules/geometry2d.md) | The archive provides declarations for this module; consult the C interface and header comments for its supported behavior. |
| [`clm_geometry3d`](docs/modules/geometry3d.md) | The archive provides declarations for this module; consult the C interface and header comments for its supported behavior. |
| [`clm_math`](docs/modules/math.md) | Scalar mathematical convenience functions. |
| [`clm_math_calculus`](docs/modules/math_calculus.md) | Finite-difference derivatives, quadrature, and ODE steps. |
| [`clm_math_collision2d`](docs/modules/math_collision2d.md) | Two-dimensional overlap, ray, segment, and sweep tests. |
| [`clm_math_collision3d`](docs/modules/math_collision3d.md) | Three-dimensional overlap, ray, segment, and sweep tests. |
| [`clm_math_core`](docs/modules/math_core.md) | Shared mathematical value types and storage adapters. |
| [`clm_math_geometry2d`](docs/modules/math_geometry2d.md) | Two-dimensional geometric measurements, bounds, and transforms. |
| [`clm_math_geometry3d`](docs/modules/math_geometry3d.md) | Three-dimensional geometric measurements, bounds, and transforms. |
| [`clm_math_geometry_common`](docs/modules/math_geometry_common.md) | The archive provides declarations for this module; consult the C interface and header comments for its supported behavior. |
| [`clm_math_image`](docs/modules/math_image.md) | Allocation-free color, reconstruction and image-coordinate mathematics. |
| [`clm_math_interpolation`](docs/modules/math_interpolation.md) | Interpolation factors and easing curves. |
| [`clm_math_matrix`](docs/modules/math_matrix.md) | Matrix arithmetic, elimination, and affine projection. |
| [`clm_math_number`](docs/modules/math_number.md) | Integer arithmetic, modular operations, and number theory. |
| [`clm_math_pathfinding2d`](docs/modules/math_pathfinding2d.md) | Two-dimensional graph adaptation for A-star search. |
| [`clm_math_pathfinding3d`](docs/modules/math_pathfinding3d.md) | Three-dimensional graph adaptation for A-star search. |
| [`clm_math_pathfinding_internal`](docs/modules/math_pathfinding_internal.md) | Shared A-star search and graph traversal adapters. |
| [`clm_math_physics2d`](docs/modules/math_physics2d.md) | Two-dimensional kinematic and rigid-body value helpers. |
| [`clm_math_physics3d`](docs/modules/math_physics3d.md) | Three-dimensional motion, inertia, and angular helpers. |
| [`clm_math_quaternion`](docs/modules/math_quaternion.md) | Quaternion construction, rotation, and interpolation. |
| [`clm_math_random`](docs/modules/math_random.md) | Deterministic random-number state and sampling. |
| [`clm_math_spatial2d`](docs/modules/math_spatial2d.md) | Two-dimensional grid, hex, distance, and terrain helpers. |
| [`clm_math_spatial3d`](docs/modules/math_spatial3d.md) | Three-dimensional grid, projection, distance, and terrain helpers. |
| [`clm_math_spatial_common`](docs/modules/math_spatial_common.md) | Shared spatial indexing and noise infrastructure. |
| [`clm_math_vector`](docs/modules/math_vector.md) | Vector arithmetic, products, lengths, and normalization. |
| [`clm_spatial_manager`](docs/modules/spatial_manager.md) | Spatial shape registries, queries, and selection state. |
| [`clm_sphere`](docs/modules/sphere.md) | Sphere shapes registered with a spatial manager. |

### Networking and security (21)

| Module | Responsibility |
|---|---|
| [`clm_bucket`](docs/modules/bucket.md) | Driver-based object storage and byte-stream conveniences. |
| [`clm_bucket_s3`](docs/modules/bucket_s3.md) | S3 request signing and object-storage transport. |
| [`clm_crypto`](docs/modules/crypto.md) | Authenticated encryption, OS randomness, key derivation and key exchange. |
| [`clm_http_utils`](docs/modules/http_utils.md) | HTTP message parsing, framing, and serialization. |
| [`clm_jwt`](docs/modules/jwt.md) | JSON Web Token encoding, decoding, and signature verification. |
| [`clm_multipart`](docs/modules/multipart.md) | MIME multipart boundary parsing and generation. |
| [`clm_net`](docs/modules/net.md) | Portable socket registration, addressing, and network I/O. |
| [`clm_net_quic`](docs/modules/net_quic.md) | QUIC connection state and caller-driven UDP transport. |
| [`clm_net_web`](docs/modules/net_web.md) | Shared web-server routing, request state, and response delivery. |
| [`clm_net_web_client`](docs/modules/net_web_client.md) | Asynchronous HTTP request scheduling and callback delivery. |
| [`clm_net_web_client_h1`](docs/modules/net_web_client_h1.md) | HTTP/1 client facade over the shared asynchronous client. |
| [`clm_net_web_client_h2`](docs/modules/net_web_client_h2.md) | HTTP/2 client facade over the shared asynchronous client. |
| [`clm_net_web_client_h3`](docs/modules/net_web_client_h3.md) | HTTP/3 client facade over the shared asynchronous client. |
| [`clm_net_web_h1`](docs/modules/net_web_h1.md) | HTTP/1 server facade over shared web-server state. |
| [`clm_net_web_h2`](docs/modules/net_web_h2.md) | HTTP/2 stream processing and server integration. |
| [`clm_net_web_h3`](docs/modules/net_web_h3.md) | HTTP/3 stream processing and QUIC server integration. |
| [`clm_net_websocket`](docs/modules/net_websocket.md) | WebSocket connection lifecycle and network transport. |
| [`clm_tls`](docs/modules/tls.md) | TLS session state and encrypted socket I/O. |
| [`clm_tls_quic`](docs/modules/tls_quic.md) | The archive provides declarations for this module; consult the C interface and header comments for its supported behavior. |
| [`clm_ua`](docs/modules/ua.md) | User-agent text classification and version extraction. |
| [`clm_websocket`](docs/modules/websocket.md) | Incremental WebSocket frame parsing and serialization. |

### Database (5)

| Module | Responsibility |
|---|---|
| [`clm_db`](docs/modules/db.md) | Database driver dispatch, prepared statements, and transactions. |
| [`clm_db_pg`](docs/modules/db_pg.md) | PostgreSQL driver entry point for the generic database layer. |
| [`clm_db_pg_native`](docs/modules/db_pg_native.md) | Native PostgreSQL framing, authentication, and query state. |
| [`clm_db_pg_native_scram`](docs/modules/db_pg_native_scram.md) | PostgreSQL SCRAM-SHA-256 authentication messages. |
| [`clm_db_sqlite3`](docs/modules/db_sqlite3.md) | SQLite connection and prepared-statement driver. |

### Graphics and games (15)

| Module | Responsibility |
|---|---|
| [`clm_audio`](docs/modules/audio.md) | Audio decoding, device playback, and playback controls. |
| [`clm_drawing`](docs/modules/drawing.md) | Drawing colors, color conversion, and drawing value helpers. |
| [`clm_font`](docs/modules/font.md) | Font loading, glyph metrics, and reference-counted font storage. |
| [`clm_gamevironment`](docs/modules/gamevironment.md) | One global, UI-thread-owned game environment. |
| [`clm_image`](docs/modules/image.md) | Image pixel storage and reference-counted image resources. |
| [`clm_image_png`](docs/modules/image_png.md) | PNG parsing, image conversion, and PNG serialization. |
| [`clm_imagelib`](docs/modules/imagelib.md) | Dependency-free, out-of-place RGBA8 image manipulation by image IDs. |
| [`clm_lighting`](docs/modules/lighting.md) | Light descriptions and collections of scene lights. |
| [`clm_panel`](docs/modules/panel.md) | Panel configuration, transforms, and lifecycle. |
| [`clm_renderer`](docs/modules/renderer.md) | Renderer selection, drawing targets, and backend dispatch. |
| [`clm_scene`](docs/modules/scene.md) | Scene configuration and scene-resource entry points. |
| [`clm_shader`](docs/modules/shader.md) | Shader descriptions and shader-resource entry points. |
| [`clm_style`](docs/modules/style.md) | Typed, renderer-independent partial styles. |
| [`clm_surface`](docs/modules/surface.md) | Portable surfaces and lifecycle/event dispatch. |
| [`clm_texture`](docs/modules/texture.md) | Texture descriptions, image sources, and reference counts. |

### Widgets (22)

| Module | Responsibility |
|---|---|
| [`clm_widget`](docs/modules/widget.md) | Virtual widgets own same-ID panels. |
| [`clm_widget_accordion`](docs/modules/widget_accordion.md) | Vertical stack of headers with exactly one owned clm_widget_list open. |
| [`clm_widget_button`](docs/modules/widget_button.md) | Push button with optional owned span and image children. |
| [`clm_widget_choice`](docs/modules/widget_choice.md) | A grouped selectable choice with an always-present owned span label. |
| [`clm_widget_container`](docs/modules/widget_container.md) | A virtual grouping widget; panels remain flat. |
| [`clm_widget_dropdown`](docs/modules/widget_dropdown.md) | A readonly input with an owned right-hand button and an owned default list. |
| [`clm_widget_image`](docs/modules/widget_image.md) | Displays an existing clm_image resource, retaining one reference while assigned. |
| [`clm_widget_input`](docs/modules/widget_input.md) | UTF-8 textbox with codepoint caret/selection (not grapheme, bidi or shaping). |
| [`clm_widget_list`](docs/modules/widget_list.md) | A single-selection, top-to-bottom list. |
| [`clm_widget_list_item`](docs/modules/widget_list_item.md) | A text row with independently optional left/right image icons. |
| [`clm_widget_manager`](docs/modules/widget_manager.md) | Per-surface widget event routing and virtual tree layout. |
| [`clm_widget_menu`](docs/modules/widget_menu.md) | The archive provides declarations for this module; consult the C interface and header comments for its supported behavior. |
| [`clm_widget_popover`](docs/modules/widget_popover.md) | The archive provides declarations for this module; consult the C interface and header comments for its supported behavior. |
| [`clm_widget_progress`](docs/modules/widget_progress.md) | Integer progress/HP/mana bar. |
| [`clm_widget_scroller`](docs/modules/widget_scroller.md) | Scrollbar with a proportional, minimum-sized thumb. |
| [`clm_widget_slider`](docs/modules/widget_slider.md) | Horizontal integer slider: minimum 0, maximum 1..INT32_MAX (default 100). |
| [`clm_widget_span`](docs/modules/widget_span.md) | Plain UTF-8 text label. |
| [`clm_widget_tab`](docs/modules/widget_tab.md) | Horizontal tab strip with borrowed page widget IDs on the same surface. |
| [`clm_widget_text`](docs/modules/widget_text.md) | Editable UTF-8 text with color tags, word wrapping and optional logical line numbers. |
| [`clm_widget_toast`](docs/modules/widget_toast.md) | The archive provides declarations for this module; consult the C interface and header comments for its supported behavior. |
| [`clm_widget_toggle`](docs/modules/widget_toggle.md) | Checkbox with an optional owned span label. |
| [`clm_widget_toolbar`](docs/modules/widget_toolbar.md) | Toolbar with 1-8 equal-width, owned container columns. |

### Utilities

| Module | Responsibility |
|---|---|
| [`clm_compress`](docs/modules/compress.md) | Compression codecs, incremental transforms, and ZIP helpers. |
| [`clm_console`](docs/modules/console.md) | Portable terminal I/O, styling, and console inspection. |
| [`clm_datetime`](docs/modules/datetime.md) | Civil date conversion, timestamps, durations, and formatting. |

## Cross-language compatibility

Module documentation describes intended capabilities and behavioral expectations. Language-specific implementations may differ in their data types, call signatures, error handling, resource representations, and available platform integrations. Each implementation should document those differences and clearly identify experimental or unavailable features.

## Implementation references

Implementation details do not define the language-neutral CLM contract. Consult the relevant implementation reference for concrete types, function declarations, build instructions, and platform requirements.

- [Available implementation documentation](docs/IMPLEMENTATIONS.md)

## Development and distribution

CLM is maintained as an internal TST technology at this stage. Distribution, source availability, and licensing announcements are controlled separately from these technical documents. Third-party components and assets retain their respective license obligations.
