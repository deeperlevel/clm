# clm_widget_span

> Language-neutral module overview and behavioral contract.

## Purpose

Plain UTF-8 text label. Owns a same-ID widget/panel and a copy of its text. Uses resolved typography/foreground, content-box origin and width wrapping. centered mode instead centers single-line glyph ink in the content box; it does not wrap and rejects carriage returns/newlines on creation/replacement. Newlines are supported; drawing clips to the panel and ancestor clips. Backgrounds/borders are not painted by this text-only widget.

## Behavior and constraints

Plain UTF-8 text label. Owns a same-ID widget/panel and a copy of its text.
Uses resolved typography/foreground, content-box origin and width wrapping.
centered mode instead centers single-line glyph ink in the content box;
it does not wrap and rejects carriage returns/newlines on creation/replacement.
Newlines are supported; drawing clips to the panel and ancestor clips.
Backgrounds/borders are not painted by this text-only widget.

Register a widget-manager font resolver before drawing nonempty labels.
Its loaded font face supplies family/weight/italics; size/color come from style.
Rendering occurs once per DRAW (REDRAW is followed by DRAW), before application
draw callbacks. Missing fonts/renderers and unsupported text backends report a
surface error and are recorded in span info; there is no silent font fallback.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_renderer`](renderer.md)
- [`clm_widget`](widget.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_span. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget](widget.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_span when an application needs: Plain UTF-8 text label. Owns a same-ID widget/panel and a copy of its text. Uses resolved typography/foreground, content-box origin and width wrapping. centered mode instead centers single-line glyph ink in the content box; it does not wrap and rejects carriage returns/newlines on creation/replacement. Newlines are supported; drawing clips to the panel and ancestor clips. Backgrounds/borders are not painted by this text-only widget.

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
