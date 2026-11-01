# CLM — Cross Language Modules

**Composable modules for building libraries, frameworks, and applications across programming languages.**

> **Don't start from scratch, bro.**

CLM is a collection of focused, reusable programming modules designed **from the beginning for implementation in multiple programming languages**. It gives library, framework, game, and application authors common foundations—including memory management, collections, asynchronous work, networking, HTTP, parsing, and mathematics—so they can concentrate on what their own software is supposed to do.

CLM is **not** a new programming language, a monolithic framework, a mandatory runtime, or an attempt to make programming languages identical. Pick the modules you need and leave the rest.

The current C implementation covers **56 modules**. Each module has its own [documentation](docs/); the module pages describe capability contracts, while language-specific source and headers define concrete APIs and implementation details. The module count describes the current C collection, **not completed ports across every target language**.

## Why "Cross Language"?

Most libraries are created primarily for one programming language. Porting a framework to another language can therefore mean changing not only its syntax, but also the libraries, infrastructure, and programming patterns behind it.

**Cross Language (CL)** is intentional: CLM modules are designed as reusable *capabilities* with recognizable behavior across different programming languages.

CLM's **six official implementation targets** are **C, C++, D, Odin, Rust, and Zig**. These are the languages targeted for full, first-party module coverage; this is a support direction, not a claim that all six ports are complete. The current C implementation is the active reference foundation.

Other languages—including **C3, FreePascal, Python, Ruby, Go, C#, and Java**—can have community-supported implementations. To help contributors get started, CLM plans to seed selected languages with a dependency-complete foundational module set rather than requiring a port from nothing. A seed port is **not** the same as a complete official implementation.

A developer familiar with a CLM module in one language should recognize the same concepts when using another implementation. The syntax and internals may differ; the contracts, responsibilities, and documented behavior should remain familiar.

### Portable concepts, native implementations

CLM avoids **requiring** advanced or language-specific features in its *shared design*, including:

- Runtime reflection or automatic discovery of application classes and constructors.
- Dynamic-language behavior that cannot be expressed naturally in systems languages.
- Generics or templates as a prerequisite for using the public API.
- Inheritance, decorators, annotations, or an object-oriented runtime.
- A shared memory model or mandatory CLM-wide allocator.

These features are **not forbidden internally**. An implementation can use the best native tools available—C structures and function pointers, C++ STL containers, Rust ownership and standard collections, Zig allocators, or the language's native concurrency facilities—provided it preserves the module's documented contract.

For example, `clm_container` provides explicit, ID-based service registration and resolution without requiring runtime constructor reflection. Language-specific implementations may use different internal collections and call mechanisms without changing that shared model.

**Cross-language does not mean identical source code, identical APIs in every syntactic detail, or automatic binary interoperability.** It means designing modules that can be implemented and understood consistently across languages. The C implementation can expose C-linkage headers to C++ consumers, but a **native C++ port** may also use the STL and other C++ facilities internally.

## Why "Modules"?

CLM combines what were previously considered *abstractions* and *foundational utilities* into **one collection of composable modules**. *Composable* is a design principle, not a letter in the acronym: **CLM means Cross Language Modules**.

Some modules encapsulate existing native capabilities; others implement functionality directly. Both belong in CLM if they solve a recurring problem and reduce work in the libraries and frameworks that use them.

Each module should:

1. **Have one clear purpose.** Keep its responsibilities narrow.
2. **Be usable independently** wherever practical, without importing the entire CLM collection.
3. **Compose with other modules** when there is a genuine benefit, rather than duplicating their work.
4. **Prefer native facilities** to reimplementing mature functionality.
5. **Favor explicit contracts and procedural entry points**, with optional language-specific convenience facades.
6. **Stay understandable and lightweight**, especially when included directly in another project's source.

A module need not be tiny merely for its own sake. What matters is that it removes meaningful complexity from its consumers.

## Modules

CLM is developed incrementally, based on actual use in frameworks and libraries. Modules may have different implementation maturity across languages.

### Collections, IDs and application structure

