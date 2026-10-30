# CLM Tiny Town

**Example:** Tiny Town (Crabville)  
**Category:** Configuration, structured data, serialization, and state persistence  
**Application:** Interactive console town simulator  
**Purpose:** Demonstrate how five CLM data-format modules and the common CLM tree, string, file, and ID facilities work together in a small, deterministic application.

## 1. Overview

Tiny Town is a **console-only simulation** of a town with citizens, jobs, a calendar, resources, and scheduled events. Users inspect the town, assign jobs, advance the simulation one day at a time, reload configuration, and save progress.

It does not use a graphics engine, HTTP server, sockets, or random event generator. The simulation exists to teach a practical data pipeline:

**Read a file → parse a format → inspect a shared tree → validate values → run the application → update the tree → serialize and save.**

The five example files have deliberately different responsibilities. The application does not store the same information in five equivalent formats merely to demonstrate five parsers.

## 2. Five formats, five responsibilities

| Format | Example file | CLM module | Responsibility |
|---|---|---|---|
| **INI** | `settings.ini` | `clm_string_ini` | Console display preferences and the option to automatically save after advancing a day. |
| **ENV** | `.env` | `clm_string_env` | Explicitly loaded runtime-style options, including the town name and debug display flag. |
| **TOML** | `town.toml` | `clm_string_toml` | Simulation rules such as population limit, days per year, production rates, and food consumption. |
| **JSON** | `citizens.json` | `clm_string_json` | Persistent simulation state: calendar, resources, citizens, and assigned jobs. Also provides serialization when saving. |
| **YAML** | `events.yaml` | `clm_string_yaml` | Scheduled world events and their effects on town resources. |

**ENV distinction:** Reading a `.env` file with `clm_string_env` is a deliberate parsing operation. The example does not assume that it automatically exports values into the operating system's process environment. It is not a secrets-management demonstration.

**JSON distinction:** The JSON file is the mutable save state. The other four files primarily describe settings or content that users may edit and reload.

## 3. Supporting CLM modules

| CLM module | How Tiny Town uses it |
|---|---|
| **`clm_file`** | Opens/reads the five UTF-8 data files and writes the serialized JSON save state back to disk. |
| **`clm_string`** | Holds file text as CLM-managed string resources and receives serialized JSON text. Also supports text validation. |
| **`clm_tree`** | Provides the shared typed object/array representation used after parsing INI, ENV, TOML, JSON, and YAML. The program retrieves objects, arrays, and values, and updates the JSON tree before saving. |
| **`clm_id`** | Resolves property names into the ID keys used to look up fields in parsed trees. This keeps names understandable without requiring each parser to invent its own property system. |
| **`clm_bpa`** | Supplies the underlying ID-based storage and lifetime management for CLM strings and parsed tree resources. |
| **`clm_base`** | Supplies common CLM identifiers and fundamental types used throughout the modules. |

The format modules handle **syntax**. The shared tree module handles **structured data**. The example application handles **business rules and validation**. Those responsibilities should remain separate in every language port.

## 4. File and data flow

```text
settings.ini  -- clm_string_ini  --+
.env          -- clm_string_env  ---+
town.toml     -- clm_string_toml ---+--> clm_tree --> validate settings
 events.yaml  -- clm_string_yaml ---+                       |
                                                           v
                                                 Active simulation rules
                                                           |
                                                           v
                                                  Next-day calculations
                                                           |
citizens.json -- clm_string_json --> clm_tree --> town state / citizen jobs
                                         ^                 |
                                         |                 v
                                update saved fields <-- in-memory changes
                                         |
                                         v
                                  JSON serialization
                                         |
                                         v
                                clm_file --> citizens.json
```

All five input files are read through `clm_file` and held as text using `clm_string` before their respective parsers create CLM tree resources. They are **different inputs to a common structured-data interface**, not five unrelated processing engines.

After validation, the application keeps a small, language-native snapshot of the town's current rules and state. This allows daily simulation logic to operate on clearly defined values instead of repeatedly traversing a parser tree. The owned JSON tree remains available for saving, so known fields can be updated without rebuilding the entire document or deliberately discarding unrelated properties.

## 5. What each configuration file demonstrates

### INI — `settings.ini`

Contains simple, grouped preferences such as `verbose_days` and `autosave`. It demonstrates that presentation options and save policy can be changed independently of simulation rules. The example interprets boolean-looking INI values according to its own validation rules.

### ENV — `.env`

Provides values such as `CLM_TOWN_NAME=Crabville` and `CLM_DEBUG=false`. It demonstrates explicitly loading environment-style key/value text from a file. These values affect the visible town name and optional debug messages; they do not mutate the parent process's environment.

### TOML — `town.toml`

Contains structured sections for town limits and economy rules. For example, the number of days in a simulated year, food produced by farmers or hunters, wood produced by builders, and food consumed per citizen are read from TOML rather than hardcoded into the application logic.

### JSON — `citizens.json`

Contains the year, current day, food, wood, gold, and citizen array. Each citizen has a name and job. JSON is loaded at startup into the current simulation state, modified as commands are executed, and serialized when the user saves.

