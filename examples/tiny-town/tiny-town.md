# CLM Tiny Town

**CLM Tiny Town** is a console-based simulation of a small town, its residents, their jobs, and its resources. Players advance the town one day at a time, inspect its status, assign jobs, and respond to scheduled events.

The example primarily demonstrates how CLM parses multiple configuration and data formats into a working application. Town settings, simulation rules, residents, and events are read from INI, ENV, TOML, JSON, and YAML files. Changes to the town can be saved, and selected configuration files can be reloaded without restarting the simulation.

## CLM modules demonstrated

- [clm_string_ini](/docs/clm_string_ini.md) — Parses display preferences and save settings from INI.
- [clm_string_env](/docs/clm_string_env.md) — Parses local environment-style configuration.
- [clm_string_toml](/docs/clm_string_toml.md) — Parses simulation rules and resource production rates.
- [clm_string_json](/docs/clm_string_json.md) — Loads and saves persistent town and citizen data.
- [clm_string_yaml](/docs/clm_string_yaml.md) — Parses scheduled town events.
- [clm_tree](/docs/clm_tree.md) — Provides access to parsed, structured data.
- [clm_id](/docs/clm_id.md) — Resolves identifiers for data fields.
- [clm_string](/docs/clm_string.md) — Holds text used during parsing and serialization.
- [clm_file](/docs/clm_file.md) — Reads configuration files and writes saved state.
- [clm_bpa](/docs/clm_bpa.md) — Supports ID-based data storage.
