# CLM String Format

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md), [clm_file](clm_file.md), [clm_id](clm_id.md), [clm_path](clm_path.md), [clm_platform](clm_platform.md), [clm_string](clm_string.md), [clm_table](clm_table.md), [clm_tree](clm_tree.md).
- **External libraries / platform facilities (C reference):** None beyond the language runtime and standard platform facilities.

## Purpose

Shared low-level parsing and formatting utilities used by CLM structured-text format modules.

## Capabilities and operations

- Provide common tokenization, type conversion and traversal helpers.
- Support generic access to the CLM tree/table representation and formatting primitives.
- Act as common infrastructure for INI, JSON, TOML, YAML and ENV codecs.
- The shared helpers are not a separate universal clm_formats document model.

## Shared parsing infrastructure

`clm_string_format` is not itself an INI, JSON, YAML or TOML parser. It provides reusable pieces for those format modules, such as lexical scanning, field-name handling, scalar conversion, structured tree helpers and common output logic.

## Division of responsibility

| Operation | Module |
|---|---|
| Shared parsing/formatting machinery | `clm_string_format` |
| INI documents | [clm_string_ini](clm_string_ini.md) |
| JSON documents | [clm_string_json](clm_string_json.md) |
| TOML documents | [clm_string_toml](clm_string_toml.md) |
| YAML documents | [clm_string_yaml](clm_string_yaml.md) |
| Environment assignments | [clm_string_env](clm_string_env.md) |
| Delimited tables | [clm_string_dsv](clm_string_dsv.md) |

The formats share typed CLM trees and tables as appropriate. A previous `clm_formats` universal document API is not part of the current module inventory.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Provide common tokenization, type conversion and traversal helpers.
