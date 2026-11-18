# CLM Regular Expressions (`clm_re`)

## Purpose and scope

`clm_re` is CLM's small, reusable **byte-oriented regular-expression engine**. It supports compiling a pattern once, referring to the compiled expression by its CLM ID, and searching or fully matching multiple pieces of text. It is designed for portable pattern matching and predictable resource bounds, **not** full PCRE, Perl, POSIX, Python, or Unicode-regex compatibility.

This document is **language-neutral**: operation names describe behavior rather than requiring any particular language's object model. The current C interface uses the `clm_re_*` names shown in the API mapping below.

### Core design principles

- **Compile once, reuse:** parsing and bytecode compilation occur when a pattern is registered. Subsequent match calls reuse the compiled program and allocate only temporary VM working state.
- **CLM resource identity:** each compiled expression owns a BPA STRUCT block under a caller-supplied `clm_id`. IDs are in the same block namespace as other BPA values; compiling never overwrites an existing ID.
- **Restricted, non-backtracking execution:** patterns are compiled to a small NFA-style instruction program. The VM advances sets of active states without recursive backtracking. A step budget limits the amount of work per match invocation.
- **Byte semantics:** every input unit is one byte. UTF-8 can be supplied as input, but the module does not decode Unicode scalar values, case-fold Unicode, or guarantee matches align to character boundaries.
- **Explicit ownership and lifetime:** the compiled program owns no pointers to the original pattern; source text is borrowed only for each synchronous match call. Deleting a compiled expression invalidates its ID.

## Dependencies

| Role | CLM module / facility | Why it is needed |
|---|---|---|
| **Direct** | `clm_base` (`base.h`) | Common integer types and `clm_id` identity. |
| **Direct** | `clm_bpa` (`bpa.h`, BPA implementation) | Registers, validates, retrieves, and deletes compiled STRUCT blocks; validates/borrows optional STRING input blocks. |
| **Transitive through BPA** | `clm_arena` | Memory-pool/arena implementation used by BPA. |
| **Transitive through BPA implementation** | `clm_datetime` | Used for BPA type support in the supplied CLM core revision; not invoked by regex matching itself. |
| **Standard runtime** | C-compatible allocation, string, Boolean, and formatting facilities | Temporary compiler/VM memory, pattern lengths, bounded diagnostics. |

**External regex libraries:** none. `clm_re` does not need PCRE2, POSIX regex, RE2, or another regex engine. It inherits normal build and platform requirements of whichever CLM BPA implementation you link.

### Dependency relationships

```text
clm_re
  |-- clm_base                [IDs and basic types]
  |-- clm_bpa                 [compiled-pattern ownership and STRING access]
  |     |-- clm_arena          [BPA storage dependency]
  |     `-- clm_datetime       [used by this revision of BPA]
  `-- standard C runtime     [scratch allocations / diagnostics]
```

For an application, build/link the regex implementation with the matching revision of CLM core. A single public header is not sufficient for execution; `re.c` and the actual BPA implementation must also be linked. No special regex initialization routine is introduced: use BPA under its established CLM lifecycle and synchronization rules.

## Pattern lifecycle

1. **Choose an unused CLM ID.** The destination ID must not already identify any BPA block.
2. **Compile a pattern.** Supply the pattern bytes and optional flags. On success, one immutable compiled program is stored in a BPA STRUCT block with that ID.
3. **Search or match any number of texts.** Pass the same compiled ID to successive calls. Results are returned into caller-owned fixed-size structures; the pattern does not need recompilation.
4. **Delete when finished.** Release the compiled ID using the regex-specific delete operation (which checks type/magic) or the BPA deletion operation when you knowingly own the block. BPA shutdown also releases registered expressions.

```text
pattern = "(fox|dog)+"
expression = compile(pattern, flags = none, id = chosen_unused_id)

first  = search(expression, "The fox runs", start_byte = 0)
second = search(expression, "dogdog!", start_byte = 0)
third  = full_match(expression, "foxdog")

delete(expression)
```

**Important:** the pattern is copied into compiled bytecode, not stored as a pointer to caller text. Compiled bytecode is an internal runtime structure, **not a stable serialized file format**. Do not write arbitrary bytes into a compiled BPA STRUCT or allow it to be mutated/deleted while a match is active.

## Available operations

| Language-neutral operation | C API | Semantics |
|---|---|---|
| Compile from terminated text | `clm_re_compile` | Stops at the first NUL byte. |
| Compile from exact bytes | `clm_re_compile_n` | Can represent NUL as an input pattern byte; accepts NULL only for zero length. |
| Compile from an existing CLM string | `clm_re_compile_string_id` | Borrows an existing BPA STRING; does not retain it. |
| Remove compiled program | `clm_re_delete` | Rejects missing IDs or unrelated BPA blocks. |
| Search CLM string | `clm_re_search` | Finds at/after a byte offset in a BPA STRING. |
| Search terminated text | `clm_re_search_cstr` | Finds at/after a byte offset in a NUL-terminated buffer. |
| Search exact bytes | `clm_re_search_n` | Finds at/after a byte offset in a length-delimited buffer. |
| Full-match CLM string | `clm_re_match` | Requires every byte of an existing BPA STRING to match. |
| Full-match terminated text | `clm_re_match_cstr` | Requires all bytes before the first NUL to match. |
| Full-match exact bytes | `clm_re_match_n` | Requires all provided bytes to match, including any embedded NUL. |

