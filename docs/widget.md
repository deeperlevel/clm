# clm_widget

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Virtual widgets own same-ID panels. Use widget APIs for preferred geometry and lifecycle; panel APIs remain available for visibility/focus/transforms. Widgets form an owning virtual tree; destroying one destroys its descendants. Panel geometry remains surface-relative. Child preferred positions are relative to the parent's content box. Visual transforms remain panel-local, not inherited. Styles are borrowed IDs: detach them before destroying their style records. All calls belong on the surface UI thread. Mutations are forbidden in callbacks, except cursor feedback setters, which may be…

## Behavior and constraints

Virtual widgets own same-ID panels. Use widget APIs for preferred geometry
and lifecycle; panel APIs remain available for visibility/focus/transforms.
Widgets form an owning virtual tree; destroying one destroys its descendants.
Panel geometry remains surface-relative. Child preferred positions are relative
to the parent's content box. Visual transforms remain panel-local, not inherited.
Styles are borrowed IDs: detach them before destroying their style records.
All calls belong on the surface UI thread. Mutations are forbidden in callbacks,
except cursor feedback setters, which may be used by hover/drag handlers.

## Integration

- C reference: [**clm_widget**](../c/widget.md).
- Related CLM declarations in this archive: `clm_panel`, `clm_style`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