- [clm_container](docs/clm_container.md) — ID-based service registration, resolution, invocation and scoping.
- [clm_dictionary](docs/clm_dictionary.md) — ID-keyed typed dictionaries built on BPA.
- [clm_dispatch](docs/clm_dispatch.md) — Command, event, query and exception dispatch with dependency resolution.
- [clm_ecs](docs/clm_ecs.md) — Entity-component system with blueprints, components, entity instances and systems.
- [clm_id](docs/clm_id.md) — Registry mapping string names and CLM numeric IDs.
- [clm_storage](docs/clm_storage.md) — Fixed-size typed multidimensional grids and atlases backed by BPA.
- [clm_table](docs/clm_table.md) — BPA-backed tabular collection with rows and columns.
- [clm_tree](docs/clm_tree.md) — Nested object/array/value structure suitable for structured documents.

### Concurrency

- [clm_async](docs/clm_async.md) — Cooperative stackless task scheduling, wake/cancel/state operations.
- [clm_async_pool](docs/clm_async_pool.md) — Multiworker cooperative task pools with scheduling and overflow controls.
- [clm_channel](docs/clm_channel.md) — Concurrent producer/consumer message channel with blocking and timed APIs.

### Core foundations

- [clm_base](docs/clm_base.md) — Core CLM types, identifiers, constants, and shared conventions.
- [clm_arena](docs/clm_arena.md) — ID-based growable bump arenas with stable offsets and explicit borrow/return leases.
- [clm_bpa](docs/clm_bpa.md) — Block Pool Allocation: ID-addressed typed blocks and arrays, with arena-backed small-object slabs.
- [clm_bytes](docs/clm_bytes.md) — Owned byte buffers, borrowed byte views, inspection and file-related byte helpers.
- [clm_crypto](docs/clm_crypto.md) — Authenticated encryption, secure random material, and key-related cryptographic operations.
- [clm_gen](docs/clm_gen.md) — Random value generation and hexadecimal helpers.
- [clm_hash](docs/clm_hash.md) — Digest and hexadecimal hash helpers across supported algorithms.
- [clm_managed](docs/clm_managed.md) — Configurable coordinator for cleanup hooks and collection scheduling.
- [clm_platform](docs/clm_platform.md) — OS, compiler, architecture, and platform information.

### Filesystem, streaming and data transport

- [clm_compress](docs/clm_compress.md) — Compression/decompression codecs, streaming and ZIP archive utilities.
- [clm_console](docs/clm_console.md) — Console output, input and terminal control helpers.
- [clm_directory](docs/clm_directory.md) — Directory creation, traversal, modification and inspection.
- [clm_file](docs/clm_file.md) — Filesystem file read/write operations, metadata and utility operations.
- [clm_multipart](docs/clm_multipart.md) — MIME multipart body parsing/generation and boundary helpers.
- [clm_path](docs/clm_path.md) — Filesystem path parsing, joining, normalization and inspection.
- [clm_stream](docs/clm_stream.md) — Source/sink adapters and incremental data pumps/copying.

### Mathematics

- [clm_math](docs/clm_math.md) — Scalar mathematics convenience functions.
- [clm_math_collision](docs/clm_math_collision.md) — 2D shape overlap and ray intersection checks.
- [clm_math_core](docs/clm_math_core.md) — Shared math vector/matrix/scalar types and constants.
- [clm_math_geometry](docs/clm_math_geometry.md) — Geometric measurements, polygon operations and point relationships.
- [clm_math_interpolation](docs/clm_math_interpolation.md) — Interpolation and easing operations.
- [clm_math_matrix](docs/clm_math_matrix.md) — Matrix construction, multiplication, inversion and transforms.
- [clm_math_number](docs/clm_math_number.md) — Integer arithmetic, GCD/LCM and modular number theory.
- [clm_math_pathfinding](docs/clm_math_pathfinding.md) — A* graph pathfinding.
- [clm_math_physics2d](docs/clm_math_physics2d.md) — Simple 2D physics stepping and jump helpers.
- [clm_math_quaternion](docs/clm_math_quaternion.md) — Quaternion construction, rotation, interpolation and composition.
- [clm_math_random](docs/clm_math_random.md) — Deterministic seeded numerical random helpers.
- [clm_math_vector](docs/clm_math_vector.md) — Vector arithmetic, geometric products and interpolation.

### Networking and serving

- [clm_net](docs/clm_net.md) — Cross-platform socket/network primitives, readiness waiting, and polling.
- [clm_net_web](docs/clm_net_web.md) — HTTP/1.1 server with reactor-assisted connection dispatch, routing, middleware, streaming helpers, TLS, gzip, and templates.
- [clm_tls](docs/clm_tls.md) — TLS client/server sessions layered over sockets.

