# clm_widget

> Language-neutral module overview and behavioral contract.

## Purpose

Virtual widgets own same-ID panels. Use widget APIs for preferred geometry and lifecycle; panel APIs remain available for visibility/focus/transforms. Widgets form an owning virtual tree; destroying one destroys its descendants. Panel geometry remains surface-relative. Child preferred positions are relative to the parent's content box. Visual transforms remain panel-local, not inherited. Styles are borrowed IDs: detach them before destroying their style records. All calls belong on the surface UI thread. Mutations are forbidden in callbacks, except cursor feedback setters, which may be…

## Behavior and constraints

Virtual widgets own same-ID panels. Use widget APIs for preferred geometry
and lifecycle; panel APIs remain available for visibility/focus/transforms.
Widgets form an owning virtual tree; destroying one destroys its descendants.
Panel geometry remains surface-relative. Child preferred positions are relative
to the parent's content box. Visual transforms remain panel-local, not inherited.
Styles are borrowed IDs: detach them before destroying their style records.
All calls belong on the surface UI thread. Mutations are forbidden in callbacks,
except cursor feedback setters, which may be used by hover/drag handlers.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_panel`](panel.md)
- [`clm_style`](style.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_panel](panel.md) - required CLM capability.
- [clm_style](style.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget when an application needs: Virtual widgets own same-ID panels. Use widget APIs for preferred geometry and lifecycle; panel APIs remain available for visibility/focus/transforms. Widgets form an owning virtual tree; destroying one destroys its descendants. Panel geometry remains surface-relative. Child preferred positions are relative to the parent's content box. Visual transforms remain panel-local, not inherited. Styles are borrowed IDs: detach them before destroying their style records. All calls belong on the surface UI thread. Mutations are forbidden in callbacks, except cursor feedback setters, which may be…

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
