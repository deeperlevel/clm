# CLM Drawing

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Provides drawing value types and color manipulation independent of a particular canvas, surface or rendering backend.

## Capabilities and operations

- Construct and compare RGBA colors.
- Change alpha, interpolate colors and compose one straight-alpha color over another.
- Parse color strings and format display/web color representations.
- Manage color resources by ID where that form is convenient.
- Supply commonly used named color constants.

## Resource, data and ownership model

A color is a value. Parsing/formatting and optional registry operations operate on the same underlying color model. Unlike rendering commands, the module does not own a surface or graphics context.

## Behavioral contract and boundaries

The reference color model is 8-bit straight-alpha RGBA. Alpha blending and interpolation require well-defined rounding/clamping. Web-facing color names and strings must not be mistaken for an HTML/CSS layout engine.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Parse a design color, blend it over a background color, and pass the resulting value to a renderer.

## Related CLM modules

[clm_base](clm_base.md).



This is drawing data, distinct from `clm_renderer`, which issues drawing commands.