### Text, data formats and web utilities

- [clm_clp](docs/clm_clp.md) — Command-line argument/token parsing into ID-keyed dictionaries.
- [clm_string](docs/clm_string.md) — String handling and encoding-aware text operations.
- [clm_string_dsv](docs/clm_string_dsv.md) — Delimited-separated values parsing and writing.
- [clm_string_env](docs/clm_string_env.md) — .env-format parsing and writing.
- [clm_string_format](docs/clm_string_format.md) — Reusable parsing/formatting primitives used by structured text formats.
- [clm_http_utils](docs/clm_http_utils.md) — HTTP/1.x utilities: message parsing, serialization, chunked-transfer coding and header helpers (tokens, Content-Length, keep-alive, Expect, reason phrases, Date) used by clm_net_web.
- [clm_string_ini](docs/clm_string_ini.md) — INI-format parsing and writing.
- [clm_string_json](docs/clm_string_json.md) — JSON parsing and writing.
- [clm_string_toml](docs/clm_string_toml.md) — TOML parsing and writing.
- [clm_string_yaml](docs/clm_string_yaml.md) — YAML parsing and writing.
- [clm_stringtify](docs/clm_stringtify.md) — Object-to-text presentation and formatting with configurable adapters.
- [clm_template](docs/clm_template.md) — Compiled text templates and rendering contexts.
- [clm_ua](docs/clm_ua.md) — User-agent identification, bot detection and device classification.
- [clm_url](docs/clm_url.md) — URL/IRI parsing, inspection, component modification and normalization.

**Status note:** The links above describe the **56-module C collection** and its language-neutral capability documentation. Features vary by implementation and snapshot; a documented module name does not imply that every language port is complete. Older `cla_` names belong to the project's history and are being migrated to `clm_`.

## CLM in practice

### HTTP services and small websites

`clm_net_web` is an **HTTP programming library**, inspired by the role of Go's `net/http`: it is intended for service endpoints, Web APIs, and lightweight HTML/static content—not as an Nginx or Apache replacement.

The current **HTTP/1.1** implementation includes:

- An HTTP server with method/path routing, named path parameters, and middleware.
- Request-scoped context and cancellation checks; TLS/HTTPS integration; optional gzip responses; redirects and template rendering.
- **Reactor-assisted connection dispatch:** idle keep-alive connections park rather than continuously occupying worker execution.
- Configurable connection limits and separate idle, read, and write timeouts.
- Incremental **fixed-length request-body reading** via a body-reader API; buffered decoding remains available for chunked and other request bodies.
- **Streaming HTTP/1.1 chunked responses** through explicit flush and subsequent writes, alongside the existing buffered response path.

This does **not** yet imply incremental chunked *request* decoding, universally nonblocking handler I/O, a complete outbound HTTP client, WebSocket support, or native HTTP/2/HTTP/3. Newer HTTP versions are planned to share the **same `clm_net_web` application-facing handler model** and be enabled through configuration when implemented.

### Example applications

CLM has been exercised through small demonstration applications, including **CLM Hello** (console/data presentation), **CLM Tiny Town** (console simulation), **CLM Console Chat** (networked messaging), and **CLM Web** (a minimal HTTP server). Examples demonstrate module composition; they are not additional CLM modules.

### A practical composition: SPIder

SPIder can build its initial RESTful **SPI (SPIder Programming Interface)** endpoints with CLM's HTTP foundation:

```text
SPIder — application/SPIs, validation, built-in service resolution
  ├── clm_net_web      HTTP/1.1 transport, routes and middleware
  ├── clm_string_json  JSON requests and responses
  ├── clm_id           consistent numeric identifiers
  ├── clm_container    ID-based registrations and resolution
  ├── clm_async_pool   cooperative worker scheduling
  ├── clm_path         filesystem path utilities
  └── clm_ua           optional User-Agent inspection
```

CLM owns the reusable HTTP and infrastructure capabilities; **SPIder owns the application-level SPI contracts, service orchestration, validation, and framework conventions**. SPIder's tailored built-in service registry need not depend on a separate, more extensive registration library. Other applications can use `clm_net_web` directly without adopting SPIder.

