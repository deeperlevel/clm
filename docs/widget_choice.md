# clm_widget_choice

> Language-neutral module overview and behavioral contract.

## Purpose

A grouped selectable choice with an always-present owned span label. Group identity is (surface_id, nonzero group_id), independent of widget ancestry. Members of a live group must specify the same positive max_choices. The group disappears when its last member is destroyed; its limit is fixed while members exist. New choices start unselected.

## Behavior and constraints

A grouped selectable choice with an always-present owned span label. Group
identity is (surface_id, nonzero group_id), independent of widget ancestry.
Members of a live group must specify the same positive max_choices. The
group disappears when its last member is destroyed; its limit is fixed
while members exist. New choices start unselected.

A single-choice group behaves as radio buttons: activation selects another
member and clears the previous one, even if it is readonly or disabled;
activating the selected member does not clear it. Multi-choice groups toggle
on activation; when full, selecting another returns CLM_WIDGET_LIMIT_REACHED
and leaves the group unchanged. Disabled, hidden and readonly selections
still count toward the limit. Programmatic deselection is always allowed.

Clicks, including on the label, and non-repeating Space presses activate the
choice unless it is readonly or effectively disabled. Activation happens before
the application's click/key handler. A failed capacity attempt is recorded
in last_selection_status; it is not a surface error.

Change callbacks run for user and programmatic changes, after the entire
group has been updated. A replacement notifies the old choice first, then the
new choice. No callback is sent for unchanged state or destruction. Widget
mutations are forbidden during these callbacks. Callbacks are not emitted
when initial creation fails.

The label ID is assigned internally. Callers may edit its text/style, but
must not destroy, reparent or reposition it. A non-an absent value label is required;
empty UTF-8 is allowed. The label is pointer-passthrough. The mark uses up
to 16 logical units with up to 4 units gap. Default single-choice marks are
circles; multi-choice marks are checkboxes, tinted by resolved foreground.
Per-state image IDs override the mark, retain their colors and are retained
until destruction. Marks require a sprite-capable renderer; nonempty labels
require a widget-manager font resolver. Child transforms are not inherited.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_image`](image.md)
- [`clm_renderer`](renderer.md)
- [`clm_widget_span`](widget_span.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_choice. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_image](image.md) - required CLM capability.
- [clm_renderer](renderer.md) - required CLM capability.
- [clm_widget_span](widget_span.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_choice when an application needs: A grouped selectable choice with an always-present owned span label. Group identity is (surface_id, nonzero group_id), independent of widget ancestry. Members of a live group must specify the same positive max_choices. The group disappears when its last member is destroyed; its limit is fixed while members exist. New choices start unselected.

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
