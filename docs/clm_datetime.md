# CLM - DateTime

## Dependencies

The current CLM reference implementation composes these modules:

- [clm_base](./clm_base.md) — shared CLM identifiers, scalar types, and conventions.
- [clm_bpa](./clm_bpa.md) — ID-addressed, typed storage and lifecycle for date/time values.
- [clm_string](./clm_string.md) — parsing from and formatting into CLM string resources.

**External dependencies:** None required directly. Reading the current date and time uses the host platform's system clock and local-time facilities; calendar operations and fixed-offset conversions require no external date/time library.

## 1. Purpose and scope

`clm_datetime` provides **date**, **time**, and **datetime** values with strict ISO-style text conversion, Gregorian calendar operations, fixed UTC offsets, comparisons, arithmetic, and Unix-millisecond conversions. Values may be stored under CLM resource IDs or handled directly by value for supported formatting and validation operations.

The module deliberately focuses on **calendar values and fixed offsets**, not a full timezone database. It does not resolve named geographic timezones, historical daylight-saving transitions, or local-time ambiguities. Language implementations may use native date/time types, provided they maintain the defined value ranges and behavioral distinctions.

## 2. Value kinds and representation

| Kind | Meaning | Text example |
|---|---|---|
| `DATE` / `clm_date` | Proleptic Gregorian year, month, and day. | `2026-11-02` |
| `TIME` / `clm_time` | Wall-clock hour, minute, second, millisecond, and optional fixed UTC offset. | `11:06:00.000+08:00` |
| `DATETIME` / `clm_datetime` | Calendar date together with a time and optional fixed offset. | `2026-11-02T11:06:00.000+08:00` |

An offset is measured in **whole minutes east of UTC**: `+08:00` corresponds to `+480`; `-05:30` corresponds to `-330`. A value can have **no offset** (offset-naive) or an explicitly present offset, including UTC (`Z`). An absent offset must not be silently interpreted as UTC or the system's local zone.

Supported values are:

- Years **0001–9999**, using Gregorian leap-year and month-length rules.
- Months **1–12** and valid day-of-month values.
- Hours **0–23**, minutes **0–59**, seconds **0–59**, and milliseconds **0–999**.
- Fixed offsets **−23:59 to +23:59**, inclusive, when an offset is present.

The reference implementation uses distinct BPA type tags for `DATE`, `TIME`, and `ISO_DATETIME`. They are different stored resource kinds even though date and time components can be retrieved from a datetime.

## 3. Text parsing and formatting

The accepted forms are a strict, intentionally limited subset of ISO 8601:

| Kind | Accepted input format | Examples |
|---|---|---|
| Date | `YYYY-MM-DD` | `2024-02-29` |
| Time | `HH:mm:ss[.f[f[f]]][Z\|+HH:mm\|-HH:mm]` | `09:30:00`, `09:30:00.5Z` |
| Datetime | Date, literal `T`, then time | `2026-11-02T09:30:00.250+08:00` |

Fractional seconds may contain **one, two, or three digits**. These are interpreted as decimal fractions of a second: `.5` = 500 ms, `.05` = 50 ms, `.005` = 5 ms. An explicit `Z` denotes zero UTC offset. Numeric offsets use a colon and two-digit hour/minute fields.

Input rejects invalid dates, leap seconds, `24:00`, unsupported fractional precision, surrounding whitespace, timezone names, ordinal/week dates, compact date/time variants, and additional trailing text.

**Formatting is normalized**, always writing exactly three fractional second digits for a time or datetime. A present zero offset is written as `Z`; an absent offset produces no suffix. Dates use `YYYY-MM-DD`, and datetimes use `T` between date and time.

The reference implementation's maximum formatted buffer requirement is **30 bytes including its terminating NUL**. Language ports may expose idiomatic string results while preserving the same textual representation.

## 4. Resource lifecycle and access

Date, time, and datetime values can be registered under caller-chosen CLM IDs. A setter or parser creates a missing resource or replaces a resource **of the same kind**. It does not replace a registered resource of another kind.

