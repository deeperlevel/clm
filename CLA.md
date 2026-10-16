# Anansi — Cross Language Architecture (CLA)

> **Status: Draft architectural specification.** This document records Anansi's intended cross-language design and currently agreed scope. Details marked **To specify** are intentionally open; implementations must not invent conflicting guarantees and present them as established CLA behavior.

## 1. What is Cross Language Architecture?

**Cross Language Architecture (CLA)** is an approach to designing software architecture and behavioral contracts independently of any single programming language. It lets developers working in different languages discuss the same concepts, implement the same public behavior, and contribute improvements to a shared specification.

CLA is **not** a programming language, transpiler, wire protocol, or claim that different languages execute code identically. It separates:

- **The contract:** operations, inputs and outputs, invariants, failure conditions, and externally visible behavior.
- **The implementation:** algorithms, allocation strategies, standard libraries, compiler features, and idiomatic language syntax.
- **Conformance:** tests and examples showing that implementations preserve the contract.

A Python implementation can use Python's standard library. A Go implementation can use Go's packages. A C++20 implementation can use C++ facilities. None is inherently the authoritative definition of Anansi: **this CLA document is the starting point for resolving behavioral disagreements**.

### 1.1 CLA-Source

**CLA-Source** is an open collaboration model in which architectural specifications can be reviewed, improved, and versioned alongside source-code implementations. Contributions may improve the common contract even when the contributor does not write code in every supported language.

Implementations should be created from the CLA contract, not merely translated line-by-line from another language implementation.

## 2. Guidelines for language implementations

The following are **guidelines**, not universal prohibitions:

1. **Prefer explicit contracts.** Define inputs, outputs, and behavior without relying on language-specific conventions.
2. **Avoid reflection and runtime introspection as required architectural mechanisms.** They are not equally available or equivalent across languages. An implementation may use native introspection internally if the shared API does not depend on it.
3. **Avoid dynamic discovery as a requirement.** Do not require runtime class scanning, automatic field inspection, or method-name lookups to implement Anansi's public contracts.
4. **Reuse dependable native facilities.** Do not reinvent `pathlib`, `encoding/json`, or `std::filesystem` when they can implement the specified behavior correctly.
5. **Keep formats and failure semantics explicit.** Unsupported syntax should produce a defined failure rather than surprising partial results.
6. **Prefer small, composable utilities.** Anansi is a foundation, not a general-purpose application framework.
7. **Respect language-native ownership and error conventions.** Equivalent semantics are required; identical implementation mechanisms are not.
8. **Validate actual behavior.** Shared fixtures and tests matter more than superficial API-name similarity.

### 2.1 Required vs recommended

The absence of reflection-dependent **public behavior** and preservation of documented input/output semantics are architectural goals. A recommendation to avoid a particular language feature internally is **not** itself a prohibition. The cross-language contract must remain implementable without relying on that feature.

## 3. Anansi's scope

Anansi is a collection of reusable utilities for library and framework authors. It has no dependency on Weaver, Webinator, or SPIder.

Initial areas:

- `path`
- `json`
- `yaml`
- `toml`
- `env`
- `user_agent`
- `validation`
- `csv`
- `panic`
- `assert`

`ustr` and custom arena-backed Unicode string handling are **deferred** and are not part of the initial contract.

All implementations should document which CLA utility groups they support and which specific CLA revision they target.

## 4. General data and serialization principles

### 4.1 Explicit application metadata

Anansi serialization must not require automatic introspection of user-defined class or struct fields. An implementation may offer operations on ordinary JSON-style maps/arrays/primitives, or accept **explicitly supplied schemas, field descriptors, or conversion callbacks**.

For example, a caller explicitly describes the fields to serialize rather than relying on automatic discovery of an object's attributes.

The metadata representation may vary by language; the **logical field names, supported value types, and observable result** must follow the relevant utility contract.

### 4.2 Common logical values

Where useful, parsers and serializers should work with the following portable categories:

- null / absent value (where meaningful)
- boolean
- string
- integer
- floating-point value (within documented precision/range)
- ordered array
- string-keyed mapping/object

