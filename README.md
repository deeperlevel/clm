# CLA — Cross Language Abstraction

**Don't start from scratch, bro.**

CLA (Cross Language Abstraction) is a growing family of modular libraries that provides familiar programming capabilities across multiple languages. Its goal is simple: **learn a capability once, then recognize and use it across languages**.

CLA is both the name of the project and the identity behind its libraries. Each capability is published under the `cla_*` naming convention, such as `cla_console`, `cla_path`, and `cla_json`.

CLA is not a new programming language, virtual machine, compiler, or universal runtime. It is a practical collection of reusable abstractions, utilities, and documented behavioral contracts. Implementations may use native standard libraries, operating-system APIs, third-party libraries, or custom code—whatever best serves the capability in that language.

> **At your service.** CLA's butler mascot represents the project's purpose: quietly provide the tools developers need, without taking over their applications.

## Why CLA?

Moving between programming languages often means learning different APIs for familiar tasks: manipulating arrays, creating directories, parsing JSON, using sockets, or writing to the console. The underlying ideas are similar, but names, conventions, and behaviors differ.

CLA aims to make that transition easier by providing:

- **Shared concepts.** The same module categories and broadly consistent operations across supported languages.
- **Modular adoption.** Include only the source modules your project needs; CLA is not an all-or-nothing framework.
- **Native implementation freedom.** A Python module can use Python facilities, while C or C++ modules can use their own suitable implementations.
- **Predictable behavior.** Documented semantics for inputs, outputs, edge cases, errors, and platform-dependent features.
- **Less repetition.** Reuse common solutions instead of rebuilding them in every language or application.

The objective is **conceptual and behavioral portability, not identical source code**. Language-specific syntax, memory ownership, concurrency, and performance characteristics still matter. CLA complements native language features rather than replacing them.

## Not reinventing the wheel — standardizing how wheels are used

**CLA is not about reinventing the wheel. It is about standardizing how existing wheels are used across different vehicles.**

Programming languages already provide capable tools for everyday tasks. Python has dictionaries and file APIs; C# has collections and streams; Go has maps and networking primitives; C and C++ have their own libraries and system interfaces. CLA does not assume those tools are inadequate or need to be replaced.

Instead, CLA defines a **recognizable set of capabilities and behavioral contracts** across languages, while letting each implementation use the most appropriate foundations available. The *vehicle* may change—Python, C, C++, C#, Go, or another language—but developers should recognize what each *wheel* is for and how to use it.

Our approach is straightforward:

1. **Reuse first.** Prefer stable native APIs, standard libraries, operating-system services, and suitable existing implementations.
2. **Wrap and standardize when useful.** Provide consistent names, operations, and documented behavior where language ecosystems differ.
3. **Implement only what is missing.** Write original functionality when existing facilities cannot satisfy the CLA contract or the target environment lacks the capability.
4. **Preserve native freedom.** Developers can always choose their language's own APIs, generics, templates, and libraries instead of CLA.
5. **Avoid abstraction for its own sake.** A CLA module should earn its place through practical reuse, clearer contracts, or transferable programming knowledge—not merely by renaming an existing function.

For example, `cla_dictionary_is` may use a native dictionary or map in one language and a purpose-built data structure in another. Its value is not that CLA invented dictionaries; it is that developers can rely on a familiar *integer-key/string-value* capability and explicitly documented behavior across implementations.

**The goal is to avoid rebuilding solutions *and* relearning unrelated interfaces whenever the programming language changes.** CLA standardizes the experience without requiring identical internals.

## Design principles

