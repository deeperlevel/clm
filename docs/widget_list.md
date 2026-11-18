# clm_widget_list

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

A single-selection, top-to-bottom list. Only list-item widgets may be attached as application children; internal scrollbar children are reserved. The list is the TAB stop; clicking an item also permits keyboard navigation. Up/Down (nonwrapping) and Home/End move the active highlight, skipping hidden, disabled and readonly items. Enter/Space or a click selects. Hover changes only the highlight; leaving clears mouse highlighting, not keyboard highlighting.

## Behavior and constraints

A single-selection, top-to-bottom list. Only list-item widgets may be attached
as application children; internal scrollbar children are reserved.
The list is the TAB stop; clicking an item also permits keyboard navigation.
Up/Down (nonwrapping) and Home/End move the active highlight, skipping hidden,
disabled and readonly items. Enter/Space or a click selects. Hover changes
only the highlight; leaving clears mouse highlighting, not keyboard highlighting.

Overflow scrollbars default on. Disabling them hides the bars, not scrolling:
wheel and keyboard reveal still work, and content always clips to the viewport.
Items stretch to the viewport width and keep their preferred heights.
Hidden items take no space. Selection survives hiding/disabling an item;
detaching/destroying an item clears its active/selected IDs without notification.
Changes to layout/scroll positions commit at the next surface poll.

Selection callbacks observe committed state, for both input and programmatic
changes, under normal callback mutation restrictions. No-op changes are silent.
Styles are caller-owned; the list background/border use the ordinary box style.

## Integration

- C reference: [**clm_widget_list**](../c/widget_list.md).
- Related CLM declarations in this archive: `clm_renderer`, `clm_widget_manager`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