### YAML — `events.yaml`

Contains a list of events, each with a name, scheduled day, and resource adjustments. A festival might add food and gold; a storm might remove wood. Events are data-driven: adding or adjusting a valid YAML event changes future simulation behavior without recompiling the application.

In the reference scenario, scheduled events recur on their configured day of each simulated year.

## 6. Console commands

| Command | Expected behavior | Modules most directly involved |
|---|---|---|
| `status` | Display the current calendar, population, and resource totals. | Previously loaded state from `clm_string_json` / `clm_tree`. |
| `citizens` | List citizens and their jobs. | Previously loaded JSON state. |
| `events` | List scheduled events and their effects. | `clm_string_yaml` / `clm_tree`, via the active settings snapshot. |
| `next` | Advance exactly one simulated day and apply configured rules. | TOML-derived rules, YAML-derived events, JSON-derived state. |
| `hire Alice Builder` | Change Alice's job in working state; persist it on save. | The application's state, then `clm_tree` / JSON during saving. |
| `save` | Update the stored JSON tree, serialize it, and write `citizens.json`. | `clm_tree`, `clm_string_json`, `clm_string`, `clm_file`. |
| `reload` | Re-read and validate INI, ENV, TOML, and YAML without resetting current state. | The four configuration parsers, `clm_file`, `clm_string`, `clm_tree`. |
| `help` | Display supported commands. | Application console interface. |
| `quit` | Exit the application. | Application lifecycle. |

## 7. Deterministic simulation rules

Advancing one day follows a fixed order:

1. Advance the calendar, rolling to the next year when the configured year length is exceeded.
2. Compute food and wood production from each citizen's assigned job and the active TOML rates.
3. Subtract food consumption based on the population and configured consumption per citizen.
4. Apply YAML events scheduled for the new day.
5. Display the resulting town state and optionally save it if INI autosave is enabled.

The initial reference has jobs such as **Farmer**, **Hunter**, and **Builder**. Rules for producing and consuming resources are intentionally modest. Resource balances are prevented from becoming negative, and the implementation guards against arithmetic overflow.

**No timers or random numbers are involved.** Given the same initial files, the same ordered user commands, and the same validation policies, implementations in different languages should reach equivalent simulation states. This makes Tiny Town useful for CLM cross-language conformance tests.

## 8. Reload and save semantics

### Reload

`reload` refreshes **INI, ENV, TOML, and YAML only**. It does not reload `citizens.json`, reset the day, or silently undo unsaved job changes.

New settings are parsed and validated before becoming active. If an input is invalid or the new settings conflict with existing state—for example, a population limit smaller than the current population—the reload is rejected and the previously active settings remain in effect.

### Save

`save` synchronizes the in-memory state with the already owned JSON tree, including the calendar, resources, and citizen jobs. The JSON serializer converts that tree into text, and the file module writes the result to `citizens.json`.

Updating the existing JSON tree instead of recreating a minimal object helps retain unrelated fields that may be present in the file. The initial example performs direct file replacement; production software should consider temporary files, atomic replacement where supported, and durability/error recovery.

## 9. Resource ownership and error handling

Tiny Town is intentionally **single-threaded**. It does not need `clm_async`, `clm_async_pool`, `clm_channel`, or networking modules. This also avoids implying that BPA's ID-based resources are automatically safe for concurrent access.

The application explicitly owns the CLM strings and trees it creates. A successful parse produces a new tree resource; it does **not** transfer ownership of the source string. Input strings, output strings, root trees, and child references therefore have different ownership rules:

- **Owned root trees** must be deleted when no longer needed.
- **Owned CLM strings** must be deleted independently of parsed trees.
- **Borrowed child tree IDs** remain valid only while their owning tree is alive and unchanged in a way that invalidates them.
- **Validated snapshots** used by the simulation are application values, not automatically synchronized CLM resources.

File-reading errors, invalid UTF-8, malformed source syntax, missing required fields, type mismatches, and invalid settings must all be detected before unsafe or inconsistent state transitions occur. A format parser's success is not equivalent to application-level validation success.

## 10. Cross-language porting expectations

A port does not need to copy C data structures, compiler macros, or build instructions. It should reproduce the **observable application contract**:

- Load the same five files with the corresponding language's CLM modules.
- Use a common typed representation of parsed structured data.
- Validate expected keys and types independently of syntax parsing.
- Preserve the same simulation order, command meanings, and persistence behavior.
- Keep `reload` separate from save-state loading.
- Produce compatible JSON state that another language's Tiny Town implementation can read.
- Manage resource IDs and ownership according to that language's CLM contract.

One especially useful interoperability test is to **save the town in one language, then resume it in another**, confirming matching citizens, jobs, resources, and calendar values.

## 11. Scope and learning objectives

Tiny Town is a compact educational simulation, not a production game engine, database, transaction system, or secret vault. Its five file formats are not interchangeable simply because they all produce trees; they are chosen to demonstrate different data responsibilities.

**Primary lesson:** CLM's format-specific parsers can share a consistent data model, while file I/O, strings, ID lookup, validation, simulation logic, and persistence remain separate, reusable concerns.