The `search` operations return the first match at or after the requested **absolute byte** offset, including eligible empty matches. Anchors refer to the original input, not a substring cut out at the search offset. The `match` operations require the *entire input* to match; they do not mean “match a prefix.”

## Supported pattern syntax

| Feature | Pattern example | Meaning |
|---|---|---|
| Literals | `fox` | Match those exact bytes. |
| Wildcard | `.` | Match one byte other than LF, unless DOT_ALL. |
| Character class | `[abc]`, `[a-z]` | Match any listed byte / ASCII range. |
| Negated class | `[^0-9]` | Match bytes outside the class. |
| Sequence | `ab` | Match `a`, then `b`. |
| Alternation | `cat|dog` | Try ordered branches. |
| Capturing group | `(cat)` | Store a byte span as group 1, etc. |
| Noncapturing group | `(?:cat)` | Group without allocating a capture number. |
| Optional | `a?` | Zero or one occurrence. |
| Repetition | `a*`, `a+` | Zero or more / one or more occurrences. |
| Bounded repetition | `a{3}`, `a{2,}`, `a{1,4}` | Explicit minimum and/or maximum. |
| Lazy repetition | `a*?`, `a+?`, `a??`, `a{1,4}?` | Prefer fewer repetitions when branch priorities permit. |
| Anchors | `^`, `$` | Start/end of input (or LF boundaries in MULTILINE). |
| Word boundaries | `\b`, `\B` | ASCII word/non-word transition (or its negation). |
| Shorthand sets | `\d`, `\D`, `\w`, `\W`, `\s`, `\S` | ASCII byte-oriented categories and complements. |
| Escapes | `\n`, `\r`, `\t`, `\f`, `\v`, `\xHH` | Control bytes or literal hexadecimal byte. |

Escaped punctuation is supported. Within a character class, `\b` denotes the backspace byte, not a word boundary. A few otherwise common extensions are deliberately **unsupported**: lookbehind/lookahead, backreferences, named captures, inline flag modifiers, and Unicode property matching.

Matching uses prioritized alternatives, with greedy quantifiers by default, but **do not assume byte-for-byte equivalence with other regex engines**. In particular, nested zero-length repetitions can resolve differently than Python/Perl-style engines. Test any patterns migrated from another implementation.

## Flags

- **IGNORE_CASE** (`CLM_RE_IGNORE_CASE`, bit 1): extend character matching across ASCII upper/lowercase equivalents. No Unicode folding is performed.
- **MULTILINE** (`CLM_RE_MULTILINE`, bit 2): `^` can match after an LF byte and `$` before an LF byte, in addition to input boundaries.
- **DOT_ALL** (`CLM_RE_DOT_ALL`, bit 4): dot matches LF, too.

Flags are passed when compiling and become properties of that compiled ID. Unsupported flag bits are rejected.

## Captures, byte positions, and return statuses

The result contains a `group_count` and a fixed-size `groups` sequence. Group **0** always represents the complete match. Numbered capture groups start at **1** in order of their opening parentheses. Every span has `start` and `end`, with **start inclusive, end exclusive**, measured in bytes relative to the original input. An unmatched capture has both endpoints set to the equivalent of `SIZE_MAX`; an empty participating capture has equal numeric endpoints, not sentinel values.

The result structure is owned by the caller and needs no special freeing. It is cleared on every unsuccessful execution. The compile API can optionally return an error record containing a status, a pattern byte offset, and a bounded diagnostic message.

| Status category | C constant | Meaning |
|---|---|---|
| Success | `CLM_RE_SUCCESS` (0) | A compilation or match succeeded. |
| No match | `CLM_RE_NO_MATCH` (+1) | Valid execution; no acceptable match. |
| Invalid arguments | `CLM_RE_INVALID_ARGUMENT` (-1) | Missing required pointers, invalid flags, or impossible offsets/length. |
| Pattern syntax | `CLM_RE_SYNTAX_ERROR` (-2) | Invalid or unsupported regex grammar. |
| Allocation failed | `CLM_RE_NO_MEMORY` (-3) | Scratch storage allocation failed. |
| Limit exceeded | `CLM_RE_LIMIT_EXCEEDED` (-4) | Pattern/node/VM work exceeded a documented cap. |
| ID already used | `CLM_RE_ID_EXISTS` (-5) | Would overwrite an existing BPA block. |
| Missing ID | `CLM_RE_NOT_FOUND` (-6) | The requested BPA value is absent. |
| Wrong BPA type | `CLM_RE_TYPE_MISMATCH` (-7) | BPA value is not an expected string or valid regex STRUCT. |

