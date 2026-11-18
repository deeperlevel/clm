# clm_widget_accordion

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Vertical stack of headers with exactly one owned clm_widget_list open. The section array is copied at creation; at least one section is required. Lists start empty: retrieve their IDs and attach clm_widget_list_item children. List selection, overflow scrolling and styling use the existing list APIs.

## Behavior and constraints

Vertical stack of headers with exactly one owned clm_widget_list open.
The section array is copied at creation; at least one section is required.
Lists start empty: retrieve their IDs and attach clm_widget_list_item children.
List selection, overflow scrolling and styling use the existing list APIs.

A zero header_widget_id creates an owned button with a required copied UTF-8
label (empty allowed) and optional retained left icon. A nonzero ID adopts
an existing same-surface widget, including its descendants; label/icon fields
are then ignored. Custom headers keep their implementation, style and public
callbacks, but become visible, focusable and pointer-receiving. They must not
be reserved children, list items, or ancestor-related to other headers/the
accordion's parent. On creation failure adoption is rolled back. Successful
adoption transfers ownership: destroying the accordion destroys the header.
Activation targets the header root; normal target-only event routing applies.

Clicking a header or pressing Enter/Space opens its list. Up/Down (wrapping),
Home/End select and focus an eligible header, skipping disabled/readonly ones.
Clicking the active header keeps it open without restarting its animation.
Readonly/disabled/hidden accordions ignore activation input; setters remain
available. Header public callbacks coexist with internal activation.

On switching, the old list is hidden immediately. Only the new list is
visible while its height expands with smoothstep easing (default 200 ms).
Subsequent headers move with the expanding list. Rapid switches restart the
newly selected list's expansion. Creation starts fully open. Zero duration
disables animation; changing duration finishes any current animation.
Animations use surface chrono events, beginning on the first tick after a
switch; applications must keep polling. Paused/hidden delivery stops updates.
Header/list geometry and clipping update at the next safe layout phase.

Headers share header_height (default 32). They shrink equally when there is
insufficient height; the open list receives remaining space. Root style
controls background/border/padding and vertical gutter; child margins inset
their slots. header_style_id applies only to generated buttons. list_style_id
supplies the initial list style. Styles are borrowed and observed normally.
Lists keep their selection/scroll state between switches. Expansion defaults
to true. Collapse hides generated labels and all lists while preserving
icon-driven section selection; custom header content stays user-managed.

Direct children and their geometry/visibility are reserved. Do not destroy,
hide or reparent headers/lists or attach other direct children. Their content,
styles and public handlers can be edited normally. Change callbacks observe
committed active state and exclusive visibility, with mutations forbidden.
No-op changes are silent. Sprite/font requirements follow the existing
button/list renderers; root rendering errors are also reported by get_info.

## Integration

- C reference: [**clm_widget_accordion**](../c/widget_accordion.md).
- Related CLM declarations in this archive: `clm_widget_button`, `clm_widget_list`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
