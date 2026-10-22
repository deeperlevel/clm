# CLM UA — User-Agent Parser

**Module:** `clm_ua`  
**Project:** CLM — Cross Language Modules  
**Official repository:** https://github.com/deeperlevel/clm  
**License:** CC0 1.0 Universal  
**Design:** Procedural-first, language-agnostic, dependency-free heuristic parsing

> Different languages. Shared technologies. No borders.  
> Don't start from scratch, bro.

## Overview

`clm_ua` examines a raw HTTP **User-Agent** string and produces a reusable record describing the browser, operating system, device category, and whether the string resembles a web crawler. The module provides both a complete parse operation and focused helper operations.

Identification is **heuristic**, not authoritative. User-Agent strings may be absent, incomplete, misleading, modified, or spoofed. This module is intended for request logging, diagnostics, and coarse usage analytics; it must **not** be used as an authentication, authorization, bot-verification, or security decision mechanism.

The shared contract does not prescribe regular expressions, data classes, classes, pointers, or a particular string implementation. Implementations can use language-native string scanning or pattern matching, provided their observable behavior follows the documented rules.

## Data representation

### `UserAgent`

The parse operation produces one result record with the following fields:

| Field | Logical type | Default | Description |
|---|---|---|---|
| `raw` | String | `""` | Original input, preserved unmodified; absent input becomes an empty string. |
| `browser` | String | `"Unknown"` | Detected browser family. |
| `browser_version` | String | `"Unknown"` | Version extracted from the recognized browser token. |
| `os` | String | `"Unknown"` | Detected operating-system family. |
| `os_version` | String | `"Unknown"` | Extracted OS version, when available. |
| `device_type` | String | `"Desktop"` | One of `"Desktop"`, `"Mobile"`, or `"Tablet"`. |
| `is_mobile` | Boolean | `false` | True exactly when `device_type` is `"Mobile"`. |
| `is_tablet` | Boolean | `false` | True exactly when `device_type` is `"Tablet"`. |
| `is_desktop` | Boolean | `true` | True exactly when `device_type` is `"Desktop"`. |
| `is_bot` | Boolean | `false` | Whether a known crawler-related substring is present. |

**Invariants:** Exactly one of `is_mobile`, `is_tablet`, and `is_desktop` is true. `device_type` and those three flags must agree. A recognized OS or browser can have an unknown version when no version is available.

The supplied implementation exposes a read-only result record. Other languages may represent it using a value type, immutable record, structure, or other native mechanism.

## Public API

The examples use **language-neutral pseudocode**. Type and return syntax may vary in individual ports.

| Operation | Input | Result | Purpose |
|---|---|---|---|
| `clm_ua_parse(ua_string)` | String or absent input | `UserAgent` | Produce the complete parsed record. |
| `clm_ua_browser(raw)` | String or absent input | `(browser_name, version)` | Identify browser and version. |
| `clm_ua_os(raw)` | String or absent input | `(os_name, version)` | Identify OS and version. |
| `clm_ua_device_type(raw)` | String or absent input | Device category string | Estimate device category. |
| `clm_ua_is_bot(raw)` | String or absent input | Boolean | Detect crawler-like tokens. |

### `clm_ua_parse(ua_string)`

Parse the User-Agent and return a `UserAgent` record. This operation combines the browser, OS, device, and bot helpers. Missing input is treated as the empty string; the supplied reference rejects values that are neither a string nor absent. Implementations should report invalid input using their native error conventions.

```text
ua = clm_ua_parse(user_agent_header)

log(ua.browser)
log(ua.browser_version)
log(ua.os)
log(ua.device_type)
log(ua.is_bot)
```

### `clm_ua_browser(raw)`

Return a **browser name and version** pair. The detection order is significant because many browsers include tokens identifying their underlying engine or compatible browsers.

| Priority | Browser name | Recognized token(s) | Version source |
|---:|---|---|---|
| 1 | `Edge` | `Edg/`, `Edge/`, `EdgA/`, `EdgiOS/` | Digits and periods after the token |
| 2 | `Opera` | `OPR/`, `Opera/` | Digits and periods after the token |
| 3 | `Samsung Internet` | `SamsungBrowser/` | Digits and periods after the token |
| 4 | `Firefox` | `Firefox/`, `FxiOS/` | Digits and periods after the token |
| 5 | `Chrome` | `Chrome/`, `CriOS/` | Digits and periods after the token |
| 6 | `Safari` | `Version/<version>` followed later by `Safari/` | Version from `Version/` |

Token matching is case-insensitive. **The first matching rule wins**. If nothing matches, return `("Unknown", "Unknown")`.

Browser names are category labels, not guarantees that a corresponding browser is actually in use.

### `clm_ua_os(raw)`

Return an **operating-system name and version** pair, using the following precedence:

| Priority | OS name | Recognized pattern | Version behavior |
|---:|---|---|---|
| 1 | `Windows` | `Windows NT <version>` | Preserve numeric dot-separated version |
| 2 | `iOS` | `iPhone OS <version>` or `CPU OS <version>` / `CPU iPhone OS <version>` | Replace underscores with periods |
| 3 | `Android` | `Android <version>` or `Android/<version>` | Preserve numeric dot-separated version |
| 4 | `macOS` | `Mac OS X <version>` | Replace underscores with periods |
| 5 | `Linux` | `Linux` | Version is `"Unknown"` |

