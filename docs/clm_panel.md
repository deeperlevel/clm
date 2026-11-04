# CLM Panel

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_math_core](clm_math_core.md), [clm_surface](clm_surface.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Manages named panels within a surface, with geometry, transformations, input routing, focus and event handlers.

## Capabilities and operations

- Create and destroy panels on a surface; inspect geometry.
- Move, resize, rotate, tilt, scale and modify panel pivots.
- Control visibility, z order, input enablement, disabled/focus state and hit testing.
- Receive pointer, keyboard, drag/drop, resize and redraw events.
- Request layout and, in the current implementation, configure built-in panel flow and layout handlers.

## Resource, data and ownership model

A panel has identity distinct from its parent surface, is associated with a drawable region and may receive input in panel-local coordinates. Layout geometry is separate from visual transformation: changing a transform does not rewrite the layout rectangle.

## Behavioral contract and boundaries

Hit testing and event coordinate conversion must agree with the panel transform and clipping region. Drawing is a renderer responsibility. A panel does not own the parent surface. The current header includes some layout operations; the proposed independent `clm_layout` module remains an architectural proposal, not a released dependency.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Create a tool palette on a surface, arrange panels, register click handlers, and draw during each panel redraw event.

## Related CLM modules

[clm_math_core](clm_math_core.md), [clm_surface](clm_surface.md).
