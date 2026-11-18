# clm_widget_list_item

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

A text row with independently optional left/right image icons. The owned label is always present; icons retain their assigned image resources. The default row height is 28 logical units; lists stretch its width. Rows are not TAB stops by default. Label and icon panels are passthrough, so clicking anywhere on a row activates it. Hidden/disabled/readonly state prevents user activation, including readonly on an owning list.

## Behavior and constraints

A text row with independently optional left/right image icons. The owned
label is always present; icons retain their assigned image resources.
The default row height is 28 logical units; lists stretch its width.
Rows are not TAB stops by default. Label and icon panels are passthrough,
so clicking anywhere on a row activates it. Hidden/disabled/readonly state
prevents user activation, including readonly on an owning list.

A list owns single selection and the active highlight. Standalone rows also
support hover and selected state (click or Enter/Space). Defaults use distinct
platform-inspired selected/active backgrounds and borders. Explicit style
properties override these state defaults, as with other CLM controls.
The label uses normal typography; icons retain their original image colors.
The three exposed child IDs may be styled but not destroyed/reparented/moved;
arbitrary additional children are rejected. Text is copied by the owned span.
Drawing nonempty text requires the manager's font resolver; visible boxes
and icons require a sprite-capable renderer. Styles remain caller-owned.

## Integration

- C reference: [**clm_widget_list_item**](../c/widget_list_item.md).
- Related CLM declarations in this archive: `clm_widget_image`, `clm_widget_span`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
