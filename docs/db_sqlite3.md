# clm_db_sqlite3

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

SQLite connection and prepared-statement driver.

## Behavior and constraints

SQLite connection and prepared-statement driver.

SQLite result codes are translated into CLM database statuses. Statement handles and
database handles have distinct finalization paths.

## Integration

- C reference: [**clm_db_sqlite3**](../c/db_sqlite3.md).
- Related CLM declarations in this archive: `clm_db`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
