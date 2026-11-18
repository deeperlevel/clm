# clm_widget_menu

> Language-neutral module overview and behavioral contract.

## Purpose

Surface-top menu strip supporting conventional dropdown menus and ribbon-style toolbars.

## Behavior and constraints

A menu is a docked strip of configurable text and/or icon buttons. In **dropdown mode**, each item opens a popover containing either a generated list or an application-supplied widget; activation selects the menu item. In **ribbon mode**, selecting an item shows its corresponding toolbar and hides the other ribbon toolbars. An optional leading logo button may control its own dropdown.

Generated buttons, lists and toolbars belong to the menu. Application-supplied custom dropdown content is borrowed, not destroyed with the menu, and its popover attachment is removed when the menu is destroyed. Applications can inspect the active item/content and receive selection notifications.

The menu's content is composed from ordinary widgets and uses shared styling. It is not a replacement for the surface or application navigation model.

## CLM dependencies and related modules

Related CLM module interfaces used or referenced by this capability:

- [`clm_widget_button`](widget_button.md)
- [`clm_widget_list`](widget_list.md)
- [`clm_widget_popover`](widget_popover.md)
- [`clm_widget_toolbar`](widget_toolbar.md)

## Integration
- Capability availability can vary by language implementation and platform.
<!-- CLM-DOC-NEUTRAL-SCOPE:BEGIN -->
## What this document defines

This page is the language-neutral contract for clm_widget_menu. It explains
the module's responsibility, observable behavior, ownership and failure
semantics, and relationships to other CLM modules. It intentionally does not
prescribe a source language, memory representation, exception model, native
library, operating system API, or build system.

## Direct CLM dependencies

- [clm_widget_button](widget_button.md) - required CLM capability.
- [clm_widget_list](widget_list.md) - required CLM capability.
- [clm_widget_toolbar](widget_toolbar.md) - required CLM capability.

Dependencies are capability relationships: an implementation may satisfy them
with native facilities, another CLM module, or an equivalent library, provided
the linked module's observable contract is preserved.

## Typical use

Use clm_widget_menu when an application needs: Surface-top menu strip supporting conventional dropdown menus and ribbon-style toolbars.

A conforming workflow is conceptually:

1. Configure or acquire the module's inputs.
2. Perform an operation defined by this module and inspect its outcome.
3. Observe ownership, lifetime, ordering, and concurrency rules while using
   dependent CLM resources.
4. Release or reset owned state through the documented lifecycle operation.

## Language-neutral boundaries

Implementations may choose native types, allocation, error reporting, callbacks,
threads, and platform integrations. They must preserve the documented data
meaning, distinguishable outcomes, lifetime rules, synchronization contract,
and dependency relationships. Implementation availability and platform
support must be reported rather than silently substituted.
<!-- CLM-DOC-NEUTRAL-SCOPE:END -->