If no rule matches, return `("Unknown", "Unknown")`.

OS identification uses the tokens actually present; it does not infer modern marketing names from legacy version numbers. For instance, the reference reports the `Windows NT` version string rather than converting it to a Windows release name.

### `clm_ua_device_type(raw)`

Classify using case-insensitive token searches in the following order:

1. Return `"Tablet"` if the input contains `ipad`, `tablet`, `kindle`, or `silk/`.
2. Otherwise, if it contains `android`, return `"Mobile"` when it also contains `mobile`; return `"Tablet"` otherwise.
3. Otherwise, return `"Mobile"` if it contains `mobi`, `iphone`, or `ipod`.
4. Otherwise, return `"Desktop"`.

`"Desktop"` is the **default category**, not proof of an actual desktop device. Certain tablets that identify themselves using desktop-like User-Agent strings will be classified as desktop.

### `clm_ua_is_bot(raw)`

Return true if the case-insensitive input contains **any** of these substrings:

```text
bot
crawler
spider
slurp
bingpreview
```

Otherwise return false. This intentionally includes substring matches and can produce false positives or miss automated clients that do not identify themselves.

## Examples

### Typical parsing

```text
raw = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"
ua = clm_ua_parse(raw)

ua.browser          == "Chrome"
ua.browser_version  == "120.0.0.0"
ua.os               == "Windows"
ua.os_version       == "10.0"
ua.device_type      == "Desktop"
ua.is_desktop       == true
ua.is_bot           == false
```

### Android mobile versus tablet

```text
clm_ua_device_type("Android 14; Mobile") == "Mobile"
clm_ua_device_type("Android 14")         == "Tablet"
```

### Edge takes precedence over Chrome

```text
raw = "Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0"
clm_ua_browser(raw) == ("Edge", "120.0.0.0")
```

### Unknown and absent input

```text
clm_ua_browser("")    == ("Unknown", "Unknown")
clm_ua_os("")         == ("Unknown", "Unknown")
clm_ua_device_type("") == "Desktop"
clm_ua_is_bot("")     == false

clm_ua_parse(absent).raw == ""
```

## Behavior and portability requirements

**Case-insensitive matching.** Browser, operating-system, device, and bot rules are matched without regard to letter case. A port should adopt a consistent approach suitable for the ASCII tokens in these rules.

**Rule precedence.** Preserve browser and OS recognition order. For example, a User-Agent containing both `Edg/` and `Chrome/` must report `Edge`.

**Version extraction.** Browser versions consist of consecutive ASCII digits and periods immediately following the recognized token. OS extraction follows the patterns described above. Do not silently add new normalization behavior while claiming equivalence with the reference implementation.

**No network or external lookup.** Parsing takes place locally on the provided string; no remote fingerprinting, browser database, or network request is required.

**No required object model.** The result can be a struct or record. Functions are the primary API; a language-specific facade may be offered without changing the contract.

**Stateless operations.** Parsing does not require a mutable global registry, initialization step, or persistent cache. Implementations may precompile detection patterns for efficiency.

**Ownership.** Each language implementation determines how returned strings and records are allocated, copied, or freed. Its documentation must make those ownership rules explicit when relevant.

## Limitations and non-goals

- This is **not** a comprehensive User-Agent grammar implementation or browser/device database.
- User-Agent information can be spoofed. Bot detection does **not** verify crawler identity.
- Browser and operating-system versions are copied from matching tokens; their authenticity and semantic meaning are not validated.
- The rule set is intentionally small and can misclassify newer or less common browsers and devices.
- The default `Desktop` classification also applies to unrecognized and empty strings.
- Client Hints and other HTTP headers are outside this module's contract.
- The result is not suitable for security policy enforcement, access control, fraud detection, or trusted device identification.

## Cross-language compatibility checklist

An implementation claiming equivalence with this specification should verify that:

- [ ] It exposes all five procedural functions.
- [ ] The `UserAgent` result exposes the listed fields and default values.
- [ ] Empty or absent input produces the documented defaults.
- [ ] Invalid non-string input is reported rather than misinterpreted.
- [ ] Edge, Opera, Samsung Internet, Firefox, Chrome, and Safari are detected in the documented order.
- [ ] Windows, iOS, Android, macOS, and Linux are detected in the documented order.
- [ ] Underscores in iOS/macOS version tokens are converted to periods.
- [ ] Browser/OS recognition is case-insensitive.
- [ ] Device classification follows tablet-first, Android, mobile, then desktop precedence.
- [ ] Exactly one device-category flag is true.
- [ ] Bot heuristics use the documented substring list.
- [ ] The implementation has no mandatory third-party parser dependency.
- [ ] Its returned data and memory ownership rules are documented.
- [ ] Automated tests include known, unknown, ambiguous, empty, and spoofed-looking User-Agent strings.

## Related modules

`clm_ua` is independent of `clm_container`, `clm_id`, `clm_depend`, and `clm_dispatch`. Applications such as SPIder can call `clm_ua_parse()` when an HTTP backend supplies a User-Agent header, then store or log the resulting record through whichever other modules they choose.

---

**CLM — Cross Language Modules**  
*Different languages. Shared technologies. No borders.*