1. **One recognizable capability model.** Names, purposes, and behavior should remain as consistent as practical across language implementations.
2. **Independent modules.** Each `cla_*` library has a focused responsibility and can be used independently, subject to clearly documented dependencies.
3. **No lowest-common-denominator requirement.** Common capabilities have a shared contract; optional platform- or language-specific features may be exposed where available.
4. **Native where sensible.** Wrap well-tested facilities when they fit; implement missing features only when necessary.
5. **Explicit platform limits.** Unsupported features must be documented and reported predictably instead of silently approximated.
6. **Clear types and ownership.** Especially in low-level languages, memory lifecycle, encoding, resources, and error behavior must be specified.
7. **No mandatory generics.** Generic and template-based features are not required by CLA's common API. Developers remain free to use their language's native generic facilities.
8. **Source-first distribution.** CLA is distributed as application-included source code, not as precompiled shared or static libraries. Prefer single-file or header-only delivery wherever appropriate to the target language.
9. **Additive growth.** New modules and functionality can be introduced over time without forcing existing users to adopt the entire ecosystem.

## Module catalog

The following catalog describes the **intended scope** of CLA's modules. It is a roadmap, **not a claim that every module or named operation is already implemented**. Exact APIs, package availability, and supported platforms will be documented in each module's own repository or documentation.

### Language and system foundations

| Module | Purpose |
| --- | --- |
| **`cla_lang`** | Language-level helpers and extensions for facilities that are absent, inconsistent, or inconvenient in a target language—for example, `defer`-style cleanup, `panic`-style failure handling, assertions, and related helpers. Features that are already native need not be reimplemented. |
| **`cla_os`** | Operating-system information and conventions, including OS identification and version, native newline sequences, directory separators, and related system details. Illustrative operations: `get_os_info`, `get_os_version`, `get_os_newline`, `get_os_directory_separator`. |
| **`cla_arena`** | Arena-based memory allocation and lifecycle helpers where the language/runtime permits or benefits from explicit arena management. Implementations must clearly define ownership, lifetime, reset, and release semantics; support may vary by language. |
| **`cla_env`** | Reading, writing, removing, and enumerating environment-variable key/value pairs where supported, with explicit behavior for missing keys and process-level effects. |

### Console, text, and data manipulation

| Module | Purpose |
| --- | --- |
| **`cla_console`** | Rich console and terminal I/O: reading, writing, clearing, cursor positioning, selection or screen-region operations, and terminal/window properties where supported. Its normal text-facing interface is Unicode-oriented; use `cla_convert` when decoding non-Unicode byte encodings. Terminal features are capability-dependent. |
| **`cla_convert`** | Explicit conversions between supported values and representations: byte arrays and hexadecimal text, strings and numeric values, integers and floating-point values, and supported character encodings. Conversion failures, precision loss, overflow, and encoding errors must have defined behavior. |
| **`cla_string`** | String operations such as searching, slicing, trimming, splitting, joining, replacing, and comparison. Unicode indexing, normalization, and encoding semantics should be documented rather than assumed identical across languages. |
| **`cla_regex`** | Regular-expression matching, searching, substitution, and capture operations behind a consistent interface, with documented syntax and feature differences where regex engines diverge. |
| **`cla_validation`** | Common validation helpers such as `is_valid_email`, `is_valid_ipv4`, and other reusable format or value checks. Each validator should document its exact rules and limits rather than imply universal real-world validity. |

### Arrays, dictionaries, and collections

| Module | Purpose |
| --- | --- |
| **`cla_array`** | Indexed, array-oriented operations including slicing, inserting, finding, accessing first/last elements, and related utilities. Each language implementation defines how these operations work with its supported array representation. |
| **`cla_dictionary`** and **`cla_dictionary_*`** | Typed, non-generic key/value dictionaries with consistent operations. Type combinations are represented by separately named libraries or variants such as `cla_dictionary_ii` (integer → integer) and `cla_dictionary_is` (integer → string). The supported key/value type matrix is defined explicitly rather than via mandatory generics or templates. |
| **`cla_collection`** | An index-accessible collection of elements that may be object references or pointers to structures in low-level languages. In addition to indexed access, it provides predicate-based operations such as `find`, `find_all`, and `slice`. Element ownership and predicate callback contracts must be explicit. |

**Why typed dictionaries instead of generics?** CLA prioritizes recognizable, explicitly named type combinations that can be offered consistently even in languages without generics or templates. Native generic dictionaries remain a good choice for developers who want language-specific flexibility. CLA does not attempt to prohibit or replace them.

