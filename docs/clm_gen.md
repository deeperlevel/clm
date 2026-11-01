# CLM Gen

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

General-purpose generation of random values, identifiers and textual tokens.

## Capabilities and operations

- Generate integers and numeric ranges, hexadecimal text, UUID-like identifiers and alphanumeric/password-style strings.
- Use a cryptographic source only for operations specifically documented as security-appropriate.
- Distinguish generated randomness from deterministic pseudorandom simulation sequences.
- Define output format, alphabet, length and uniqueness checks in the caller’s contract.

## Detailed module contract

**Module:** `clm_gen`  
**Purpose:** Generate random numeric values, UUIDs, hexadecimal and alphanumeric strings, and strong-password-style strings using consistent CLM operations.

## Overview

`clm_gen` provides random generation without requiring applications to own platform-specific random-byte acquisition. Numeric operations return numbers directly. Text operations support **BPA destinations** and **caller-owned buffers** through `_to` variants.

The current implementation obtains random bytes from the Windows system RNG (`BCryptGenRandom`) on Windows or `/dev/urandom` on other supported targets. Consequently, portability beyond environments supporting these mechanisms needs a suitable backend. Most numeric and string selection is derived from these random bytes.

## Return values and ownership

| Kind | Contract |
| --- | --- |
| Numeric generators | Return a generated value directly, not a status code. |
| Text generators | Return `GENERATION_SUCCESS` (`0`) or `GENERATION_FAILURE` (`1`). |
| BPA destination | Caller supplies the **ID of an existing BPA string block**; generator replaces its string on success. |
| `_to` destination | Caller supplies writable memory and its capacity; generated text is written there, with a terminating zero byte on success. No BPA block is needed. |

A BPA destination is **not automatically created** by these functions. Validate it as an existing string block before generation.

For a requested text length of **N characters**, provide capacity for **N + 1 bytes** (including terminator). UUID text always needs **37 bytes**: 36 characters plus terminator. The specified length measures generated ASCII characters, which correspond to bytes in these generators.

## Numeric generation

All integer generators take inclusive lower and upper bounds. Reversed bounds are swapped, and equal bounds yield that bound.

| Operation | Result type | Range |
| --- | --- | --- |
| `clm_gen_random_i8(min, max)` | Signed 8-bit | Inclusive |
| `clm_gen_random_u8(min, max)` | Unsigned 8-bit | Inclusive |
| `clm_gen_random_i16(min, max)` | Signed 16-bit | Inclusive |
| `clm_gen_random_u16(min, max)` | Unsigned 16-bit | Inclusive |
| `clm_gen_random_i32(min, max)` | Signed 32-bit | Inclusive |
| `clm_gen_random_u32(min, max)` | Unsigned 32-bit | Inclusive |
| `clm_gen_random_i64(min, max)` | Signed 64-bit | Inclusive |
| `clm_gen_random_u64(min, max)` | Unsigned 64-bit | Inclusive |
| `clm_gen_random_f32(min, max)` | 32-bit floating point | Bounded numeric interval |
| `clm_gen_random_f64(min, max)` | 64-bit floating point | Bounded numeric interval |

The current integer implementation samples uniformly using bounded rejection sampling. Its random-byte failure path ultimately terminates the process for the unsigned 64-bit generator rather than returning a recoverable status; callers must not assume numeric generation always reports failures gracefully.

For floating-point generation, non-finite input bounds yield NaN. Reversed bounds are swapped; equal bounds return that value. The current implementation constructs a unit value from random bits and scales it into the requested interval, clamping the resulting value to the bounds. It does **not** guarantee every representable floating-point number is equally likely, and it should not be interpreted as a mathematical continuous distribution.

## Text generation

| BPA operation | Direct-buffer operation | Produces |
| --- | --- | --- |
| `clm_gen_hex(length, casing, destination_id)` | `clm_gen_hex_to(length, casing, buffer, capacity)` | Hex digits `0–9`, `a–f`, `A–F` |
| `clm_gen_uuid(casing, destination_id)` | `clm_gen_uuid_to(casing, buffer, capacity)` | Version 4 UUID string |
| `clm_gen_alphanum(length, casing, destination_id)` | `clm_gen_alphanum_to(length, casing, buffer, capacity)` | Letters and digits |
| `clm_gen_strong_password(length, destination_id)` | `clm_gen_strong_password_to(length, buffer, capacity)` | Password with multiple character classes |

### Casing options

