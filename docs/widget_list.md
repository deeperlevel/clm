# clm_widget_list

> Language-neutral module overview and behavioral contract.

## Purpose

A single-selection, top-to-bottom list. Only list-item widgets may be attached as application children; internal scrollbar children are reserved. The list is the TAB stop; clicking an item also permits keyboard navigation. Up/Down (nonwrapping) and Home/End move the active highlight, skipping hidden, disabled and readonly items. Enter/Space or a click selects. Hover changes only the highlight; leaving clears mouse highlighting, not keyboard highlighting.

## Behavior and constraints

A single-selection, top-to-bottom list. Only list-item widgets may be attached
as application children; internal scrollbar children are reserved.
The list is the TAB stop; clicking an item also permits keyboard navigation.
Up/Down (nonwrapping) and Home/End move the active highlight, skipping hidden,
disabled and readonly items. Enter/Space or a click selects. Hover changes
only the highlight; leaving clears mouse highlighting, not keyboard highlighting.

Overflow scrollbars default on. Disabling them hides the bars, not scrolling:
wheel and keyboard reveal still work, and content always clips to the viewport.
Items stretch to the viewport width and keep their preferred heights.
Hidden items take no space. Selection survives hiding/disabling an item;
detaching/destroying an item clears its active/selected IDs without notification.
Changes to layout/scroll positions commit at the next surface poll.

Selection callbacks observe committed state, for both input and programmatic
changes, under normal callback mutation restrictions. No-op changes are silent.
Styles are caller-owned; the list background/border use the ordinary box style.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_renderer`](renderer.md)
- [`clm_widget_manager`](widget_manager.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_list. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget_manager](widget_manager.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_list when an application needs: A single-selection, top-to-bottom list. Only list-item widgets may be attached as application children; internal scrollbar children are reserved. The list is the TAB stop; clicking an item also permits keyboard navigation. Up/Down (nonwrapping) and Home/End move the active highlight, skipping hidden, disabled and readonly items. Enter/Space or a click selects. Hover changes only the highlight; leaving clears mouse highlighting, not keyboard highlighting.

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
