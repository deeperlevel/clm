# CLM String DSV

## Purpose

`clm_string_dsv` reads delimited records into a `clm_table` and serializes a
table back to delimited text. CSV is the default profile: comma-separated
fields. This is a practical CLM format, not a claim of full RFC 4180
conformance.

## Schema and records

By default, the first record defines the table columns. Each header name is
looked up in the field-ID registry; unknown names are auto-registered with a
collision-free ID. Existing mappings are reused. Duplicate names or distinct
names that resolve to the same ID fail. Registrations created by a successful
parse remain in the registry; registrations created by a failed parse are
rolled back.

With `has_header` disabled, every record is data. The first record determines
the column count; columns are named `column_1`, `column_2`, and so on, and each
name is registered through `clm_id` using the same collision-safe policy.
Every field value is retained. Serialization in this mode omits the header
record and emits all rows as data.

When a header is present, each data record must have exactly the header's field
count. Values are stored as strings. An empty file produces an empty table;
blank records and trailing separators represent empty fields.

## Separators and quoting

The default separator is comma. An option may instead name a CLM string
containing one or two ASCII bytes, such as `||`. Quotes and record-ending CR or
LF are not valid separator bytes.

Fields may be unquoted, single-quoted, or double-quoted. Double-quoted fields
support exactly these escapes: `\\`, `\"`, `\n`, and `\t`. Single-quoted
fields represent an apostrophe by doubling it (`''`); backslashes have no
special meaning in single-quoted fields. Unsupported escapes, unterminated
quotes, or physical CR/LF inside a quoted field fail. Use the `\n` escape for
a newline value.

Serialization emits every field as a double-quoted value, escaping backslash,
double quote, newline, and tab. A cell containing carriage return cannot be
serialized by this subset. Missing cells serialize as empty quoted fields, so a
missing cell and a present empty string become indistinguishable after a
round-trip. Output records use LF line endings.

## Operations and encoding

`loads` parses an existing CLM string ID into a new table; `load` reads from a
file named by a CLM string ID. `dumps` writes to an existing CLM string ID;
`dump` writes to a file named by a CLM string ID. Options variants accept a
separator string ID and header setting. There are no raw C-string, caller
buffer, or raw-path codec entry points.

CLM strings store validated Unicode as UTF-8 internally. File input and output
use UTF-8. A new CLM string gets the current preferred external encoding from
`clm_string`; updating an existing string preserves its encoding preference.
Operations return CLM BPA success/failure statuses.