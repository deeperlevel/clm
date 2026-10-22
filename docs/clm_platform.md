# CLM Platform (`clm_platform`)

> **CLM — Cross Language Modules**  
> An initiative by Deeperlevel, developed with community contributions.  
> Repository: https://github.com/deeperlevel/clm  
> Contact: deeparchive@outlook.com  
> **Public Domain — CC0 1.0 Universal**: https://creativecommons.org/publicdomain/zero/1.0/  
> Free to use, modify, redistribute, and bundle for any purpose. Attribution and retention of this notice are appreciated, not required.  
> Provided **AS IS**, without warranty or liability.  
> *Don't start from scratch, bro.*

## 1. Overview

`clm_platform` provides small, procedural operations for identifying the **current runtime's operating-system family**, estimating its **device category**, and reading **process environment variables**. It is designed to be reimplemented with native facilities in different programming languages while keeping the same names, concepts, and return values.

The module does **not** measure CPU load, available RAM, process memory, frame rate, device temperature, or other performance metrics. Such functionality belongs in a separate `clm_performance` module.

No class instances, initialization call, service registry, or dependency-injection framework are required.

## 2. Public API

The following signatures are **language-neutral contracts**, not syntax requirements for any particular language.

| Function | Arguments | Return | Purpose |
|---|---|---|---|
| `clm_platform_get_os_name` | none | String | Obtain recognizable OS family name |
| `clm_platform_get_os_version` | none | String | Obtain best available OS-reported version |
| `clm_platform_is_mobile` | none | Boolean | Identify recognized mobile runtime |
| `clm_platform_is_console` | none | Boolean | Best-effort game-console identification |
| `clm_platform_is_pc` | none | Boolean | Identify recognized desktop-class OS |
| `clm_platform_is_linux` | none | Boolean | Identify generic Linux; Android excluded |
| `clm_platform_is_windows` | none | Boolean | Identify Windows |
| `clm_platform_get_environment_variable` | name: String, default: Optional String | Optional String | Read one process environment variable |
| `clm_platform_get_environment_variables` | none | Collection of String → String | Read a detached environment snapshot |

Implementations may use functions, procedures, free functions, or equivalent native constructs. Optional convenience facades are permitted but are not part of the required contract.

## 3. Operating-system identification

### `clm_platform_get_os_name()`

Returns a recognizable OS family name or `Unknown` if detection cannot determine a known family.

The initial Python implementation recognizes:

| Return value | Meaning |
|---|---|
| `Windows` | Microsoft Windows runtime |
| `macOS` | Apple macOS runtime |
| `Linux` | Generic Linux runtime, excluding positively identified Android |
| `Android` | Android runtime |
| `iOS` | iOS runtime, including platforms identified as iPadOS |
| `FreeBSD` | FreeBSD runtime |
| `OpenBSD` | OpenBSD runtime |
| `NetBSD` | NetBSD runtime |
| `Unknown` | Unidentified OS family |

The recognized names form the **current portable vocabulary**. Implementations on additional systems may initially return `Unknown` until a consistent new name is agreed upon. A target may be virtualized or containerized; this identifies the current runtime environment, not necessarily the underlying physical host.

### `clm_platform_get_os_version()`

Returns an OS-reported version string. When no usable value exists, returns `Unknown`.

Important limitations:

- A version string is **not guaranteed** to match a marketing version such as “Windows 11” or a Linux distribution release.
- The host API may report a kernel version, build number, or implementation-specific system version.
- Android's Python implementation prefers the available Android API level. This is an API level, **not** an Android marketing-version number.
- Consumers must not compare arbitrary version strings lexicographically to establish OS capabilities.

The return is a descriptive value, **not** a stable compatibility or security check.

## 4. Device-category indicators

The category functions are independent Boolean queries. They are **best-effort classifications**, not proof of physical hardware type.

### `clm_platform_is_mobile()`

Returns `true` for a runtime positively identified as `Android` or `iOS` in the initial implementation. Otherwise returns `false`.

This does not guarantee a phone form factor. Android tablets and iPads also belong to their respective mobile operating-system families.

### `clm_platform_is_console()`

Returns `true` only when a port can positively recognize a supported console runtime.

**Initial Python limitation:** standard Python does not provide reliable, portable identification of game consoles; its implementation currently returns `false`. A `false` value therefore means **not positively identified as a console**, not guaranteed absence of console hardware. Platform-specific ports may provide stronger recognition.

Avoid guessing from environment variables, user-agent strings, or arbitrary hardware characteristics when certainty is required.

### `clm_platform_is_pc()`

Returns `true` for recognized desktop-class OS families (`Windows`, `macOS`, `Linux`, `FreeBSD`, `OpenBSD`, and `NetBSD`) and `false` otherwise.

This function describes the runtime's conventional OS class, **not** verified laptop/desktop physical hardware. Linux-based appliances or embedded devices may be classified as PC by this lightweight rule.