Not every format supports every category in the same way. The CLA specification must document format-specific limitations rather than silently coercing incompatible values.

### 4.3 Failure reporting

Malformed documents, unsupported features, invalid metadata, and output conversion failures must not be silently accepted as valid input. Anansi should expose a clear error/result according to the host language's conventions.

**To specify:** Common error categories, positions (line/column or byte offset), and error-message guarantees for each format.

## 5. Utility contracts

### 5.1 `path`

Provide familiar path operations, with defined handling of **Windows** and **POSIX** syntax where appropriate.

Potential operations include constructing a path value, joining segments, extracting filename, stem, extension, and parent, and performing lexical normalization.

The specification distinguishes **lexical path manipulation** from operations that query the real filesystem. Windows-style paths should be parsable independently of the host operating system when a style is specified.

Native helpers such as Python `pathlib` and C++20 `std::filesystem` may be used internally, provided the CLA-defined behavior is preserved.

**To specify:** Exact style-selection rules, case sensitivity, drive/UNC handling, joining absolute paths, normalization, and filesystem-dependent behaviors.

### 5.2 `json`

Provide **basic JSON deserialization and compact serialization**.

- Standard JSON values form the baseline.
- Reading may accept C-style `/* ... */` block comments as a documented Anansi extension.
- Block comment markers inside quoted strings must not be treated as comments.
- Comment information need not be retained after parsing.
- Pretty-print output is not a required feature.
- No reflection-based field discovery is required or assumed.

Because comments are not part of standard JSON, implementations must document when the extension is enabled and should be able to emit ordinary valid JSON.

**To specify:** Duplicate-key behavior, numeric limits, error location, acceptance of trailing commas (not assumed), and how comment mode is selected.

### 5.3 `yaml`

Provide **basic, configuration-oriented YAML reading and writing**, not an implementation of the full YAML specification.

- **Spaces only** for indentation: tabs and mixed indentation are not supported.
- Advanced YAML features are out of scope for the initial version.
- Comments may be ignored during loading and are **not preserved** for round-trip writing.
- The supported scalar, array, and mapping forms must be explicitly defined.
- No automatic introspection of application objects is required.

**To specify:** Precise grammar subset, indentation widths, scalar quoting/escaping, null values, sequences, nested mappings, and treatment of YAML type inference.

### 5.4 `toml`

Provide basic TOML deserialization and serialization for a **documented subset** of TOML, suitable for simple application and library configuration.

No reflection-based object discovery is required. Implementations may use standard-library parsing facilities where available, supplemented by writing support or limitations clearly stated.

**To specify:** Supported TOML version and exact rules for tables, dotted keys, arrays, array-of-tables, numeric types, dates/times, and serialization of comments.

### 5.5 `env`

Provide a lightweight reader/writer for **key–value configuration text** with defined handling of headers and empty lines. ENV **files** and the operating system's **process environment** are separate concerns; the CLA must not silently treat them as identical.

**To specify:** Header syntax, key validation, quoting, escaping, comments, duplicate keys, blank-line preservation, and optional variable expansion. Variable expansion is **not assumed**.

### 5.6 `user_agent`

Accept a User-Agent string and return a predictable logical result (for example, product/browser, operating-system family, and device-related classifications when identifiable).

User-Agent strings are inconsistent and can be spoofed. Parser results are **best-effort hints**, not proof of device identity or a security boundary.

**To specify:** Result schema, unknown values, matching precedence, and shared test fixtures.

### 5.7 `validation`

Provide small, named validation functions, such as `is_valid_email_address`. Each function must state the property it actually checks and its limitations.

An email syntax check does **not** establish mailbox existence, ownership, or deliverability. Validation helpers are not automatically authorization or security checks.

**To specify:** Exact validators and normative acceptance/rejection examples.

### 5.8 `csv`

Provide parsing of CSV text into arrays of strings, preserving field order and empty fields as defined by the chosen dialect.

**To specify:** Delimiter, quote/escape rules, newline handling, malformed input, header-row support, and whether writing is included.