| Operation | Behavior |
|---|---|
| `clm_datetime_set_date(id, value)` | Creates or updates a validated date. |
| `clm_datetime_set_time(id, value)` | Creates or updates a validated time. |
| `clm_datetime_set_datetime(id, value)` | Creates or updates a validated datetime. |
| `clm_datetime_get_date(id, out)` | Reads the date part of a date or datetime. |
| `clm_datetime_get_time(id, out)` | Reads the time part of a time or datetime. |
| `clm_datetime_get_datetime(id, out)` | Reads a complete datetime only. |
| `clm_datetime_get_kind(id, out)` | Identifies the stored kind. |
| `clm_datetime_delete(id)` | Deletes a supported date/time resource without deleting unrelated block types. |

Direct-value validation is available through `clm_datetime_date_is_valid`, `clm_datetime_time_is_valid`, and `clm_datetime_is_valid`.

A CLM resource ID is a **handle for the stored value**, not a Unix timestamp, clock time, or serialized datetime. ID allocation and synchronization remain subject to the underlying storage contract.

## 5. Parsing, formatting, and current values

| Operation | Behavior |
|---|---|
| `clm_datetime_parse_date(id, text)` | Parses strict date text and stores a date. |
| `clm_datetime_parse_time(id, text)` | Parses strict time text and stores a time. |
| `clm_datetime_parse_datetime(id, text)` | Parses strict datetime text and stores a datetime. |
| `clm_datetime_parse_*_string(id, string_id)` | Parses the corresponding kind from a CLM string resource. |
| `clm_datetime_format_date_to(value, out, capacity)` | Formats a directly supplied date value. |
| `clm_datetime_format_time_to(value, out, capacity)` | Formats a directly supplied time value. |
| `clm_datetime_format_datetime_to(value, out, capacity)` | Formats a directly supplied datetime value. |
| `clm_datetime_format_to(id, out, capacity)` | Formats any stored supported kind. |
| `clm_datetime_format_string(id, string_id)` | Writes normalized text to a CLM string resource. |
| `clm_datetime_current_date(id)` | Stores the current local calendar date. |
| `clm_datetime_current_time(id)` | Stores the current local clock time, milliseconds, and UTC offset. |
| `clm_datetime_current_datetime(id)` | Stores local date and time with the current UTC offset from one clock reading. |

The **current** operations use the host system clock. Their results reflect the local offset **at the moment of capture**; retaining a fixed-offset datetime does not cause the module to track later daylight-saving or timezone changes.

## 6. Calendar queries and comparisons

| Operation | Behavior |
|---|---|
| `clm_datetime_is_leap_year(id, out)` | Reports Gregorian leap-year status for a date or datetime. |
| `clm_datetime_days_in_month(year, month, out)` | Returns 28–31 for a valid month and year. |
| `clm_datetime_weekday(id, out)` | Returns **Monday = 1** through **Sunday = 7**. |
| `clm_datetime_day_of_year(id, out)` | Returns **1–365** or **1–366**. |
| `clm_datetime_compare(left_id, right_id, out)` | Compares compatible values, returning −1, 0, or 1. |
| `clm_datetime_difference_milliseconds(left_id, right_id, out)` | Computes left minus right in milliseconds for compatible values. |

**Comparison compatibility rules:**

- Both values must be of the same stored kind (`DATE`, `TIME`, or `DATETIME`).
- For times and datetimes, both must either **have an offset** or **lack an offset**; mixing aware and naive values is an error.
- Dates compare calendar days. The beginning of a date is treated as midnight when calculating a millisecond difference.
- Offset-aware datetimes compare the **same underlying UTC instant**, even when their written local times or offsets differ.
- Offset-naive datetimes compare their **wall-clock fields**, without assuming a timezone.
- Time-only values compare their clock milliseconds, adjusting for a present fixed offset relative to a common nominal day. A time without a date cannot express the date on which an instant occurred.

The calendar follows Gregorian rules across the supported year range; the implementation does not introduce regional calendar-reform cutovers.

## 7. Arithmetic and Unix conversion

