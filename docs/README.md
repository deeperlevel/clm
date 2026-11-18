# Language-neutral CLM module documentation

135 language-neutral module interfaces documented.

[← Repository README](../README.md)

## Core and runtime

- [`clm_arena`](arena.md) — ID-addressed bump allocation with explicit, checked borrow leases.
- [`clm_bpa`](bpa.md) — Process-global ID-indexed storage for typed blocks and growable arrays.
- [`clm_clp`](clp.md) — Command-line tokenization and option-to-value mapping.
- [`clm_gen`](gen.md) — Cryptographically randomized identifiers, ranges, text and passwords.
- [`clm_id`](id.md) — Name-to-ID registration and ID lookup.
- [`clm_managed`](managed.md) — Optional coordinator for application-owned deferred cleanup routines.
- [`clm_platform`](platform.md) — Compile-target identity and portable environment constants.
- [`clm_resource`](resource.md) — Shared resource statuses and resource-lifecycle entry points.
- [`clm_types`](types.md) — Fixed-width CLM scalar aliases and plain data geometry types.

## Collections and data

- [`clm_async`](async.md) — Cooperative task scheduling and task lifecycle notifications.
- [`clm_async_pool`](async_pool.md) — Worker pools layered over cooperative schedulers.
- [`clm_box`](box.md) — Oriented box shapes registered with a spatial manager.
- [`clm_bytes`](bytes.md) — Binary-safe, growable buffers and non-owning byte-range views.
- [`clm_channel`](channel.md) — Concurrent ID channels with buffered and rendezvous transfers.
- [`clm_container`](container.md) — Explicit service registration, resolution, and scoped instances.
- [`clm_dictionary`](dictionary.md) — ID-keyed dictionaries backed by typed BPA arrays.
- [`clm_dispatch`](dispatch.md) — Command, event, and query dispatch with service resolution.
- [`clm_ecs`](ecs.md) — Entity blueprints, component storage, and systems.
- [`clm_storage`](storage.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_stream`](stream.md) — Byte sources, sinks, adapters, and incremental copying.
- [`clm_table`](table.md) — ID-keyed tabular rows, columns, and cell values.
- [`clm_tree`](tree.md) — Nested object, array, and scalar values backed by CLM storage.

## Files and serialization

- [`clm_directory`](directory.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_file`](file.md) — Path IDs reference UTF-8 BPA strings; open-file IDs identify independent native resources. _as accepts raw UTF-8 paths, _to exports caller-o
- [`clm_hash`](hash.md) — Deterministic message digests, HMAC and digest-to-text utilities.
- [`clm_path`](path.md) — Filesystem-path API using UTF-8 strings referenced by CLM IDs.
- [`clm_re`](re.md) — Portable reusable byte-oriented regular expressions for CLM.
- [`clm_string`](string.md) — ID-backed strings, text encodings, and string operations.
- [`clm_string_base64`](string_base64.md) — Base64 encoding and decoding.
- [`clm_string_dsv`](string_dsv.md) — Delimited-record parsing and generation.
- [`clm_string_env`](string_env.md) — Environment-style key/value parsing and generation.
- [`clm_string_format`](string_format.md) — Formatted string construction and format-specific helpers.
- [`clm_string_ini`](string_ini.md) — INI sections and key/value parsing and generation.
- [`clm_string_json`](string_json.md) — JSON parsing, escaping, and tree serialization.
- [`clm_string_markup`](string_markup.md) — Markup parsing and markup-oriented tree conversion.
- [`clm_string_toml`](string_toml.md) — TOML document parsing and generation.
- [`clm_string_yaml`](string_yaml.md) — YAML document parsing and generation.
- [`clm_stringtify`](stringtify.md) — Human-readable formatting of CLM values and collections.
- [`clm_template`](template.md) — Template parsing, expression evaluation, and rendering.
- [`clm_url`](url.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_uuid`](uuid.md) — UUID generation, parsing, formatting, and field inspection.

## Math and geometry

- [`clm_camera`](camera.md) — Camera view and projection configuration.
- [`clm_geometry2d`](geometry2d.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_geometry3d`](geometry3d.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_math`](math.md) — Scalar mathematical convenience functions.
- [`clm_math_calculus`](math_calculus.md) — Finite-difference derivatives, quadrature, and ODE steps.
- [`clm_math_collision2d`](math_collision2d.md) — Two-dimensional overlap, ray, segment, and sweep tests.
- [`clm_math_collision3d`](math_collision3d.md) — Three-dimensional overlap, ray, segment, and sweep tests.
- [`clm_math_core`](math_core.md) — Shared mathematical value types and storage adapters.
- [`clm_math_geometry2d`](math_geometry2d.md) — Two-dimensional geometric measurements, bounds, and transforms.
- [`clm_math_geometry3d`](math_geometry3d.md) — Three-dimensional geometric measurements, bounds, and transforms.
- [`clm_math_geometry_common`](math_geometry_common.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_math_image`](math_image.md) — Allocation-free color, reconstruction and image-coordinate mathematics.
- [`clm_math_interpolation`](math_interpolation.md) — Interpolation factors and easing curves.
- [`clm_math_matrix`](math_matrix.md) — Matrix arithmetic, elimination, and affine projection.
- [`clm_math_number`](math_number.md) — Integer arithmetic, modular operations, and number theory.
- [`clm_math_pathfinding2d`](math_pathfinding2d.md) — Two-dimensional graph adaptation for A-star search.
- [`clm_math_pathfinding3d`](math_pathfinding3d.md) — Three-dimensional graph adaptation for A-star search.
- [`clm_math_pathfinding_internal`](math_pathfinding_internal.md) — Shared A-star search and graph traversal adapters.
- [`clm_math_physics2d`](math_physics2d.md) — Two-dimensional kinematic and rigid-body value helpers.
- [`clm_math_physics3d`](math_physics3d.md) — Three-dimensional motion, inertia, and angular helpers.
- [`clm_math_quaternion`](math_quaternion.md) — Quaternion construction, rotation, and interpolation.
- [`clm_math_random`](math_random.md) — Deterministic random-number state and sampling.
- [`clm_math_spatial2d`](math_spatial2d.md) — Two-dimensional grid, hex, distance, and terrain helpers.
- [`clm_math_spatial3d`](math_spatial3d.md) — Three-dimensional grid, projection, distance, and terrain helpers.
- [`clm_math_spatial_common`](math_spatial_common.md) — Shared spatial indexing and noise infrastructure.
- [`clm_math_vector`](math_vector.md) — Vector arithmetic, products, lengths, and normalization.
- [`clm_spatial_manager`](spatial_manager.md) — Spatial shape registries, queries, and selection state.
- [`clm_sphere`](sphere.md) — Sphere shapes registered with a spatial manager.