### 5.9 `panic`

Provide a clear **fail-fast operation** for unrecoverable programming errors. Each language may use its native mechanism: panic, a fatal exception, explicit termination, or an equivalent documented behavior.

The CLA must document whether callers can intercept a panic and should not imply that process termination is guaranteed in every runtime without a separate policy.

### 5.10 `assert`

Provide a **runtime invariant check** that remains active regardless of language optimization flags. Failed assertions invoke the documented fail-fast mechanism or a similarly explicit fatal failure path.

This utility is intended for programming invariants, not ordinary input validation or recoverable user errors.

## 6. Implementation independence and dependencies

Anansi prefers the underlying language's standard library and avoids dependencies on unrelated application frameworks.

Native reusable functionality is welcome:

- Python may wrap `pathlib`, `json`, `csv`, or `tomllib` (for TOML reading).
- Go may use its standard path, JSON, CSV, and other relevant packages.
- C++20 may use `std::filesystem` and other standard facilities.

A format without suitable standard-library support (notably YAML in Python) may require custom implementation or an **explicitly documented optional/approved dependency**. Whether a dependency is accepted is a repository policy decision, not a CLA architectural requirement.

The common contract must not assume all languages have identical standard-library capabilities.

## 7. Compatibility and conformance

Each supported utility should eventually have **language-neutral fixtures** covering success cases, invalid input, and edge cases. Implementations can use their own test frameworks but should run equivalent fixtures.

Conformance should be reported per **CLA specification version** and per utility group. Language implementation versions may differ.

A change that alters the observable contract should be proposed in `CLA.md` before claiming all implementations conform. An implementation-specific optimization that preserves behavior does not need to change the CLA contract.

**To specify:** The initial conformance fixture schema, versioning rules, and minimum requirements for declaring a utility conformant.

## 8. Modular development and single-file distribution

The source repository organizes development files by programming language:

```text
anansi/
├── README.md
├── CLA.md
├── src/
│   ├── python/
│   ├── go/
│   ├── cpp/
│   ├── c/
│   └── ...
└── tests/
    └── conformance/
```

Developers maintain readable, modular sources. An external **Packer** utility assembles release artifacts that are easy to adopt directly:

- `anansi.py`: generated Python single-file source library.
- `anansi.h`: generated C single-header distribution, if that implementation supports it.
- `anansi.hpp`: generated C++20 single-header distribution, if that implementation supports it.
- Equivalent single-file source distributions in other languages where practical.

The Packer may combine files, order definitions, remove redundant imports/includes, and adapt language-specific packaging. It should not rewrite the library's behavioral contract.

### 8.1 Source-first distribution principles

1. **Generated files are editable source**, not mandatory binary packages.
2. **Single-file release is a packaging choice**, not a requirement to develop in a single file.
3. **Official release artifacts should be reproducible** from identified source revisions where feasible.
4. **Versions and provenance should be visible** in generated outputs.
5. **Unit tests should exercise generated artifacts**, not only modular development code.
6. **Package managers are optional distribution channels**, not architectural dependencies.

A single-header C file may need one translation unit defining an implementation macro; a C++ header-only release must obey C++ linkage/ODR rules. Both can be single-file distributions while using distinct compilation models.

A generated artifact may still rely on standard or documented external runtime dependencies.

## 9. CLA-Source contributions

A contribution may consist of an implementation in a new language, a change to an existing implementation, a proposed CLA clarification, conformance test fixtures, or documentation.

Suggested sequence:

1. Read the repository README and this CLA.
2. Choose a utility group and language.
3. Identify missing/ambiguous behavior before implementing it.
4. Propose clarifications to CLA where necessary.
5. Build an idiomatic implementation that satisfies the shared contract.
6. Provide conformance tests and packing/release instructions where applicable.

See [open_for_cla_contributions.md](open_for_cla_contributions.md) for a contribution invitation template.

## 10. Guiding principle

**Anansi defines common behavior, not common source code.**

The architecture belongs to all contributors. Every implementation should remain recognizable through its contracts while being free to use the strengths of its programming language.
