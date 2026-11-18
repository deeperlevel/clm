# clm_widget_button

> Language-neutral module overview and behavioral contract.

## Purpose

Push button with optional owned span and image children. Set label to an absent value for an icon-only button; icon_image_id 0 omits the icon. Child IDs are assigned automatically and exposed for editing their content/style only: do not destroy, reparent or reposition them. Children pass pointer input to the button and are destroyed with it. Their initial internal style ID is reserved: do not modify, destroy or reuse it. Images retain their source ID.

## Behavior and constraints

Push button with optional owned span and image children. Set label to an absent value
for an icon-only button; icon_image_id 0 omits the icon. Child IDs are
assigned automatically and exposed for editing their content/style only:
do not destroy, reparent or reposition them. Children pass pointer input to
the button and are destroyed with it. Their initial internal style ID is
reserved: do not modify, destroy or reuse it. Images retain their source ID.

Content is left-aligned by default; label_centered centers single-line label
ink within its available area. The icon is at most 16 logical units square. The
horizontal style gutter separates it from the label (default 4 units).
A right-hand icon is anchored at the content's right edge because span text
is not measured; its label occupies the remaining width.
Styles determine text, colors,
border width/corners, spacing and layout. Unspecified background, border
color and corners come from the surface's platform-inspired template;
border width and padding default to 1 and 4.
Explicit transparent colors or zero widths/padding remain valid overrides.
Button backgrounds are rasterized and require a sprite-capable renderer.
Nonempty labels require a widget-manager font resolver.
Hover, pressed, disabled and focus use template colors unless those colors
are explicitly specified in the global or widget style.

Mouse clicks and non-repeating Space/Enter key presses invoke the button's
widget click callback. Keyboard clicks are synthetic widget events: their
panel event has CLICK kind, key and PRESS action; panel-level click observers
are not called. Readonly or effectively disabled buttons do not invoke the
widget click callback. Key events are still delivered normally. Inherited
clipping/visibility follow the widget tree; child transforms are not inherited.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_widget_image`](widget_image.md)
- [`clm_widget_span`](widget_span.md)

## Integration
- Availability and backend support may vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_button. It explains
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

Use clm_widget_button when an application needs: Push button with optional owned span and image children. Set label to an absent value for an icon-only button; icon_image_id 0 omits the icon. Child IDs are assigned automatically and exposed for editing their content/style only: do not destroy, reparent or reposition them. Children pass pointer input to the button and are destroyed with it. Their initial internal style ID is reserved: do not modify, destroy or reuse it. Images retain their source ID.

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