| Operation | Behavior |
|---|---|
| `clm_datetime_add_days(id, days)` | Adds signed calendar days to a date or datetime while retaining time and offset. |
| `clm_datetime_add_milliseconds(id, milliseconds)` | Adds signed milliseconds to a time or datetime; time-only values wrap within one day, while datetimes carry across dates. |
| `clm_datetime_to_unix_milliseconds(id, out)` | Converts an **offset-aware datetime** to signed milliseconds since the Unix epoch. |
| `clm_datetime_from_unix_milliseconds(id, milliseconds, offset_minutes)` | Creates an aware datetime at the given instant, expressed using a specified fixed offset. |
| `clm_datetime_change_offset(id, offset_minutes)` | Changes an aware time or datetime's displayed offset while preserving the represented UTC-relative value. |

The Unix epoch is **1970-01-01T00:00:00Z**. Negative Unix-millisecond values represent times before that instant. An offset-naive datetime cannot be converted to an unambiguous Unix timestamp without first providing an offset.

Day addition and millisecond addition use fixed civil calendar and clock arithmetic, **not timezone-aware scheduling**. A caller needing the result of adding a day across a named timezone's daylight-saving transition must use an appropriate timezone facility in addition to this module.

Operations that exceed the supported date range report **out of range** instead of wrapping the calendar. Fixed offset changes may alter the visible date when conversion crosses midnight.

## 8. Results, errors, and ownership

The current reference implementation distinguishes the following results. Language ports may expose idiomatic exceptions or result types as long as they preserve these distinctions.

| Result | Meaning |
|---|---|
| `SUCCESS` | Operation completed. |
| `FAILURE` | System clock, allocation, or another operation failed. |
| `INVALID_ARGUMENT` | Invalid date/time fields, text, arguments, or incompatible offset awareness. |
| `NOT_FOUND` | Source resource ID is absent. |
| `TYPE_MISMATCH` | Resource has a different stored kind or unrelated BPA type. |
| `OUT_OF_RANGE` | Arithmetic or conversion falls outside the supported calendar range. |
| `BUFFER_TOO_SMALL` | Output buffer cannot hold the complete normalized text. |

Invalid input and unsuccessful updates must leave existing values and caller-owned outputs unchanged. The caller owns direct value objects and supplied output buffers. Registered resources are managed by the storage module until explicit deletion or shutdown.

The reference implementation shares the BPA resource registry. Callers must synchronize concurrent access; a numeric ID does not make the underlying object thread-safe.

## 9. Illustrative usage

Language-neutral pseudocode; specific syntax varies by language:

```text
datetime.parse_date(BIRTHDAY_ID, "2024-02-29")
datetime.is_leap_year(BIRTHDAY_ID)                       # true
datetime.weekday(BIRTHDAY_ID)                            # 1=Monday ... 7=Sunday

datetime.parse_datetime(EVENT_ID, "2026-11-02T09:30:00.250+08:00")
datetime.to_unix_milliseconds(EVENT_ID)
datetime.change_offset(EVENT_ID, 0)
datetime.format(EVENT_ID)                                # "2026-11-02T01:30:00.250Z"

datetime.add_days(EVENT_ID, 1)
datetime.add_milliseconds(EVENT_ID, 1500)
datetime.format(EVENT_ID)
```

The strings illustrate the fixed-offset semantics and are not a prescription for how individual language bindings must be named.

## 10. Boundaries and portability

- **Date range:** Gregorian years 0001–9999; no leap-second or 24:00 representation.
- **Precision:** milliseconds; no nanosecond or arbitrary-precision fractional seconds.
- **Offsets:** fixed numeric offsets only, not timezone identifiers, DST policies, or historical timezone databases.
- **Awareness:** offset-absent values remain distinct from UTC-aware values.
- **Clock:** current-value operations use the platform's wall clock, not a monotonic timer.
- **Storage:** typed managed values and procedural operations, not a date scheduler, recurrence engine, or persistence database.
- **Consumers:** `clm_console` and other modules may format date/time resources but are **not dependencies** of `clm_datetime`.

These constraints keep `clm_datetime` portable and composable while leaving timezone databases and scheduling policy to specialized modules or applications.
