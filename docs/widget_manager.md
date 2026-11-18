# clm_widget_manager

> Language-neutral module overview and behavioral contract.

## Purpose

Per-surface widget event routing and virtual tree layout. Panels stay flat. Ordinary panels are never laid out or dispatched as widgets. Existing public panel observers coexist.

## Behavior and constraints

Per-surface widget event routing and virtual tree layout. Panels stay flat.
Ordinary panels are never laid out or dispatched as widgets.
Existing public panel observers coexist.

Unspecified widget styles use CLM-rendered, light platform-inspired defaults
selected from the surface backend (Windows, macOS, iOS, Android, X11/Wayland
or web). Terminal and unsupported console backends use neutral defaults.
These templates are approximations, not native controls: they do not track
system theme, dark mode, accent, high contrast, or OS theme changes. Explicit
global, inherited and widget style properties take precedence, including
transparent colors and zero dimensions. Custom widget image IDs retain their
original colors. Font names are passed to the application font resolver;
the manager does not load fonts.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_widget`](widget.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_manager. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_widget](widget.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_manager when an application needs: Per-surface widget event routing and virtual tree layout. Panels stay flat. Ordinary panels are never laid out or dispatched as widgets. Existing public panel observers coexist.

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
