# clm_widget_manager

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Per-surface widget event routing and virtual tree layout. Panels stay flat. Ordinary panels are never laid out or dispatched as widgets. Existing public panel observers coexist.

## Behavior and constraints

Per-surface widget event routing and virtual tree layout. Panels stay flat.
Ordinary panels are never laid out or dispatched as widgets.
Existing public panel observers coexist.

Unspecified widget styles use CLM-rendered, light platform-inspired defaults
selected from the surface backend (Windows, macOS, iOS, Android, X11/Wayland
or web). Terminal and unsupported console backends use neutral defaults.
These templates are approximations, not native controls: they do not track
system theme, dark mode, accent, high contrast, or OS theme changes. Explicit
global, inherited and widget style properties take precedence, including
transparent colors and zero dimensions. Custom widget image IDs retain their
original colors. Font names are passed to the application font resolver;
the manager does not load fonts.

## Integration

- C reference: [**clm_widget_manager**](../c/widget_manager.md).
- Related CLM declarations in this archive: `clm_widget`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