A more specialized web server or proxy could also be built *using* lower-level CLM modules, without requiring `clm_net_web` itself to become a general-purpose Nginx replacement.

## API philosophy: functions first

The baseline for most modules is **a procedural API**, because functions and explicit data can be expressed even in languages without classes.

For example, an implementation can expose operations such as:

```text
register_id("webapi.host", 101)
field_id = get_id("webapi.host")
```

An optional convenience facade could provide:

```text
IdRegistry.register("webapi.port", 102)
field_id = IdRegistry.get("webapi.port")
```

The convenience interface delegates to the same implementation; it is not a separate implementation or a requirement imposed on C.

Similarly, a handler can be registered with dependency IDs and invoked with those dependencies as arguments, without reflection or OOP machinery. Platform-specific details, error handling, memory ownership, and asynchronous execution must be documented per language where they differ.

## Distribution: source-first, with compiled-library options

**CLM is source-first**, but it does not require consumers to compile every implementation file themselves. Projects can include selected module sources **or link precompiled distributions** when appropriate. There is **no mandatory CLM-wide runtime**.

### C implementation

The C modules use separate **public `.h` declaration files** and **`.c` implementation files**. Consumers include the public headers and either compile/link the required implementation sources or link precompiled **static libraries (`.a` / `.lib`)**. This avoids requiring `CLM_*_IMPLEMENTATION` single-header switches. Public C headers may use `extern "C"` guards so C++ applications can link the C implementation; a native C++ implementation is free to use its own language facilities.

### Use only what you need

For example, an application using `clm_id`, `clm_container`, and `clm_string_json` should include those modules **and their actual transitive dependencies**, not the entire collection.

```text
my_application/
  ├── app source
  ├── selected CLM public headers
  └── selected CLM sources or linked static libraries
```

Selective bundling may be provided as a build/distribution convenience; it must resolve dependencies, retain required symbols and third-party notices, and avoid duplicate definitions. Bundling is not arbitrary source concatenation.

- **No mandatory CLM binary runtime or package manager.** Use CLM in the manner that suits the consuming project.
- **Keep native dependencies explicit.** A module may rely on native standard facilities or an appropriately licensed third-party implementation.
- **Keep modules independently usable.** Do not introduce umbrella dependencies merely to simplify distribution.
- **Preserve ownership rules.** Document allocation, borrowed views, cleanup, concurrency, and error semantics per implementation.
- **Keep source inspectable.** CLM-authored code is dedicated to the public domain under **CC0 1.0 Universal**; vendored or third-party code retains its own licensing terms.

## Development approach

A cross-language module should begin with a **small behavioral contract**:

1. Identify the recurring problem and the smallest useful public API.
2. Specify behavior, errors, ownership, lifetimes, and edge cases where relevant.
3. Implement using the destination language's native capabilities whenever possible.
4. Provide a lightweight convenience facade only when it improves that language's developer experience.
5. Write conformance tests that exercise the same observable contract across implementations.
6. Offer language-specific scaffolds so volunteers can implement a module without redesigning it.

**Language support is inclusive, not uniform.** The official language targets define the full-support goal; community languages may begin with complete-feature seed modules and expand through conformance-tested contributions. Mature language standard libraries are welcome as implementation backends rather than obstacles to portability.

## Scope: what CLM is not

CLM does **not** aim to:

- Replace native console or output facilities merely to make them look alike.
- Rebuild native dictionary, vector, map, or memory-management mechanisms when they already satisfy the module's behavioral contract.
- Standardize entire language standard libraries.
- Hide important systems-language ownership and resource-lifetime rules.
- Require every project to use every module.

CLM **does** aim to provide practical features that library and framework developers would otherwise repeatedly implement on their own.

## Development direction

Current priorities include strengthening HTTP/1.1 interoperability and streaming behavior, an outbound HTTP client/transport, and later **HTTP/2 and HTTP/3 support under `clm_net_web`**. Protocol implementation details can remain separate internally while sharing one handler API. These are **plans**, not features already shipped.

Longer-term module ideas include surfaces, rendering, declarative markup parsing, and renderer-drawn UI widgets (`clm_surface`, `clm_renderer`, `clm_string_tags`, `clm_widget`). They are not included in the 56-module list above.

## Guiding principle

> **Use the right language. Reuse the modules you need. Focus on what your software is meant to do.**
>
> **Don't start from scratch, bro.**
