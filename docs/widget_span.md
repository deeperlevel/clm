# clm_widget_span

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Plain UTF-8 text label. Owns a same-ID widget/panel and a copy of its text. Uses resolved typography/foreground, content-box origin and width wrapping. centered mode instead centers single-line glyph ink in the content box; it does not wrap and rejects carriage returns/newlines on creation/replacement. Newlines are supported; drawing clips to the panel and ancestor clips. Backgrounds/borders are not painted by this text-only widget.

## Behavior and constraints

Plain UTF-8 text label. Owns a same-ID widget/panel and a copy of its text.
Uses resolved typography/foreground, content-box origin and width wrapping.
centered mode instead centers single-line glyph ink in the content box;
it does not wrap and rejects carriage returns/newlines on creation/replacement.
Newlines are supported; drawing clips to the panel and ancestor clips.
Backgrounds/borders are not painted by this text-only widget.

Register a widget-manager font resolver before drawing nonempty labels.
Its loaded font face supplies family/weight/italics; size/color come from style.
Rendering occurs once per DRAW (REDRAW is followed by DRAW), before application
draw callbacks. Missing fonts/renderers and unsupported text backends report a
surface error and are recorded in span info; there is no silent font fallback.

## Integration

- C reference: [**clm_widget_span**](../c/widget_span.md).
- Related CLM declarations in this archive: `clm_renderer`, `clm_widget`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
