# clm_widget_dropdown

> Language-neutral module overview and behavioral contract.

## Purpose

A readonly input with an owned right-hand button and an owned default list. Populate list_id with ordinary list-item children. Default-list selection commits the item ID and copies its label, including reselecting the same item. The list's public change handler remains available.

## Behavior and constraints

A readonly input with an owned right-hand button and an owned default list.
Populate list_id with ordinary list-item children. Default-list selection
commits the item ID and copies its label, including reselecting the same item.
The list's public change handler remains available.

dropdown_id zero selects the default list. A custom same-surface widget is
borrowed as popover content without reparenting it. It must remain alive until
replaced or the dropdown is destroyed. Its previous layout/input flags are
restored on replacement/destruction. Do not separately attach/detach its
popover while borrowed. Custom content calls commit from its own handlers;
selection IDs are opaque application IDs, not necessarily widget IDs.
Replacing popup content cancels queued commits from the previous content.

Commit copies and validates single-line UTF-8 immediately, then applies at the
next safe layout phase. Last queued commit wins. Applying it updates display
and selection, closes the popup, and notifies only if ID or text changed.
Callbacks see committed state and follow normal mutation restrictions; commit
and open/close/toggle are callback-safe exceptions. set_value is immediate,
silent, does not close the popup, and supersedes a queued commit on success.

The default list initially matches the preferred dropdown width; popup_height
defaults to 160. Resize it through its widget rectangle. Custom content keeps
its preferred dimensions. Popovers provide clamping/flipping, outside-click
and Escape dismissal, and focus restoration. Input is blocked when the dropdown
is readonly, effectively hidden or disabled; programmatic set_value is allowed.
Internal child IDs support styling, not destruction/reparenting or text edits.
Rendering uses the existing input/button/list font and sprite requirements.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_widget_input`](widget_input.md)
- [`clm_widget_list`](widget_list.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_dropdown. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_widget_input](widget_input.md) - required CLM capability.
- [clm_widget_list](widget_list.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_dropdown when an application needs: A readonly input with an owned right-hand button and an owned default list. Populate list_id with ordinary list-item children. Default-list selection commits the item ID and copies its label, including reselecting the same item. The list's public change handler remains available.

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
