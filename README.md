# CLM — Cross Language Modules

**Composable modules for building libraries, frameworks, and applications across programming languages.**

> **Don't start from scratch, bro.**

CLM is a collection of focused, reusable programming modules designed **from the beginning for implementation in multiple programming languages**. It gives framework and library authors common capabilities—such as dependency injection, message dispatching, ID-based storage, and parsing—so they can concentrate on what their own software is supposed to do.

CLM is **not** a new standard library, a framework, a runtime, or an attempt to make programming languages identical. Pick the modules you need and leave the rest.

## Why "Cross Language"?

Most libraries are created primarily for one programming language. Porting a framework to another language can therefore mean changing not only its syntax, but also the libraries, infrastructure, and programming patterns behind it.

**Cross Language (CL)** is intentional: CLM modules are designed as reusable *capabilities* with understandable behavior across languages such as **C, C++, Go, D, FreePascal, Odin, and C#**, with room for community-supported implementations in other languages.

A developer familiar with a CLM module in one language should recognize the same concepts when using another implementation. The syntax and internals may differ; the concepts, responsibilities, and documented behavior should remain familiar.

### Portable concepts, native implementations

CLM avoids **requiring** advanced or language-specific features in its *shared design*, including:

- Runtime reflection or automatic discovery of application classes and constructors.
- Dynamic-language behavior that cannot be expressed naturally in systems languages.
- Generics or templates as a prerequisite for using the public API.
- Inheritance, decorators, annotations, or an object-oriented runtime.
- A shared memory model or mandatory CLM-wide allocator.

These features are **not forbidden internally**. An implementation can use the best native tools available—native maps and dictionaries, C++ STL containers, Go maps and slices, C structures and function pointers, or C# delegates—provided it preserves the module's documented contract.

For example, `clm_depend` uses explicit integer service IDs and registered factories rather than relying on constructor reflection. That model works with language-native callables and with ordinary function pointers in C.

**Cross-language does not mean identical source code, identical APIs in every syntactic detail, or automatic binary interoperability.** It means designing modules that can be implemented and understood consistently across languages.

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
- [clm_arena](docs/clm_arena.md) — ID-based growable memory arenas with scoped borrowing.
- [clm_bpa](docs/clm_bpa.md) — Block Pool Allocation: global ID-addressed typed blocks and arrays, with pool-backed storage.
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

- [clm_net](docs/clm_net.md) — Cross-platform socket/network primitives and readiness waits.
- [clm_net_web](docs/clm_net_web.md) — HTTP/1.1 server: routes, middleware, body handling, TLS, gzip, templates.
- [clm_tls](docs/clm_tls.md) — TLS client/server sessions layered over sockets.

### Text, data formats and web utilities

- [clm_clp](docs/clm_clp.md) — Command-line argument/token parsing into ID-keyed dictionaries.
- [clm_string](docs/clm_string.md) — String handling and encoding-aware text operations.
- [clm_string_dsv](docs/clm_string_dsv.md) — Delimited-separated values parsing and writing.
- [clm_string_env](docs/clm_string_env.md) — .env-format parsing and writing.
- [clm_string_format](docs/clm_string_format.md) — Reusable parsing/formatting primitives used by structured text formats.
- [clm_string_httpp](docs/clm_string_httpp.md) — HTTP/1.x message parsing, serialization and chunked-transfer helpers.
- [clm_string_ini](docs/clm_string_ini.md) — INI-format parsing and writing.
- [clm_string_json](docs/clm_string_json.md) — JSON parsing and writing.
- [clm_string_toml](docs/clm_string_toml.md) — TOML parsing and writing.
- [clm_string_yaml](docs/clm_string_yaml.md) — YAML parsing and writing.
- [clm_stringtify](docs/clm_stringtify.md) — Object-to-text presentation and formatting with configurable adapters.
- [clm_template](docs/clm_template.md) — Compiled text templates and rendering contexts.
- [clm_ua](docs/clm_ua.md) — User-agent identification, bot detection and device classification.
- [clm_url](docs/clm_url.md) — URL/IRI parsing, inspection, component modification and normalization.

