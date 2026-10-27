# CLM — Cross Language Modules

**Composable modules for building libraries, frameworks, and applications across programming languages.**

> **Don't start from scratch, bro.**

CLM is a collection of focused, reusable programming modules designed **from the beginning for implementation in multiple programming languages**. It gives framework and library authors common capabilities—such as dependency injection, message dispatching, ID-based storage, and parsing—so they can concentrate on what their own software is supposed to do.

CLM is **not** a new standard library, a framework, a runtime, or an attempt to make programming languages identical. Pick the modules you need and leave the rest.

## Why "Cross Language"?

Most libraries are created primarily for one programming language. Porting a framework to another language can therefore mean changing not only its syntax, but also the libraries, infrastructure, and programming patterns behind it.

**Cross Language (CL)** is intentional: CLM modules are designed as reusable *capabilities* with understandable behavior across languages such as **C, C++, Go, Python, D, FreePascal, Odin, and C#**, with room for community-supported implementations in other languages.

A developer familiar with CLM dependency registration in Python should recognize the same concepts when using its C or Go implementation. The syntax and internals may differ; the concepts, responsibilities, and documented behavior should remain familiar.

### Portable concepts, native implementations

CLM avoids **requiring** advanced or language-specific features in its *shared design*, including:

- Runtime reflection or automatic discovery of application classes and constructors.
- Dynamic-language behavior that cannot be expressed naturally in systems languages.
- Generics or templates as a prerequisite for using the public API.
- Inheritance, decorators, annotations, or an object-oriented runtime.
- A shared memory model or mandatory CLM-wide allocator.

These features are **not forbidden internally**. An implementation can use the best native tools available—Python dictionaries, C++ STL containers, Go maps and slices, C structures and function pointers, or C# delegates—provided it preserves the module's documented contract.

For example, `clm_depend` uses explicit integer service IDs and registered factories rather than relying on constructor reflection. That model works with Python callables and with ordinary function pointers in C.

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

| Module | Purpose |
| --- | --- |
| [`clm_base`](./docs/clm_base.md) | Base types and structures. |
| [`clm_bpa`](./docs/clm_bpa.md) | ID-addressed block allocation and reusable memory pooling, plus structure-agnostic managed arrays, with explicit resource lifetimes and validated data transfers. matching. |
| [`clm_console`](./docs/clm_console.md) | Standard-stream text input/output and ANSI-compatible terminal controls. |
| [`clm_dictionary`](./docs/clm_dictionary.md) | ID-based integer-key dictionaries with BPA/BPAA-managed typed values and key iteration. |
| [`clm_string`](./docs/clm_string.md) | BPA-backed Unicode strings and BPAA-backed string arrays, stored as UTF-8 with selectable UTF-8, Latin-1, or UTF-16LE byte export. |
| [`clm_container`](./docs/clm_container.md) | A procedural, **typed key/value collection registry**. A numeric **container ID** identifies each collection. Within it, caller-defined numeric **entry keys** identify values. Container keys are **not** positional array indices; gaps in the numeric key space are permitted. |
| [`clm_storage`](./docs/clm_storage.md) | ID-addressed, typed sequential and multidimensional storage with explicit allocation, navigation, transformations, and manually managed lifetimes. |
| [`clm_id`](./docs/clm_id.md) | Map names to numeric IDs, optionally use ASCII case-insensitive matching, and generate sequential IDs. |
| [`clm_depend`](./docs/clm_depend.md) | Procedural, ID-based dependency injection and factories with tracked instance identities, explicit references, and deferred transient disposal. |
| [`clm_ecs`](./docs/clm_ecs.md) | ID-based entity and component composition with system queries built on CLM Depend's tracked transient factories. |
| [`clm_dispatch`](./docs/clm_dispatch.md) | A procedural, ID-based, asynchronous message dispatcher |
| [`clm_path`](./docs/clm_path.md) | ID-addressed filesystem path mapping, composition and transformation using CLM-owned string containers. |
| [`clm_formats`](./docs/clm_formats.md) | Provides reusable lexical processing, a common document-tree representation, and parsing and serialization of several text-based data formats. |
| [`clm_format`](./docs/clm_format.md) | Reusable lexical rules, tokenization, and ID-mapped owned document trees shared by independent data-format codecs. |
| [`clm_ini`](./docs/clm_ini.md) | Parse and serialize sectioned INI data as string-valued CLM objects. |
| [`clm_csv`](./docs/clm_csv.md) | Parse and serialize header-based CSV into CLM arrays of string-valued row objects. |
| [`clm_json`](./docs/clm_json.md) | Parse and serialize JSON using CLM-owned document trees and registered numeric field IDs. |
| [`clm_yaml`](./docs/clm_yaml.md) | Parse and serialize a deliberately limited block-oriented YAML subset. |
| [`clm_toml`](./docs/clm_toml.md) | Parse and serialize tables, scalar fields, and arrays in a practical TOML subset. |
| [`clm_platform`](./docs/clm_platform.md) | Focused operating-system and platform capabilities. |
| [`clm_directory`](./docs/clm_directory.md) | Provides directory operations only through registered CLM Path IDs. It never requires callers to pass mutable output buffers or raw path strings to directory functions. |
| [`clm_file`](./docs/clm_file.md) | Provides whole-file, text/line, and block-streaming file operations through mapped **CLM Path IDs** and caller-created **CLM Storage IDs**. |
| [`clm_managed`](./docs/clm_managed.md) | Optional manual or interval-based coordinator for deferred disposal of CLM Depend, Container and Storage resources. |
| [`clm_ua`](./docs/clm_ua.md) | Lightweight User-Agent parsing. |

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

