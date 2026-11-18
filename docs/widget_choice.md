# clm_widget_choice

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

A grouped selectable choice with an always-present owned span label. Group identity is (surface_id, nonzero group_id), independent of widget ancestry. Members of a live group must specify the same positive max_choices. The group disappears when its last member is destroyed; its limit is fixed while members exist. New choices start unselected.

## Behavior and constraints

A grouped selectable choice with an always-present owned span label. Group
identity is (surface_id, nonzero group_id), independent of widget ancestry.
Members of a live group must specify the same positive max_choices. The
group disappears when its last member is destroyed; its limit is fixed
while members exist. New choices start unselected.

A single-choice group behaves as radio buttons: activation selects another
member and clears the previous one, even if it is readonly or disabled;
activating the selected member does not clear it. Multi-choice groups toggle
on activation; when full, selecting another returns CLM_WIDGET_LIMIT_REACHED
and leaves the group unchanged. Disabled, hidden and readonly selections
still count toward the limit. Programmatic deselection is always allowed.

Clicks, including on the label, and non-repeating Space presses activate the
choice unless it is readonly or effectively disabled. Activation happens before
the application's click/key handler. A failed capacity attempt is recorded
in last_selection_status; it is not a surface error.

Change callbacks run for user and programmatic changes, after the entire
group has been updated. A replacement notifies the old choice first, then the
new choice. No callback is sent for unchanged state or destruction. Widget
mutations are forbidden during these callbacks. Callbacks are not emitted
when initial creation fails.

The label ID is assigned internally. Callers may edit its text/style, but
must not destroy, reparent or reposition it. A non-NULL label is required;
empty UTF-8 is allowed. The label is pointer-passthrough. The mark uses up
to 16 logical units with up to 4 units gap. Default single-choice marks are
circles; multi-choice marks are checkboxes, tinted by resolved foreground.
Per-state image IDs override the mark, retain their colors and are retained
until destruction. Marks require a sprite-capable renderer; nonempty labels
require a widget-manager font resolver. Child transforms are not inherited.

## Integration

- C reference: [**clm_widget_choice**](../c/widget_choice.md).
- Related CLM declarations in this archive: `clm_image`, `clm_renderer`, `clm_widget_span`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
