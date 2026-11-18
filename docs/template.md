# clm_template

> Language-neutral module overview and behavioral contract.

## Purpose

Template parsing, expression evaluation, and rendering.

## Behavior and constraints

Template parsing, expression evaluation, and rendering.

Compilation builds reusable template structure. Rendering walks that structure against
an explicit context while escaping and output limits are applied at write boundaries.

Text templates in a Jinja2-like language.

A template is compiled once and rendered any number of times with different
data. Rendering never runs code: templates can only read variables, so they
are safe to take from less trusted sources, and output size, loop iterations
and nesting are bounded.

Syntax
{{ expression }}                       print a value
{% if a %}..{% elif b %}..{% else %}..{% endif %}
{% for x in items %}..{% else %}..{% endfor %}   (else runs when empty)
{% set name = expression %}            variable local to the enclosing block
{% raw %}..{% endraw %}                text copied without interpretation
{# comment #}
{%- ... -%}, {{- ... -}}, {#- ... -#}  strip whitespace on that side
Expressions
literals: 'text' "text" 12 3.5 true false none (also True False None)
a.b  a['b']  a[0]  a[-1]               members and array elements
+ - * / // % **  ~ (join as text)  == != < <= > >=  and or not
x in y  x not in y  (substring, array element, or object key)
x if cond else y
x is [not] test(args)                  tests: defined undefined none string
number integer float boolean mapping sequence even odd divisibleby(n)
true false lower upper
x|filter(args)                         filters: upper lower capitalize
title trim length (count) default(value, boolean) (d) join(sep)
replace(old, new) escape (e) safe first last int(default) float(default)
string abs round(digits) reverse truncate(length, killwords, end)
urlencode sum
range(stop) range(start, stop[, step]) as a for-loop iterable
loop.index loop.index0 loop.revindex loop.revindex0 loop.first loop.last
loop.length inside a for loop
Not supported: macros, includes, inheritance, list/dict literals, iterating
the keys of an object, calling methods, and tuple unpacking.

Data
A clm_template_context holds named values (strings, numbers, booleans,
an absent value) and can bind clm_tree nodes (objects, arrays, leaves) by reference,
so JSON/YAML/TOML-parsed data renders directly. Object members are found by
name through the clm_id registry, so the names a template uses must be
registered (use the *_loads_auto_ids parsers or clm_id_register).
Strings are UTF-8; filters change ASCII letters only.
Undefined names print as nothing and are falsy, unless strict_undefined is on.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_bytes`](bytes.md)
- [`clm_types`](types.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_template. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_types](types.md) - required CLM capability.
- [clm_bytes](bytes.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_template when an application needs: Template parsing, expression evaluation, and rendering.

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
