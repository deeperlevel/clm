# CLM Table

## 1. Purpose

`clm_table` is an ID-addressed, dynamically sized table for string-valued data.
It is intended for configuration records, CSV rows, and other data with named
columns and ordered rows. Column names resolve through `clm_id`; row positions
are zero-based indexes.

This is not the same abstraction as [CLM Storage](./clm_storage.md). Storage is
a fixed-dimension grid of fixed-width values. A table owns variable-length text
cells and supports adding and removing rows and columns.

Use [CLM Tree](./clm_tree.md) for nested objects and typed BPA leaf values; a
table is for ordered records with a shared named-column schema.

## 2. Data model

A table has an ordered column schema and an ordered sequence of rows:

```text
Table
    columns : ordered sequence of (column_id, retained_column_name)
    rows    : ordered sequence of rows

Row
    cells   : zero or one string value per declared column
```

Each column ID is a `clm_id` obtained by resolving a previously registered
column name. A table retains the name spelling supplied when the column is
added. This permits later serialization without relying on reverse lookup,
which `clm_id` does not guarantee and which would be ambiguous when aliases
exist.

Column IDs must be unique within a table. Two different names that map to the
same ID cannot both be columns in that table. The table does not generate or
register column IDs; applications or parsers manage those mappings through
`clm_id`.

## 3. Cell values and missing cells

Every cell value is a Unicode string stored internally as UTF-8. Numeric-looking,
boolean-looking, and null-looking text is not converted:

```text
"1234"  remains the string "1234"
"true"  remains the string "true"
"null"  remains the string "null"
```

Cell values are copied into table-owned storage. Updating or deleting the source
string does not affect a table cell. Empty text is valid and different from a
missing cell. Newly added rows have missing cells; adding a column to a table
with existing rows also leaves those cells missing. Use `cell_contains` to
distinguish a missing value from an empty string. Reading a missing cell fails.

## 4. Schema and row behavior

- Columns are appended in declaration order. Removing a column removes its
  cells and compacts the remaining column positions without changing row order.
- Rows are appended or inserted in order. Insertion and removal shift later row
  indexes; row indexes are not stable identifiers.
- Removing a row deletes its cells. Clearing rows preserves the schema.
- Setting a cell requires an existing row and declared column. Setting replaces
  the current value or fills a missing cell.
- Removing a cell makes it missing again; it does not remove its row or column.

Tables may be sparse: a row need not have a value for every declared column.
Implementations must not silently replace a missing value with an empty string.

## 5. Public operations

The C declarations are in `clm/table.h`. The language-neutral operations are:

| Operation | Behavior |
|---|---|
| `clm_table_new(table_id)` | Creates an empty table. |
| `clm_table_delete(table_id)` | Deletes the table and all owned cells. |
| `clm_table_add_column(table_id, column_name)` | Resolves a registered name, stores its ID and spelling, and appends the column. |
| `clm_table_remove_column(table_id, column_id)` | Removes a column and all cells in it. |
| `clm_table_column_count(table_id)` | Returns the number of columns. |
| `clm_table_column_id_at(table_id, index, &column_id)` | Retrieves the mapped ID at a schema position. |
| `clm_table_column_name_to(table_id, index, buffer, capacity)` | Copies the retained UTF-8 name; returns byte length excluding NUL. |
| `clm_table_row_count(table_id)` | Returns the number of rows. |
| `clm_table_append_row(table_id)` | Appends an empty row and returns its index. |
| `clm_table_insert_row(table_id, index)` | Inserts an empty row before an index; the current row count appends. |
| `clm_table_remove_row(table_id, index)` | Deletes a row and compacts later row indexes. |
| `clm_table_clear_rows(table_id)` | Removes all rows while retaining columns. |
| `clm_table_cell_contains(table_id, row, column_id)` | Checks whether the cell has a value. |
| `clm_table_set_from(table_id, row, column_id, text)` | Copies valid NUL-terminated UTF-8 text into a cell. |
| `clm_table_set(table_id, row, column_id, source_string_id)` | Copies text from an existing CLM string block. |
| `clm_table_get(table_id, row, column_id, destination_string_id)` | Copies a present cell into an existing CLM string block. |
| `clm_table_remove_cell(table_id, row, column_id)` | Makes a cell missing without changing its row or column. |

### Results

Creation, deletion, row/column mutation, and cell removal return the common CLM
BPA success or failure status. Setting returns `CLM_BPA_SET_SUCCESS` or
`CLM_BPA_FAILURE`; retrieval returns `CLM_BPA_GET_SUCCESS` or
`CLM_BPA_FAILURE`. Row append/insert return the resulting zero-based row index
or `CLM_BPA_FAILURE`. Count operations return a nonnegative `clm_int` or
`CLM_BPA_FAILURE`. `cell_contains` returns a boolean.

An operation must fail without partially changing the table if validation or
allocation fails. Invalid table IDs, undeclared columns, out-of-range row
indexes, invalid UTF-8, and incompatible string resources are errors.

## 6. Example

```text
clm_id_register("host", 101)
clm_id_register("port", 102)

clm_table_new(CONFIG)
clm_table_add_column(CONFIG, "host")
clm_table_add_column(CONFIG, "port")
row = clm_table_append_row(CONFIG)          // 0
clm_table_set_from(CONFIG, row, 101, "Server111")
clm_table_set_from(CONFIG, row, 102, "1234")
```

A parser can add columns from a header row or from a known schema, append rows,
and set cells by their resolved column IDs. A headerless delimited format may
instead use an ordered row representation, since it has no names to map.

Retaining column names also supports serialization. A format serializer can
emit a header using the stored names and then output each row in order. It must
apply that format's quoting and escaping rules; the table itself does not define
CSV or other file syntax.

## 7. Ownership and concurrency

The table owns its column metadata and copies each cell's text. Callers retain
ownership of source buffers and source CLM string blocks. Deleting a table
releases all internal resources and invalidates its ID. As with other CLM
registries, implementations must document their concurrency behavior; the C
reference is not thread-safe unless explicitly changed.

## 8. Implementation status

`clm/table.h` implements the API using a private BPA metadata block, BPAA arrays
for ordered column IDs, retained column names, and row dictionary IDs, plus one
heterogeneous dictionary per row. Each present cell is a dictionary-owned BPA
string block. Missing cells have no row-dictionary entry; empty strings are
stored as present values.

Rows and columns are dynamically appended/inserted or removed. Column names
must resolve through `clm_id` before being added, and aliases that map to an
already-present column ID are rejected. The table owns row dictionaries and
cell copies, but callers retain ownership of source strings and the `clm_id`
registry. The test suite covers schema ordering, sparse cells, UTF-8 validation,
row and column mutation, copy independence, and cleanup.