For example, Python can expose:

```python
clm_id_register("webapi.host", 101)
field_id = clm_id_get("webapi.host")
```

With an optional convenience facade:

```python
IdRegistry.register("webapi.port", 102)
field_id = IdRegistry.get("webapi.port")
```

The convenience interface delegates to the same implementation; it is not a separate implementation or a requirement imposed on C.

Similarly, a handler can be registered with dependency IDs and invoked with those dependencies as arguments, without reflection or OOP machinery. Platform-specific details, error handling, memory ownership, and asynchronous execution must be documented per language where they differ.

## Distribution: include the source

**CLM is source-first.** Modules are generally intended to be **included directly in the consuming library, framework, or application**, rather than requiring a separately installed CLM runtime or a precompiled CLM library.

The preferred distribution is a **small, self-contained source file** when that is practical. Where a language or platform requires multiple files—for example, platform-specific implementations or C header/source pairs—those files can be distributed together.

### Bundle only what you use

Suppose a Python project needs the following modules:

```text
clm_id.py
clm_container.py
clm_dispatch.py
clm_depend.py
clm_console.py
```

Rather than shipping five independent CLM modules, the project can **bundle the selected sources into one `clm.py`**:

```text
my_framework/
  ├── spider.py
  └── clm.py    # selected CLM modules bundled together
```

Usage then looks like:

```python
from clm import IdRegistry, Container, Depend, Dispatch
```

Bundling is a **build/distribution step**, not simply concatenating files in arbitrary order. A bundler should resolve inter-module imports and ordering, retain required symbols and licenses, and avoid duplicate definitions or initialization side effects. Python modules that currently import one another may need import rewriting when merged into a single file.

### Source-first principles

- **No mandatory CLM binary runtime.** Include the implementations your project actually needs.
- **No forced package-manager dependency** merely to access a small CLM module.
- **Bundle selectively.** Exclude unused modules instead of distributing the full collection.
- **Keep native dependencies minimal.** A module may use an established standard library or an explicitly documented optional third-party dependency; for example, a Python YAML adapter may use PyYAML.
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

- Replace `print()` in Python, `printf()` in C, `std::cout` in C++, or `fmt` in Go just to make them look alike.
- Rebuild existing dictionary, vector, map, or memory-management systems when a language already provides suitable facilities.
- Standardize entire language standard libraries.
- Hide important systems-language ownership and resource-lifetime rules.
- Require every project to use every module.

CLM **does** aim to provide practical features that library and framework developers would otherwise repeatedly implement on their own.

## Guiding principle

> **Use the right language. Reuse the modules you need. Focus on what your software is meant to do.**
>
> **Don't start from scratch, bro.**