## Networking and security

- [`clm_bucket`](bucket.md) — Driver-based object storage and byte-stream conveniences.
- [`clm_bucket_s3`](bucket_s3.md) — S3 request signing and object-storage transport.
- [`clm_crypto`](crypto.md) — Authenticated encryption, OS randomness, key derivation and key exchange.
- [`clm_http_utils`](http_utils.md) — HTTP message parsing, framing, and serialization.
- [`clm_jwt`](jwt.md) — JSON Web Token encoding, decoding, and signature verification.
- [`clm_multipart`](multipart.md) — MIME multipart boundary parsing and generation.
- [`clm_net`](net.md) — Portable socket registration, addressing, and network I/O.
- [`clm_net_quic`](net_quic.md) — QUIC connection state and caller-driven UDP transport.
- [`clm_net_web`](net_web.md) — Shared web-server routing, request state, and response delivery.
- [`clm_net_web_client`](net_web_client.md) — Asynchronous HTTP request scheduling and callback delivery.
- [`clm_net_web_client_h1`](net_web_client_h1.md) — HTTP/1 client facade over the shared asynchronous client.
- [`clm_net_web_client_h2`](net_web_client_h2.md) — HTTP/2 client facade over the shared asynchronous client.
- [`clm_net_web_client_h3`](net_web_client_h3.md) — HTTP/3 client facade over the shared asynchronous client.
- [`clm_net_web_h1`](net_web_h1.md) — HTTP/1 server facade over shared web-server state.
- [`clm_net_web_h2`](net_web_h2.md) — HTTP/2 stream processing and server integration.
- [`clm_net_web_h3`](net_web_h3.md) — HTTP/3 stream processing and QUIC server integration.
- [`clm_net_websocket`](net_websocket.md) — WebSocket connection lifecycle and network transport.
- [`clm_tls`](tls.md) — TLS session state and encrypted socket I/O.
- [`clm_tls_quic`](tls_quic.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_ua`](ua.md) — User-agent text classification and version extraction.
- [`clm_websocket`](websocket.md) — Incremental WebSocket frame parsing and serialization.

## Database

- [`clm_db`](db.md) — Database driver dispatch, prepared statements, and transactions.
- [`clm_db_pg`](db_pg.md) — PostgreSQL driver entry point for the generic database layer.
- [`clm_db_pg_native`](db_pg_native.md) — Native PostgreSQL framing, authentication, and query state.
- [`clm_db_pg_native_scram`](db_pg_native_scram.md) — PostgreSQL SCRAM-SHA-256 authentication messages.
- [`clm_db_sqlite3`](db_sqlite3.md) — SQLite connection and prepared-statement driver.

## Graphics and games

- [`clm_audio`](audio.md) — Audio decoding, device playback, and playback controls.
- [`clm_drawing`](drawing.md) — Drawing colors, color conversion, and drawing value helpers.
- [`clm_font`](font.md) — Font loading, glyph metrics, and reference-counted font storage.
- [`clm_gamevironment`](gamevironment.md) — One global, UI-thread-owned game environment.
- [`clm_image`](image.md) — Image pixel storage and reference-counted image resources.
- [`clm_image_png`](image_png.md) — PNG parsing, image conversion, and PNG serialization.
- [`clm_imagelib`](imagelib.md) — Dependency-free, out-of-place RGBA8 image manipulation by image IDs.
- [`clm_lighting`](lighting.md) — Light descriptions and collections of scene lights.
- [`clm_panel`](panel.md) — Panel configuration, transforms, and lifecycle.
- [`clm_renderer`](renderer.md) — Renderer selection, drawing targets, and backend dispatch.
- [`clm_scene`](scene.md) — Scene configuration and scene-resource entry points.
- [`clm_shader`](shader.md) — Shader descriptions and shader-resource entry points.
- [`clm_style`](style.md) — Typed, renderer-independent partial styles.
- [`clm_surface`](surface.md) — Portable surfaces and lifecycle/event dispatch.
- [`clm_texture`](texture.md) — Texture descriptions, image sources, and reference counts.

## Widgets

- [`clm_widget`](widget.md) — Virtual widgets own same-ID panels.
- [`clm_widget_accordion`](widget_accordion.md) — Vertical stack of headers with exactly one owned clm_widget_list open.
- [`clm_widget_button`](widget_button.md) — Push button with optional owned span and image children.
- [`clm_widget_choice`](widget_choice.md) — A grouped selectable choice with an always-present owned span label.
- [`clm_widget_container`](widget_container.md) — A virtual grouping widget; panels remain flat.
- [`clm_widget_dropdown`](widget_dropdown.md) — A readonly input with an owned right-hand button and an owned default list.
- [`clm_widget_image`](widget_image.md) — Displays an existing clm_image resource, retaining one reference while assigned.
- [`clm_widget_input`](widget_input.md) — UTF-8 textbox with codepoint caret/selection (not grapheme, bidi or shaping).
- [`clm_widget_list`](widget_list.md) — A single-selection, top-to-bottom list.
- [`clm_widget_list_item`](widget_list_item.md) — A text row with independently optional left/right image icons.
- [`clm_widget_manager`](widget_manager.md) — Per-surface widget event routing and virtual tree layout.
- [`clm_widget_menu`](widget_menu.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_widget_popover`](widget_popover.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_widget_progress`](widget_progress.md) — Integer progress/HP/mana bar.
- [`clm_widget_scroller`](widget_scroller.md) — Scrollbar with a proportional, minimum-sized thumb.
- [`clm_widget_slider`](widget_slider.md) — Horizontal integer slider: minimum 0, maximum 1..INT32_MAX (default 100).
- [`clm_widget_span`](widget_span.md) — Plain UTF-8 text label.
- [`clm_widget_tab`](widget_tab.md) — Horizontal tab strip with borrowed page widget IDs on the same surface.
- [`clm_widget_text`](widget_text.md) — Editable UTF-8 text with color tags, word wrapping and optional logical line numbers.
- [`clm_widget_toast`](widget_toast.md) — Module contract and supported behavior are described in the linked overview.
- [`clm_widget_toggle`](widget_toggle.md) — Checkbox with an optional owned span label.
- [`clm_widget_toolbar`](widget_toolbar.md) — Toolbar with 1-8 equal-width, owned container columns.

## Utilities

- [`clm_compress`](compress.md) — Compression codecs, incremental transforms, and ZIP helpers.
- [`clm_console`](console.md) — Portable terminal I/O, styling, and console inspection.
- [`clm_datetime`](datetime.md) — Civil date conversion, timestamps, durations, and formatting.






<!-- CLM-DOC-INDEX-SCOPE:BEGIN -->
## How to use this directory

Each &lt;module&gt;.md file is the canonical language-neutral contract for a CLM
capability. clm_&lt;module&gt;.md files are retained as legacy-prefixed references
and link to the canonical page. Start with a module's purpose, then follow its
direct dependency links before selecting an implementation. Binding-specific
signatures, build commands, and platform notes belong to implementation
repositories rather than these contracts.
<!-- CLM-DOC-INDEX-SCOPE:END -->
