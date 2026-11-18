# clm_re

> Language-neutral module overview and behavioral contract.

## Purpose

Portable reusable byte-oriented regular expressions for CLM. Compile patterns once, store independent instructions in BPA STRUCT blocks, and search or fully match many inputs by referring to the compiled block ID. No pointer into the source pattern is retained after compilation. The engine uses an ordered, non-backtracking NFA virtual machine with bounded compiler structures and an explicit per-call VM instruction budget. Time and memory usage still depend on pattern and input complexity.

## Behavior and constraints

Portable reusable byte-oriented regular expressions for CLM.
Compile patterns once, store independent instructions in BPA STRUCT blocks,
and search or fully match many inputs by referring to the compiled block ID.
No pointer into the source pattern is retained after compilation.
The engine uses an ordered, non-backtracking NFA virtual machine with
bounded compiler structures and an explicit per-call VM instruction budget.
Time and memory usage still depend on pattern and input complexity.

Syntax: literals, ., ^, $, [...], [^...], ranges, concatenation, |,
(...), (?:...), *, +, ?, {n}, {n,}, {n,m}, and lazy quantifiers.
Escapes: \d \D \w \W \s \S \b \B \n \r \t \f \v \xHH and escaped
punctuation. Classes accept shorthand escapes and \b means backspace there.
ASCII case folding and character categories only; UTF-8 is matched as bytes.
In particular, dot consumes ONE byte, not one UTF-8 code point, and all
reported offsets are byte offsets. A match may begin or end inside a
multi-byte UTF-8 sequence; this module does not validate UTF-8 boundaries.
No backreferences, lookaround, named groups, or inline flags. This is a
deliberately restricted syntax, not a Perl/PCRE/Python compatibility mode.
Subtle empty-match/repetition priorities may differ from other engines.

Matches are leftmost-first, with ordered alternation and greedy repetition
by default. Group 0 is the whole match; captures are numbered by opening '('.
Unmatched captures have SIZE_MAX offsets. A repeated capture retains its last
participating value; a participating empty group has equal start/end offsets.
Empty repetition cycles are stopped at the first repeated VM state. Offsets
are absolute byte offsets, with exclusive ends. Outputs are cleared on every
non-success result.

IDs share BPA's block namespace. Compilation never replaces an existing ID.
Source strings are borrowed only during the call; compiled expressions own
no external pointers. Delete with clm_re_delete or clm_bpa_delete; BPA
shutdown also releases expressions. Serialize access as required by BPA.
Never modify compiled BPA payloads directly. Compiled blocks are runtime
artifacts, not a stable serialized/on-disk format. The caller must not
delete, replace, or mutate the compiled BPA block during a match.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_re. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_re when an application needs: Portable reusable byte-oriented regular expressions for CLM. Compile patterns once, store independent instructions in BPA STRUCT blocks, and search or fully match many inputs by referring to the compiled block ID. No pointer into the source pattern is retained after compilation. The engine uses an ordered, non-backtracking NFA virtual machine with bounded compiler structures and an explicit per-call VM instruction budget. Time and memory usage still depend on pattern and input complexity.

A conforming workflow is conceptually:

1. Configure or acquire the module's inputs.
2. Perform an operation defined by this module and inspect its outcome.
3. Observe ownership, lifetime, ordering, and concurrency rules while using
   dependent CLM resources.
4. Release or reset owned state through the documented lifecycle operation.

## Language-neutral boundaries

Implementations may choose native types, allocation, error reporting, callbacks,
threads, and platform integrations. They must preserve the documented data
meaning, distinguishable outcomes, lifetime rules, synchronization contract,
and dependency relationships. Implementation availability and platform
support must be reported rather than silently substituted.
<!-- CLM-DOC-NEUTRAL-SCOPE:END -->