## Resource bounds and performance

| Resource | Current limit | Enforcement |
|---|---:|---|
| Source pattern size | 65,536 bytes | 8 × MAX_INSTRUCTIONS |
| Parser AST nodes | 8,192 | Reject compilation exceeding cap |
| Compiled VM instructions | 8,192 | Reject compilation exceeding cap |
| Capturing groups | 16 plus group 0 | Reject excess group declarations |
| Parenthesis nesting depth | 64 | Reject excess nesting |
| Bounded repetition count | 1,000 | Reject excessive explicit counts |
| VM instruction visits | 10,000,000 per call | Abort matching with LIMIT_EXCEEDED |

These limits provide **explicit failure behavior**, not a guarantee of tiny memory usage or fixed execution time. Each VM thread includes capture slots; temporary active-state storage scales with instruction count and the fixed capture capacity. Compiling a large pattern can also require sizable temporary parser arrays.

The VM's NFA approach avoids the exponential recursive backtracking characteristic of some other regex implementations. However, a search over long input still requires work across input bytes and active states. High-load services should enforce their own input-size limits, allocation budgets, and request deadlines in addition to `CLM_RE_MAX_STEPS`.

## Binary input and UTF-8

The engine interprets patterns and input as byte sequences. For the UTF-8 encoding of `é` (`C3 A9`):

```text
input bytes: C3 A9
pattern "."         -> matches byte C3; span [0,1)
pattern ".."        -> can match both bytes; span [0,2)
pattern "\\xA9"     -> can match the second byte at offset 1
```

That means capture boundaries may slice inside a UTF-8 character. Word boundaries and case-insensitive matching use **ASCII** classifications. `\w` does not classify non-ASCII letters as Unicode words. For Unicode-aware searching, use a dedicated Unicode-capable engine or preprocess text at the application level.

Length-delimited APIs can process embedded zero bytes; C-string APIs stop at the first zero byte.

## Ownership, lifetime, and threading

- The compiling caller selects and owns the destination BPA ID; a pattern is neither automatically replaced nor automatically evicted.
- All compiled instructions reside in one BPA STRUCT allocation; deleting that block releases the program.
- Input patterns and text are **borrowed** for the duration of the respective synchronous call only.
- Results and compiler errors are **caller-owned, fixed-size records**. Their fields do not point into the input.
- Match execution allocates temporary VM state; this state is not retained between calls.
- Follow the existing BPA concurrency contract: synchronize shared registry access, and never delete/replace/modify a compiled block or source BPA STRING while a match uses it.
- Consider `clm_re_delete` preferable to generic BPA deletion when you want type validation before releasing an ID.

## Usage guidance

- Compile known patterns once at application/service initialization, reuse them for many matches, and delete them during shutdown.
- Use **search** for finding matching spans within a larger text and **full match** for validation of an entire field.
- For repeated search, advance by the previous match's ending byte. If the last match is empty, advance at least one byte or stop at end-of-input to avoid repeating the same match forever.
- Store regex IDs alongside application-owned validation rules; never assume any particular numeric ID is private to regex because the BPA namespace is shared.
- Do not rely on `\w`, dot, or case-insensitive matching as Unicode-aware validators.
- For untrusted patterns or large texts, apply application-level input limits, and always handle `CLM_RE_LIMIT_EXCEEDED` as a legitimate outcome.

## Testing and compatibility status

The supplied implementation was compiled as strict C11 with GCC and reviewed using a test harness that substitutes a minimal BPA registry. The harness exercises pattern reuse, capture groups, quantifiers, full-match versus search, ASCII categories, byte-oriented matching, and selected error conditions. That **does not replace a complete build and integration test with the real CLM BPA implementation**.

A differential exercise against Python's regex engine found differences involving certain nested empty repetitions. This module explicitly promises its **documented supported syntax and byte-oriented behavior**, not full cross-engine semantic parity. Deeper fuzzing and real-BPA integration tests remain recommended before treating arbitrary untrusted patterns as production hardened.

## Maintainer implementation map

| Phase | Key internal function(s) | Why it exists |
|---|---|---|
| Parse | `re_atom`, `re_piece`, `re_parse_sequence`, `re_parse_alt` | Convert syntax into bounded AST nodes, keeping group and branch order. |
| Normalize | `re_join` | Balance long concatenation/alternation ASTs to bound compiler recursion. |
| Compile | `re_compile_node`, `re_emit` | Lower AST into continuation-linked SPLIT/SAVE/consuming instructions. |
| Publish | `clm_re_compile_n` | Only publish a completed, right-sized BPA program. |
| Validate | `re_load` | Confirm BPA type, magic, size, opcode, and target bounds. |
| Execute | `re_add`, `re_execute` | Maintain prioritized active-thread lists and capture state, with an explicit step ceiling. |
| Retire | `clm_re_delete` | Release a valid compiled program by ID. |

These comments and this documentation describe the **provided implementation**. They do not add new regex operators, Unicode support, or different match semantics.
