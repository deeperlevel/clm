# CLM Command-Line Parser

## Purpose

`clm_clp` parses caller-provided command-line arguments into a typed CLM
dictionary whose values are all strings. It does not read process-global
arguments, perform type inference, execute commands, expand variables, or load
response files.

On success, the parser creates a new dictionary using the caller's unused
output ID. The caller owns that dictionary and deletes it with the matching
typed-dictionary destruction operation.

## Supported argument forms

Each argument describes one named option:

| Form | Parsed key | Parsed value |
| --- | --- | --- |
| `--host=127.0.0.1` | `host` | `127.0.0.1` |
| `--port 8080` | `port` | `8080` |
| `-p=8080` or `-p 8080` | `p` | `8080` |
| `--verbose` | `verbose` | `true` |
| `output=file.txt` | `output` | `file.txt` |
| `--name=` | `name` | empty string |

The first `=` separates a key from its value; later `=` characters belong to
the value. One or more leading hyphens are removed from option keys. Short
options are not grouped: `-abc` is the key `abc`. `--no-name` is an ordinary
key, not a negated boolean. A negative numeric token such as `-1` may be used
as the value following a key. Other tokens that cannot be associated with a
key are positional arguments and cause a syntax error.

If a key has no inline value, the next token is its value unless that token
looks like another option; otherwise its value is the string `true`. Repeated
keys replace their prior value, so the last occurrence wins.

## Input APIs

- `parse_argv(argc, argv, output_id)` treats `argv[0]` as the program name and
  skips it.
- `parse_tokens(count, tokens, output_id)` treats every supplied token as an
  argument.
- `parse_string(command_line, output_id)` tokenizes an argument-only string;
  it does not expect or skip a program name.

The string tokenizer recognizes ASCII whitespace separators, single- and
double-quoted spans, and backslash escaping of the next byte. It is not a
POSIX shell or Windows command interpreter: substitutions, pipelines,
redirections, shell operators, and response-file syntax are not processed.
For reliable platform-specific process parsing, prefer `parse_argv`.

## Names, IDs, and values

Dictionary keys are numeric `clm_id` values. If an option name is already
registered, its existing ID is used. Otherwise the parser allocates an unused
ID and registers the name, so callers can later resolve it with `clm_id_get`
and read its value from the result dictionary. Values are UTF-8 strings; flags
use the literal string `true`. Numeric-looking values remain strings.

Names registered by a successful parse remain in the global ID registry.
Names registered by a parse that later fails are rolled back. Existing caller
registrations are never removed by the parser. Distinct names mapped to the
same numeric ID address the same dictionary entry; repeated writes follow the
last-value-wins rule.

## Failure and ownership

The output dictionary ID must be unused. Invalid pointers/counts, malformed
tokens, positional arguments, ID-registration errors, or allocation failures
return a failure status and leave no output dictionary behind. Auto-registered
names created during that failed parse are removed where possible.

The parser owns the dictionary it creates, but does not own the global ID
registry beyond registrations it must roll back on failure. The C binding is
not thread-safe; callers must serialize parsing and ID-registry access.

## Cross-language contract

Bindings may expose idiomatic argument arrays and result types, but should
preserve the supported grammar, string-only values, registration behavior,
last-value-wins rule, and failure rollback. A binding that receives native
process arguments should pass an already-tokenized argument sequence rather
than reimplementing the host shell's parsing rules.