The examples `ii` and `is` are shorthand for *integer key/integer value* and *integer key/string value*. Additional variants can be introduced as the supported type matrix grows. Exact type widths, string representations, and variant names will be specified per contract.

### Filesystem and paths

| Module | Purpose |
| --- | --- |
| **`cla_path`** | Pure or mostly non-I/O path operations such as `join`, `get_filename`, `get_filename_without_extension`, `get_extension`, and `get_parent`. Path interpretation follows clearly documented platform conventions. |
| **`cla_directory`** | Directory operations such as getting the current or temporary directory, creating directories, copying directory trees, and searching for files. Illustrative operations: `get_current_directory`, `get_temporary_directory`, `copy_directory`, `make_new_directory`, `search_files`. |
| **`cla_file`** | File I/O and lifecycle operations, including opening, closing, flushing, reading, writing, saving content, and reading an entire file as text or bytes. Illustrative operations: `get_file_content`, `read_all_as_string`, `save_file`, `open_file`, `close_file`, `flush`. |

Path manipulation and filesystem I/O are intentionally separate concerns. For example, forming a path should not require accessing the disk. Text file operations should specify encoding and newline behavior.

### Networking and protocols

| Module | Purpose |
| --- | --- |
| **`cla_net`** | Low-level networking primitives, including sockets, addresses, connections, and network I/O where supported. |
| **`cla_http`** | HTTP functionality built on shared networking concepts, such as requests, responses, headers, status codes, and HTTP transport operations. |
| **`cla_udp`** | UDP-specific datagram operations and socket behavior, extending the networking foundation with connectionless communication. |

Network modules should document synchronous/asynchronous support, timeout semantics, error reporting, resource cleanup, and platform differences. HTTP and UDP are separate protocols and need not share identical abstractions beyond their common networking foundations.

### Time and structured data

| Module | Purpose |
| --- | --- |
| **`cla_datetime`** | Dates, times, durations, parsing, formatting, and time-zone-aware operations where supported, with clear treatment of local time, UTC, and precision. |
| **`cla_json`** | JSON parsing, serialization, inspection, and generation with consistent value types and error handling. |
| **`cla_yaml`** | YAML parsing and serialization, including mappings, sequences, scalars, and the supported YAML feature/version set. |
| **`cla_csv`** | Reading and writing delimiter-separated tabular text, including quoting, escaping, line endings, and configurable delimiters. |
| **`cla_toml`** | TOML parsing and serialization, including typed configuration values and tables. |
| **`cla_ini`** | INI-style configuration parsing and writing, with explicit handling of dialect differences such as comments, sections, duplicate keys, and case sensitivity. |

### Hashing and cryptography

| Module | Purpose |
| --- | --- |
| **`cla_hash_<algorithm>`** | Algorithm-specific hashing modules, for example `cla_hash_sha512` and `cla_hash_md5`. Each module describes its digest format, supported inputs, and algorithm limitations. MD5 is provided, if implemented, only for compatibility or non-security use cases; it is not suitable for collision-resistant security purposes. |
| **`cla_crypt_<algorithm>`** | Algorithm-specific cryptographic modules, such as `cla_crypt_aes`, with interfaces for the supported operations and modes. Contracts must specify key handling, nonce/IV requirements, authentication, and secure error behavior. Prefer authenticated encryption for new applications. |

Security-sensitive modules require special care: equivalent naming does not guarantee secure usage. Defaults, platform backends, algorithm versions, test vectors, and security constraints must be documented and reviewed.

### Windowing and input

| Module | Purpose |
| --- | --- |
| **`cla_window`** | Cross-platform window creation, event handling, and user input abstraction, potentially backed by libraries such as GLFW where appropriate. This may include keyboard, mouse, display, window state, and event-loop operations. Rendering and GPU APIs are separate concerns unless explicitly added to a defined contract. |

## Cross-language expectations

A CLA library should make it easy for a developer to move between implementations while retaining the same understanding of the capability. That requires more than matching function names.

Each module should aim to document:

