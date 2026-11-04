# CLM Camera

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_resource](clm_resource.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Represents camera settings as named/ID-addressed resources that can be referenced by scene descriptions and render operations.

## Capabilities and operations

- Create a camera from configuration values.
- Update or read the configuration of an existing camera.
- Delete camera resources through an explicit lifecycle.
- Associate a camera ID with a scene without transferring ownership.

## Resource, data and ownership model

The resource is a camera description, not a window or a GPU buffer. A consumer interprets projection and view settings while composing a scene.

## Behavioral contract and boundaries

Creation must not silently overwrite existing IDs. Invalid or unsupported configurations should report errors. Deleting a camera does not implicitly delete scenes that reference it; consumers must handle missing references.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Create a perspective camera, bind its ID to a scene, and update the camera configuration as the player moves.

## Related CLM modules

[clm_resource](clm_resource.md).