| Identifier | Meaning |
| --- | --- |
| `GENERATION_CASING_MIXED` | Mixed case where letters are generated; exact upper/lower distribution depends on the generator. |
| `GENERATION_CASING_UPPER` | Uppercase letters and digits. |
| `GENERATION_CASING_LOWER` | Lowercase letters and digits. |

Casing must be one of these three values. Invalid casing returns failure for text generators that accept casing.

- **Hex:** `UPPER` selects uppercase hex letters; `LOWER` selects lowercase; `MIXED` independently chooses a case for each hex letter (digits remain unchanged).
- **Alphanumeric:** `MIXED` selects characters from uppercase letters, lowercase letters, and digits. It does not guarantee both cases appear.
- **UUID:** Case applies to hex letters; hyphens and digits are unaffected.
- **Password:** No casing option; both upper- and lowercase groups are guaranteed if generation succeeds.

### UUID

`clm_gen_uuid` and `clm_gen_uuid_to` produce a **version 4**, variant-compatible UUID in canonical five-group text form (`8-4-4-4-12` hex digits, separated by hyphens). The implementation sets version and variant bits in 16 random bytes before text encoding. For `MIXED`, hex-letter casing is chosen per character.

### Strong passwords

`clm_gen_strong_password` and `_to` require **length >= 4** and generate ASCII characters from uppercase letters, lowercase letters, digits, and a fixed symbol alphabet. They choose at least one from each class and then shuffle all positions, so the required classes do not occupy predictable fixed positions.

The algorithm ensures **character-class diversity**, not a site's password-policy compatibility or a guaranteed strength rating. Symbols might be rejected by some services. Never log or persist plaintext passwords unnecessarily.

## Conceptual examples

**Generate a token in an existing BPA block:**

1. Create or locate a BPA string block identified by `TOKEN_ID`.
2. Invoke `clm_gen_alphanum(32, GENERATION_CASING_MIXED, TOKEN_ID)`.
3. Check for `GENERATION_SUCCESS`.
4. Retrieve the text from BPA using `TOKEN_ID`.

**Generate directly into application-owned memory:**

1. Reserve 37 writable bytes.
2. Invoke `clm_gen_uuid_to(GENERATION_CASING_LOWER, buffer, 37)`.
3. Check for `GENERATION_SUCCESS` before using the buffer as a UUID string.

**Generate a bounded random integer:**

1. Invoke `clm_gen_random_i32(1, 6)`.
2. Use the returned number in the inclusive range `1..6`.

## Validation and safety notes

- **Buffer size:** Insufficient capacity or a null destination fails in `_to` operations. Successful output is zero-terminated.
- **Failure state:** A random-source failure can occur after an `_to` operation has partially written the buffer. **Do not use buffer contents after failure.** For sensitive values, clear the buffer using a suitable secure erasure method.
- **BPA:** A missing or wrong-type destination block fails. BPA-backed generators may use temporary allocation internally.
- **Lengths:** Negative text lengths are invalid. An empty hex or alphanumeric string is permitted by the current length checks; strong passwords require at least four characters. Very large lengths are constrained by buffer capacity, allocation size, and the implementation's length types.
- **Portability:** The present non-Windows random-source backend assumes `/dev/urandom` is available and accessible; environments without it are not covered by that implementation.
- **Integer edge cases:** The provided version's signed 64-bit conversion warrants portability review around the full signed range. Do not treat this documentation as confirmation of exhaustive edge-case testing.
- **Cryptographic usage:** Random-byte acquisition uses system cryptographic RNG facilities where supported, but the whole module has not been established here as a formally audited cryptographic API. Evaluate specific uses accordingly.

## Design boundaries

`clm_gen` generates values; it does not manage BPA block lifetimes, securely store secrets, enforce application password policies, or supply a seeded/reproducible simulation RNG. It also does not provide a general failure-reporting interface for numeric generation in the current API.

## Related modules

- **`clm_base`:** Numeric types and their ranges.
- **BPA:** Storage for generated strings in ID-addressed blocks.
- **`clm_platform`:** Compile-time target description; not itself a runtime random-source provider.

---

**CLM — Cross Language Modules** · [github.com/deeperlevel/clm](https://github.com/deeperlevel/clm)  
**License:** CC0 1.0 Universal · Public Domain · Provided AS IS.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md).

## Example use cases

Generate integers and numeric ranges, hexadecimal text, UUID-like identifiers and alphanumeric/password-style strings.