### `clm_platform_is_linux()` and `clm_platform_is_windows()`

Return `true` when the detected OS family is exactly `Linux` or `Windows`, respectively.

Android is not reported as generic Linux by `clm_platform_is_linux()`, even though Android uses a Linux kernel. This separation helps applications select platform-specific behavior.

### Classification examples

| Identified OS | Mobile | PC | Linux | Windows |
|---|---|---|---|---|
| Windows | false | true | false | true |
| macOS | false | true | false | false |
| Linux | false | true | true | false |
| Android | true | false | false | false |
| iOS | true | false | false | false |
| FreeBSD / OpenBSD / NetBSD | false | true | false | false |
| Unknown | false | false | false | false |

Console status is intentionally omitted from the table: its detection depends on the runtime/port, and the reference Python version currently has no positive detector.

## 5. Environment variables

### `clm_platform_get_environment_variable(name, default = null)`

Reads `name` from the **current process environment**.

- If the variable exists, return its String value, **including an empty string**.
- If the variable is missing, return `default` (`null`/no-value when omitted).
- Variable names must be nonempty Strings and must not contain the NUL character or `=`.
- The optional default, when present, is a String.
- Invalid arguments produce an argument error via the target language's normal error mechanism.

**Portable example:**

```text
mode = clm_platform_get_environment_variable("APP_MODE", "development")

if mode == "production":
    enable_production_configuration()
```

Process-environment names and values remain governed by native OS behavior, including platform-specific name case sensitivity and encoding.

### `clm_platform_get_environment_variables()`

Returns a **detached snapshot** of the current process environment as a collection of String name/value pairs. Modifying the returned collection must not modify the live process environment.

**Security note:** environment variables may contain credentials, tokens, and other secrets. Do not indiscriminately log, expose, or send the entire snapshot to clients.

Environment **mutation** (`set`, `remove`, or process spawning) is not included in this first module.

## 6. Lifecycle and resource management

- All public operations are independently callable and require no initialization or cleanup.
- OS identification is read-only.
- Environment reads do not create or modify environment variables.
- The environment snapshot is a caller-owned return value, with ownership expressed using each language's native conventions.
- Implementations may rely on native standard libraries or platform APIs. The contract does not mandate a particular backing library.

## 7. Error handling

The initial contract uses normal errors for invalid inputs rather than introducing a shared status enumeration:

| Situation | Required behavior |
|---|---|
| OS cannot be recognized | `Unknown` |
| OS version is unavailable | `Unknown` |
| Console cannot be positively identified | `false` |
| Environment variable absent | Specified default, or null/no-value |
| Environment variable present but empty | Empty String |
| Environment variable name invalid | Argument error |
| Default value invalid | Argument/type error |

Native access restrictions and platform failures may be handled according to each language's standard error model. Ports should document any deviations.

## 8. Cross-language implementation guidance

Use simple native APIs in each language. For example:

- System identification may use operating-system APIs, runtime metadata, or system calls.
- Process environment access may use each language's standard environment library.
- Avoid mandatory object models, reflection, registries, or CLM Container dependencies for this module.
- Normalize recognized OS **family names** to the spelling in Section 3.
- Keep device classification conservative where data is incomplete.
- On consoles or other specially supported targets, implement positive detection only when trustworthy target-specific evidence exists.

**Implementation-specific consideration:** some runtimes (including mobile Python distributions) may report a host-like OS family rather than a distinct Android/iOS target. That can lead to `Unknown` or a desktop-class classification. Do not claim device identity is infallible.

## 9. Relationship to other CLM modules

| Module | Responsibility |
|---|---|
| `clm_platform` | OS family, OS version, coarse device classification, process environment |
| `clm_ua` | Parse potentially spoofed HTTP User-Agent strings from remote clients |
| `clm_path` | Filesystem-path manipulation and native path conventions |
| `clm_performance` (planned) | CPU, memory, timing, and other performance metrics |

`clm_platform` describes the **local running application**, whereas `clm_ua` describes a **remote client as claimed by an HTTP header**. The two should not be conflated.

## 10. Compatibility checklist

An implementation conforms to this version when it:

- [ ] Provides all nine procedural API operations.
- [ ] Returns OS-family names using the documented vocabulary.
- [ ] Provides `Unknown` rather than inventing a version or OS family.
- [ ] Distinguishes recognized Android from generic Linux.
- [ ] Uses the documented mobile/PC/Windows/Linux classification rules.
- [ ] Treats console detection as best-effort and does not claim that `false` disproves console hardware.
- [ ] Distinguishes absent environment variables from present-but-empty variables.
- [ ] Supports the optional default for environment-variable lookups.
- [ ] Returns an independent environment snapshot.
- [ ] Performs no performance monitoring and requires no CLM dependency infrastructure.

---

**CLM — Different languages. Shared technologies. No borders.**  
*Don't start from scratch, bro.*
