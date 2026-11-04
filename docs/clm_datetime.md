# CLM Datetime

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Handles validated calendar dates, clock times and combined datetime values under CLM resource identifiers, plus conversion and comparison utilities.

## Capabilities and operations

- Validate calendar dates, clock times and combined representations.
- Set, query, delete and copy values addressed by IDs.
- Parse and format standardized textual date/time forms, including CLM string resources.
- Read current system date/time; derive leap years, month lengths, weekdays and day of year.
- Compare values, compute millisecond differences and perform date/time arithmetic.

## Resource, data and ownership model

Date, time and datetime are distinct semantic value kinds. A date has calendar fields; a time has wall-clock fields; a combined datetime has both. The ID registry preserves value kind so misuse can be rejected instead of silently interpreting the wrong type.

## Behavioral contract and boundaries

Uses Gregorian years 0001–9999 and strict ISO-style calendar, wall-clock and combined date/time text. Time fields may carry a fixed UTC offset; a timezone database, named time zones or daylight-saving rules are not provided. Invalid calendar dates, leap seconds, 24:00, malformed offsets, unsupported formats and arithmetic overflow must be distinguished. Storage retains the value kind (date, time or combined datetime), and operations must reject a mismatching kind.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Parse two datetimes, compare their order, compute an elapsed duration and format the result for display.

## Related CLM modules

[clm_base](clm_base.md).
