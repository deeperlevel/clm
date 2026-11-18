# clm_widget_list_item

> Language-neutral module overview and behavioral contract.

## Purpose

A text row with independently optional left/right image icons. The owned label is always present; icons retain their assigned image resources. The default row height is 28 logical units; lists stretch its width. Rows are not TAB stops by default. Label and icon panels are passthrough, so clicking anywhere on a row activates it. Hidden/disabled/readonly state prevents user activation, including readonly on an owning list.

## Behavior and constraints

A text row with independently optional left/right image icons. The owned
label is always present; icons retain their assigned image resources.
The default row height is 28 logical units; lists stretch its width.
Rows are not TAB stops by default. Label and icon panels are passthrough,
so clicking anywhere on a row activates it. Hidden/disabled/readonly state
prevents user activation, including readonly on an owning list.

A list owns single selection and the active highlight. Standalone rows also
support hover and selected state (click or Enter/Space). Defaults use distinct
platform-inspired selected/active backgrounds and borders. Explicit style
properties override these state defaults, as with other CLM controls.
The label uses normal typography; icons retain their original image colors.
The three exposed child IDs may be styled but not destroyed/reparented/moved;
arbitrary additional children are rejected. Text is copied by the owned span.
Drawing nonempty text requires the manager's font resolver; visible boxes
and icons require a sprite-capable renderer. Styles remain caller-owned.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_widget_image`](widget_image.md)
- [`clm_widget_span`](widget_span.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_list_item. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_widget_image](widget_image.md) - required CLM capability.
- [clm_widget_span](widget_span.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_list_item when an application needs: A text row with independently optional left/right image icons. The owned label is always present; icons retain their assigned image resources. The default row height is 28 logical units; lists stretch its width. Rows are not TAB stops by default. Label and icon panels are passthrough, so clicking anywhere on a row activates it. Hidden/disabled/readonly state prevents user activation, including readonly on an owning list.

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