- **Purpose and vocabulary:** what the capability means, and what its functions do.
- **Public operations and data types:** inputs, outputs, defaults, and explicit type mappings.
- **Behavioral rules:** ordering, indexing, boundaries, missing values, mutability, and side effects.
- **Errors:** expected failure categories and how they are surfaced in each language.
- **Encoding and Unicode:** particularly for strings, console output, file I/O, and serialization.
- **Ownership and lifetime:** particularly for arenas, pointers, files, sockets, windows, and callbacks.
- **Platform capabilities:** which operations require a terminal, OS facility, native library, or optional backend.
- **Conformance tests:** shared cases that different language implementations can run against their respective public APIs.

**Consistency does not mean pretending differences do not exist.** For example, a C implementation may return a status code and write an output parameter, while a Python implementation may raise an exception. Their documentation should still map these mechanisms to the same conceptual error categories and expected outcomes.

## Distribution model: source code, not compiled libraries

CLA follows a **source-included distribution model**. Its modules are meant to become part of the consuming application's source tree or build inputs. CLA does **not** distribute precompiled libraries as its official delivery format.

### Preferred forms

- **Single-file modules.** Where practical, a capability is delivered as one source file that can be copied, vendored, or otherwise included directly in an application.
- **Header-only modules.** For languages such as C and C++, a capability may be delivered through a header-only interface and implementation, with language-appropriate guards and linkage rules.
- **Language-appropriate source inclusion.** In languages that use imports or modules, the CLA implementation should remain source-distributed and easy to include in the application's build or project. Packaging metadata or a source-only package may assist retrieval, but it must not turn the official CLA artifact into a precompiled binary library.

**Not part of CLA's distribution plan:** published `.so`, `.dll`, `.dylib`, `.a`, `.lib`, or equivalent precompiled shared/static library artifacts. CLA's official releases provide **source**, not compiled link targets.

### What this means for applications

1. **Include only the modules you use.** CLA does not require applications to bring in every capability.
2. **Build with your application.** In compiled languages, CLA source is compiled as part of the consuming application's normal build when necessary. "Not a compiled library" refers to how CLA is *distributed*, not a prohibition against compilation by the application.
3. **Keep dependencies visible.** If a module uses another CLA module or an OS/platform facility, document that relationship. The source-included model should not imply hidden dependencies.
4. **Support review and modification.** Developers can inspect, debug, and adapt the included source subject to the project's license.
5. **Document integration per language.** Each implementation should show precisely which files to include or import, any one-time build definitions required, and how to avoid duplicate definitions or symbol collisions.

A source-only CLA release may still rely on platform APIs or external system libraries for certain capabilities (for example, sockets or windowing). Those dependencies should be declared; **CLA itself is not shipped as a compiled library**.

## Modularity and dependencies

The guiding rule is **use only what you need**. A console application might use `cla_console` and `cla_convert`; a configuration tool might use `cla_file`, `cla_path`, and `cla_toml`; a server might use `cla_net` and `cla_http`.

Modules may depend on other CLA modules when doing so meaningfully improves consistency or maintainability, but dependencies should remain explicit and minimal. No application should need to include every CLA module to use one feature.

## Status and roadmap

CLA is intended to expand over time with additional `cla_*` capabilities, more language implementations, improved conformance coverage, and documentation. The catalog above expresses the intended design direction. Individual library repositories or release pages should be treated as the source of truth for **what is currently available and stable**.

Initial areas of focus can include:

1. Establishing shared module naming, API conventions, error models, and conformance guidelines.
2. Implementing and documenting foundational modules such as console, conversion, strings, paths, files, and collections.
3. Defining precise cross-language contracts for typed dictionaries and object-oriented collections.
4. Expanding to formats, networking, cryptography, windowing, and other specialized capabilities.

## Who is CLA for?

CLA is for developers who work across languages, library and framework authors who need reusable foundations, educators who want to teach concepts that transfer across ecosystems, and teams that value predictable APIs without sacrificing native implementation choices.

You can still use standard libraries, language-specific frameworks, templates, and generics. CLA is another tool in your toolbox—not a restriction on how you write software.

---

**CLA — Cross Language Abstraction**  
*Don't start from scratch, bro.*
