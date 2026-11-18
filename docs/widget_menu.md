# clm_widget_menu

> Language-neutral capability overview. C implementation details are documented separately.

## Purpose

Surface-top menu strip supporting conventional dropdown menus and ribbon-style toolbars.

## Behavior and constraints

A menu is a docked strip of configurable text and/or icon buttons. In **dropdown mode**, each item opens a popover containing either a generated list or an application-supplied widget; activation selects the menu item. In **ribbon mode**, selecting an item shows its corresponding toolbar and hides the other ribbon toolbars. An optional leading logo button may control its own dropdown.

Generated buttons, lists and toolbars belong to the menu. Application-supplied custom dropdown content is borrowed, not destroyed with the menu, and its popover attachment is removed when the menu is destroyed. Applications can inspect the active item/content and receive selection notifications.

The menu's content is composed from ordinary widgets and uses shared styling. It is not a replacement for the surface or application navigation model.

## Integration

- Related capabilities: button, list, toolbar, popover.
- [C reference](../c/widget_menu.md) — signatures, concrete types, backend requirements, and error codes for one implementation.
- Capability availability can vary by language implementation and platform.
