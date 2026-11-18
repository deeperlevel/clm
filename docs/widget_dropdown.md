# clm_widget_dropdown

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

A readonly input with an owned right-hand button and an owned default list. Populate list_id with ordinary list-item children. Default-list selection commits the item ID and copies its label, including reselecting the same item. The list's public change handler remains available.

## Behavior and constraints

A readonly input with an owned right-hand button and an owned default list.
Populate list_id with ordinary list-item children. Default-list selection
commits the item ID and copies its label, including reselecting the same item.
The list's public change handler remains available.

dropdown_id zero selects the default list. A custom same-surface widget is
borrowed as popover content without reparenting it. It must remain alive until
replaced or the dropdown is destroyed. Its previous layout/input flags are
restored on replacement/destruction. Do not separately attach/detach its
popover while borrowed. Custom content calls commit from its own handlers;
selection IDs are opaque application IDs, not necessarily widget IDs.
Replacing popup content cancels queued commits from the previous content.

Commit copies and validates single-line UTF-8 immediately, then applies at the
next safe layout phase. Last queued commit wins. Applying it updates display
and selection, closes the popup, and notifies only if ID or text changed.
Callbacks see committed state and follow normal mutation restrictions; commit
and open/close/toggle are callback-safe exceptions. set_value is immediate,
silent, does not close the popup, and supersedes a queued commit on success.

The default list initially matches the preferred dropdown width; popup_height
defaults to 160. Resize it through its widget rectangle. Custom content keeps
its preferred dimensions. Popovers provide clamping/flipping, outside-click
and Escape dismissal, and focus restoration. Input is blocked when the dropdown
is readonly, effectively hidden or disabled; programmatic set_value is allowed.
Internal child IDs support styling, not destruction/reparenting or text edits.
Rendering uses the existing input/button/list font and sprite requirements.

## Integration

- C reference: [**clm_widget_dropdown**](../c/widget_dropdown.md).
- Related CLM declarations in this archive: `clm_widget_input`, `clm_widget_list`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
