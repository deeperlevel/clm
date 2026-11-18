# clm_widget_popover

> Language-neutral capability overview. C implementation details are documented separately.

## Purpose

Surface-bound overlay controller for dialog-style content and contextual popups.

## Behavior and constraints

An existing widget can be attached as popup content without changing its ownership. The controller places attached content above ordinary panels with a backdrop that optionally dims content underneath. Popovers can be anchored to other widgets and may flip or clamp to remain within the surface.

Opening moves focus into the content; Escape, outside presses, surface focus loss, and invalid or hidden anchors can dismiss it. The controller handles focus restoration and topmost popup ordering. Attachment does not transfer ownership of the existing widget.

Opening, closing, and toggling are deferred to a safe layout phase. Multiple dialogs and menus can share this overlay mechanism; the enclosing application decides whether a popup represents a dialog, menu, or another kind of UI.

## Integration

- Related capabilities: widget, layout, focus and input handling.
- [C reference](../c/widget_popover.md) — signatures, concrete types, backend requirements, and error codes for one implementation.
- Capability availability can vary by language implementation and platform.
