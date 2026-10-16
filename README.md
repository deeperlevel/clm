# Anansi

**Shared foundations. Native implementations.**

Anansi is a cross-language collection of reusable utilities for building **libraries, frameworks, and applications**. It defines predictable, language-neutral behavior for common tasks without requiring every implementation to use the same algorithms, standard-library APIs, or source layout.

Anansi is part of the **CLA-Source** approach: architecture and behavioral contracts are developed openly, independently of any one programming language. The shared specification is in [CLA.md](CLA.md); each language implementation follows that specification using its own conventions.

> **Status:** Anansi is being designed and developed. The capabilities below describe the intended scope, not a claim that every feature or language implementation is complete.

## Why Anansi?

A developer building Weaver, Webinator, SPIder, a game engine, or another framework shouldn't need to relearn the meaning of routine operations every time they change languages. Anansi provides a common vocabulary and a shared behavioral contract while preserving native implementation choices.

- **Language-agnostic design:** consistent results for defined inputs and operations across supported languages.
- **Native implementation freedom:** Python may use `pathlib` or `json`; Go may use standard packages; C++20 may use `std::filesystem` or another suitable approach.
- **Explicit data contracts:** serialization of application-defined objects does not depend on automatic reflection or runtime field discovery.
- **Practical scope:** small, reusable utilities rather than a replacement for an entire standard library.
- **Source-first releases:** modular development sources and optional generated single-file distributions for direct inclusion.
- **Independent adoption:** Anansi does not depend on Weaver, Webinator, or SPIder.

## Planned utilities

| Utility | Intended behavior |
| --- | --- |
| **path** | Path parsing and manipulation, including defined handling for Windows and POSIX-style paths. |
| **json** | Compact JSON reading and writing; reading may accept `/* block comments */` as an Anansi extension. |
| **yaml** | A deliberately limited, configuration-oriented YAML reader/writer; spaces-only indentation, no advanced YAML features; comments are not preserved when writing. |
| **toml** | Basic TOML reading and writing for a documented subset of the format. |
| **env** | Read and write simple key–value configuration text, retaining a defined notion of headers and blank lines. |
| **user_agent** | Parse a User-Agent string into a predictable result object. |
| **validation** | Small validation helpers, such as syntactic email-address checks. |
| **csv** | Parse CSV records into arrays of strings with documented quoting and delimiter behavior. |
| **panic** | Fail-fast utility for unrecoverable programming errors, mapped to native language behavior. |
| **assert** | Runtime invariant checking with behavior defined independently of optional language optimization flags. |

Additional utilities may be added when they are needed by real-world consumers. The initial scope intentionally **does not include `ustr`** or a custom Unicode arena/string model.

### Deliberately limited format support

Anansi aims for a **documented subset** of YAML and TOML, not comprehensive compatibility with every feature of those specifications. JSON with block comments is a nonstandard parsing extension; standard-compliant JSON output should remain possible. Comments are not assumed to survive a parse–serialize round trip.

The exact supported syntax, scalar types, errors, and round-trip guarantees belong in [CLA.md](CLA.md) and its future format-specific conformance fixtures. Until those details are formalized, do not assume acceptance of an arbitrary YAML, TOML, ENV, or CSV document.

## Cross Language Architecture (CLA)

**Cross Language Architecture** describes architecture and observable behavior independently of a particular programming language. Each implementation may use native standard-library facilities and language idioms, provided it preserves the shared contract.

The goal is not identical source code. It is **equivalent behavior and portable architectural knowledge**.

For Anansi, this means:

1. Define operations, input/output behavior, and failure cases in CLA.
2. Implement those behaviors in a language-native way.
3. Test conformance using language-independent fixtures where practical.
4. Propose CLA changes when an implementation exposes ambiguity or a portability problem.

Read [CLA.md](CLA.md) for the architectural specification and guidance.

## Language implementations

Anansi's initial development targets are **Python**, **Go**, and **C++20**. This is a development goal, not a statement that all three are complete. Other languages are welcome through CLA-Source contributions.

Suggested repository layout:

```text
anansi/
├── README.md
├── CLA.md
├── LICENSE
├── open_for_cla_contributions.md
├── src/
│   ├── python/
│   ├── go/
│   ├── cpp/
│   ├── c/
│   ├── d/
│   ├── ruby/
│   │   └── open_for_cla_contributions.md
│   └── pascal/
│       └── open_for_cla_contributions.md
└── tests/
    └── conformance/
```

The directory listing is illustrative; implementation status should be reported by the repository, not inferred merely from the presence of a folder.

## Dependencies and implementation policy

Anansi **prefers standard-library functionality** and avoids unnecessary dependencies. Using `pathlib` or `json` internally in Python is encouraged when the behavior fits the CLA contract; no bespoke path or parser implementation is required just for its own sake.

Some capabilities, such as YAML support, may need additional code or a carefully chosen dependency in a particular language. Such dependencies should be documented clearly, and should not alter the cross-language public contract. A third-party library must not become an implicit requirement of CLA itself.

> CLA defines *what Anansi guarantees*. Each language implementation decides *how to provide it*.

## Reflection-free public data mapping

Anansi parsers can return ordinary values, maps, arrays, or a shared logical value model. Automatic discovery of application object fields is **not** part of the required serialization contract. Where mapping to user-defined objects is supported, callers provide the schema and/or conversion functions explicitly.

Using built-in JSON libraries to parse ordinary dictionaries or maps does not violate this principle. What Anansi avoids is requiring reflection-based object discovery as a cross-language feature.

## Source-first distribution

Development source is organized into multiple language-specific files. A separate **Packer** utility can create release artifacts by combining those files and optimizing bundled imports or includes.

Depending on the language and project, a GitHub Release may contain:

- One Python module (`anansi.py`) that can be copied directly into an application.
- One C distribution header (`anansi.h`), potentially using a single-implementation translation-unit pattern.
- One C++20 distribution header (`anansi.hpp`), usable directly where supported.
- Other native, editable single-file artifacts where practical.

The **release artifact is source code**, not necessarily a compiled binary. It can be inspected and modified locally. Package-manager distributions may be offered as conveniences, but are not a prerequisite to using the generated source files.

A single-file artifact does **not** imply that platform or approved runtime dependencies magically disappear.

### Release quality

The modular source and the **actual generated release files** should each be tested. The generated artifacts should carry version and source-revision information, and releases should document any external dependencies. Keeping a generated file under version control in a consuming project helps teams review local modifications and upgrades.

## Contributing

Contributions are welcome to both the CLA specification and language implementations. You do **not** need to know Python or C++ to contribute an implementation in your own language.

Start with [open_for_cla_contributions.md](open_for_cla_contributions.md). A language folder may also contain its own contribution invitation.

Useful contributions include clearer behavioral specifications, new conformance fixtures, implementations, portability fixes, and documentation.

## Ecosystem

Anansi is independent and usable on its own. It is intended to provide reusable foundations for projects such as **Weaver** (dependency injection), **Webinator** (mediator/message dispatch), and **SPIder** (application hosting). These higher-level projects do not define Anansi's architecture.

## License

See [LICENSE](LICENSE) for the repository's actual license terms once selected and published. No license is implied solely by this README.
