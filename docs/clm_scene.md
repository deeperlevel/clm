# CLM Scene

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_resource](clm_resource.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Defines a current minimal ID-based scene description for cameras, lighting sets and meshes. This is an **early implementation stub**, not the final language-neutral CLM scene-management contract.

## Capabilities and operations

- Create and delete a scene description.
- Reference a camera and lighting collection by ID.
- Associate and remove mesh IDs.
- Inspect the scene’s referenced resources.

## Resource, data and ownership model

The current graphical scene description stores *references* to resource IDs; it does not own those assets. Scene destruction therefore must not destroy referenced cameras, lights or meshes.

## Behavioral contract and boundaries

CLM’s planned language-neutral `clm_scene` has a **broader scope**: general game scene management, potentially including 2D/3D game states, introduction screens, tutorials, menus and gameplay transitions. No lifecycle, transition, stacking or activation API should be assumed from this graphics-only stub until it is formally specified.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

In the current stub, associate a camera and light set with a mesh collection for a scene draw call. A separate future scene-management contract may represent the game’s Main Menu and Gameplay scenes.

## Related CLM modules

[clm_resource](clm_resource.md).



**Specification status:** provisional. Do not infer a final shared scene-management API from the reference `scene.h` alone.
