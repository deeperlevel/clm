# clm_widget_tab

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Horizontal tab strip with borrowed page widget IDs on the same surface. The strip docks TOP (default) or BOTTOM: placement overrides widget.layout. A parent container's layout still takes precedence. Set widget.panel.rect.height for the strip height (32 by default). Use set_placement to change docking; left/right placement is not supported.

## Behavior and constraints

Horizontal tab strip with borrowed page widget IDs on the same surface.
The strip docks TOP (default) or BOTTOM: placement overrides widget.layout.
A parent container's layout still takes precedence. Set widget.panel.rect.height
for the strip height (32 by default). Use set_placement to change docking;
left/right placement is not supported.

Tabs have equal preferred outer widths (120 by default), with START, CENTER, END
or STRETCH alignment. On overflow they shrink equally, without wrapping.
widget.style_id supplies strip padding/border spacing and horizontal gutter;
the strip itself is transparent. Active/inactive styles apply to the owned
buttons, including inherited label typography. Zero IDs use platform-inspired
defaults. Styles are borrowed and observed through normal style revisions.

Labels are required copied UTF-8 strings; empty labels are allowed. Icons
are optional retained clm_image IDs, always placed to the left of the label.
Each nonzero page ID must be unique, unrelated by ancestry to the other
pages, and not an ancestor/descendant of this strip. A page cannot be shared
with another strip. Zero means no associated page. Pages are not reparented
or destroyed, and must remain alive while associated.

Creation and selection set only the selected page's local visibility true.
Ancestor visibility still applies. External page visibility changes are
overwritten on the next selection, including reselection of the active tab.
Destruction leaves the last page visibility flags unchanged.

Click, Left/Right (wrapping), and Home/End select tabs. TAB has one focus stop
for the strip; clicking a button also permits keyboard navigation. Readonly
or effectively disabled/hidden strips do not react to input; programmatic
selection is still allowed. Callbacks see committed selection/styles/page
visibility, under the usual mutation guard. No-op selections are silent.

Reserved button IDs and their callbacks/style/layout are implementation-owned.
Do not destroy/reparent/hide them or their label/image children. Label/image
content may be edited with the span/image APIs. Rendering uses button/span/
image renderers; labels require the manager's font resolver. Rendering errors
are reported by those child widgets and through the surface error mechanism.

## Integration

- C reference: [**clm_widget_tab**](../c/widget_tab.md).
- Related CLM declarations in this archive: `clm_widget_button`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
