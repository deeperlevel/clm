# clm_widget_accordion

> Language-neutral module overview and behavioral contract.

## Purpose

Vertical stack of headers with exactly one owned clm_widget_list open. The section array is copied at creation; at least one section is required. Lists start empty: retrieve their IDs and attach clm_widget_list_item children. List selection, overflow scrolling and styling use the existing list APIs.

## Behavior and constraints

Vertical stack of headers with exactly one owned clm_widget_list open.
The section array is copied at creation; at least one section is required.
Lists start empty: retrieve their IDs and attach clm_widget_list_item children.
List selection, overflow scrolling and styling use the existing list APIs.

A zero header_widget_id creates an owned button with a required copied UTF-8
label (empty allowed) and optional retained left icon. A nonzero ID adopts
an existing same-surface widget, including its descendants; label/icon fields
are then ignored. Custom headers keep their implementation, style and public
callbacks, but become visible, focusable and pointer-receiving. They must not
be reserved children, list items, or ancestor-related to other headers/the
accordion's parent. On creation failure adoption is rolled back. Successful
adoption transfers ownership: destroying the accordion destroys the header.
Activation targets the header root; normal target-only event routing applies.

Clicking a header or pressing Enter/Space opens its list. Up/Down (wrapping),
Home/End select and focus an eligible header, skipping disabled/readonly ones.
Clicking the active header keeps it open without restarting its animation.
Readonly/disabled/hidden accordions ignore activation input; setters remain
available. Header public callbacks coexist with internal activation.

On switching, the old list is hidden immediately. Only the new list is
visible while its height expands with smoothstep easing (default 200 ms).
Subsequent headers move with the expanding list. Rapid switches restart the
newly selected list's expansion. Creation starts fully open. Zero duration
disables animation; changing duration finishes any current animation.
Animations use surface chrono events, beginning on the first tick after a
switch; applications must keep polling. Paused/hidden delivery stops updates.
Header/list geometry and clipping update at the next safe layout phase.

Headers share header_height (default 32). They shrink equally when there is
insufficient height; the open list receives remaining space. Root style
controls background/border/padding and vertical gutter; child margins inset
their slots. header_style_id applies only to generated buttons. list_style_id
supplies the initial list style. Styles are borrowed and observed normally.
Lists keep their selection/scroll state between switches. Expansion defaults
to true. Collapse hides generated labels and all lists while preserving
icon-driven section selection; custom header content stays user-managed.

Direct children and their geometry/visibility are reserved. Do not destroy,
hide or reparent headers/lists or attach other direct children. Their content,
styles and public handlers can be edited normally. Change callbacks observe
committed active state and exclusive visibility, with mutations forbidden.
No-op changes are silent. Sprite/font requirements follow the existing
button/list renderers; root rendering errors are also reported by get_info.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_widget_button`](widget_button.md)
- [`clm_widget_list`](widget_list.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_accordion. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_widget_button](widget_button.md) - required CLM capability.
- [clm_widget_list](widget_list.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_accordion when an application needs: Vertical stack of headers with exactly one owned clm_widget_list open. The section array is copied at creation; at least one section is required. Lists start empty: retrieve their IDs and attach clm_widget_list_item children. List selection, overflow scrolling and styling use the existing list APIs.

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
