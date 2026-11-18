# clm_widget_button

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Push button with optional owned span and image children. Set label to nullptr for an icon-only button; icon_image_id 0 omits the icon. Child IDs are assigned automatically and exposed for editing their content/style only: do not destroy, reparent or reposition them. Children pass pointer input to the button and are destroyed with it. Their initial internal style ID is reserved: do not modify, destroy or reuse it. Images retain their source ID.

## Behavior and constraints

Push button with optional owned span and image children. Set label to nullptr
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

## Integration

- C reference: [**clm_widget_button**](../c/widget_button.md).
- Related CLM declarations in this archive: `clm_widget_image`, `clm_widget_span`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