**Status note:** This is a project scope and direction, **not a claim that every module or language port is finished**. JSON, YAML, CSV, mapping, and other modules can be developed as consumers require them. Earlier modules or filenames prefixed `cla_` are part of the project's history and should be renamed to `clm_` as they are migrated.

### A practical composition

A framework such as **SPIder** can use:

```text
SPIder
  ├── clm_id          readable names → numeric IDs
  ├── clm_container   registrations and indexed data
  ├── clm_depend      service construction and lifetimes
  ├── clm_dispatch    handler registration and dispatch
  ├── clm_path        filesystem paths
  └── clm_ua          User-Agent inspection
```

SPIder remains responsible for its own HTTP, routing, middleware, and application behavior. CLM owns the reusable modules SPIder would otherwise have to maintain itself. Another framework may choose a completely different subset.

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

## Distribution: include the source

**CLM is source-first.** Modules are generally intended to be **included directly in the consuming library, framework, or application**, rather than requiring a separately installed CLM runtime or a precompiled CLM library.

The preferred distribution is a **small, self-contained source file** when that is practical. Where a language or platform requires multiple files—for example, platform-specific implementations or C header/source pairs—those files can be distributed together.

### Bundle only what you use

Suppose a project needs the following modules:

```text
clm_id
clm_container
clm_dispatch
clm_depend
clm_console
```

Rather than shipping five independent CLM modules, the project can **bundle the selected sources into one application-local CLM unit**:

```text
my_framework/
  ├── application source
  └── CLM bundle    # selected modules bundled together
```

The application then imports or includes the bundled interfaces using its language's normal mechanisms. Bundling is a **build/distribution step**, not simply concatenating files in arbitrary order. A bundler should resolve inter-module dependencies and ordering, retain required symbols and licenses, and avoid duplicate definitions or initialization side effects. Module imports may need rewriting when sources are merged into one unit.

### Source-first principles

- **No mandatory CLM binary runtime.** Include the implementations your project actually needs.
- **No forced package-manager dependency** merely to access a small CLM module.
- **Bundle selectively.** Exclude unused modules instead of distributing the full collection.
- **Keep native dependencies minimal.** A module may use an established standard library or an explicitly documented optional third-party dependency, such as a YAML parsing library.
- **Preserve visibility and auditability.** Application developers can inspect, debug, and adapt the included source in accordance with its license.

The CLM source-distribution policy does not imply that *all underlying native or third-party dependencies* are source-only or never compiled. It describes how CLM's own modules are preferably delivered.

## Development approach

A cross-language module should begin with a **small behavioral contract**:

1. Identify the recurring problem and the smallest useful public API.
2. Specify behavior, errors, ownership, lifetimes, and edge cases where relevant.
3. Implement using the destination language's native capabilities whenever possible.
4. Provide a lightweight convenience facade only when it improves that language's developer experience.
5. Write conformance tests that exercise the same observable contract across implementations.
6. Offer language-specific scaffolds so volunteers can implement a module without redesigning it.

**Language support is inclusive, not uniform.** Core contributors can prioritize particular languages while community members contribute implementations for others, including languages with rich standard libraries such as Java and C#.

## Scope: what CLM is not

CLM does **not** aim to:

- Replace native console and output facilities merely to make them look alike.
- Rebuild existing dictionary, vector, map, or memory-management systems when a language already provides suitable facilities.
- Standardize entire language standard libraries.
- Hide important systems-language ownership and resource-lifetime rules.
- Require every project to use every module.

CLM **does** aim to provide practical features that library and framework developers would otherwise repeatedly implement on their own.

## Guiding principle

> **Use the right language. Reuse the modules you need. Focus on what your software is meant to do.**
>
> **Don't start from scratch, bro.**
