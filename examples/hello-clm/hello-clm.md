# CLM Hello

**CLM Hello** is an interactive console program that displays information based on the user's menu selection. It introduces CLM's ID-based objects and demonstrates how those objects can be converted into readable text and presented in a console.

Users can explore a heterogeneous dictionary, a table loaded from CSV, and a tree loaded from YAML. The example also displays the tree as JSON or YAML, toggles colored output, and inspects an underlying storage block.

## CLM modules demonstrated

- [clm_console](/docs/clm_console.md) — Console input, output, and object display.
- [clm_stringtify](/docs/clm_stringtify.md) — Converts CLM-managed objects into readable representations, used by the console's object output.
- [clm_id](/docs/clm_id.md) — Registers names and identifies CLM resources.
- [clm_string](/docs/clm_string.md) — Manages text resources and menu content.
- [clm_dictionary](/docs/clm_dictionary.md) — Stores values of different types in a dictionary.
- [clm_table](/docs/clm_table.md) — Represents tabular data.
- [clm_tree](/docs/clm_tree.md) — Represents structured, hierarchical data.
- [clm_string_dsv](/docs/clm_string_dsv.md) — Loads CSV data into a table.
- [clm_string_yaml](/docs/clm_string_yaml.md) — Loads YAML data into a tree.
- [clm_string_json](/docs/clm_string_json.md) — Supports the JSON representation of structured data.
- [clm_bpa](/docs/clm_bpa.md) — Provides underlying object storage and block inspection.
