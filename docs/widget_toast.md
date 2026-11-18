# clm_widget_toast

> Language-neutral capability overview. C implementation details are documented separately.

## Purpose

Noninteractive, temporary notification overlay for application and game surfaces.

## Behavior and constraints

A toast shows a short message and optionally an icon above the normal widget layer. It does not take focus or intercept pointer input. Supported placement is top-center, center, or bottom-center of the surface.

Toasts begin hidden and become visible when shown. Showing a new message replaces the previous message and restarts the lifetime. A configurable expiration interval hides the toast automatically; a zero-duration toast remains visible until explicitly hidden. Timed expiration is driven by the surface's event/clock updates, so applications must keep processing events.

The root controls background, border, and padding; child image and text widgets allow additional presentation customization.

## Integration

- Related capabilities: widget, span, image, rendering, surface clock.
- [C reference](../c/widget_toast.md) — signatures, concrete types, backend requirements, and error codes for one implementation.
- Capability availability can vary by language implementation and platform.
