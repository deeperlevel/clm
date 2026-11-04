# CLM Audio

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** miniaudio is the current C playback/codec backend (not a cross-language requirement); platform audio device.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Provides a playback engine and independently controllable audio playback instances for files and decoded sound sources supported by an implementation.

## Capabilities and operations

- Initialize and shut down an audio engine tied to an output device.
- Create and dispose individual playback instances without requiring one engine per file.
- Start, stop, pause and resume playback; query current position, duration and play state.
- Adjust gain, playback rate and seek position; configure repeated playback and scheduled delays.

## Resource, data and ownership model

An engine owns the output-device connection and outlives every playback instance created through it. The current interface models engine/playback objects rather than registry-addressed resource IDs. A playback instance owns its decoder/play state; it does not own the engine.

## Behavioral contract and boundaries

Gain is a nonnegative linear multiplier. Playback speed is positive and also alters pitch; it is not time-stretching. Repeats count *additional* plays after the initial one; negative-one denotes indefinite repetition. Loop scheduling that relies on delays or finite repeats requires an update/pump step. Initialization, invalid states, active dependents, and device failures must be distinguishable. The current C implementation accepts WAV, MP3, FLAC and Ogg Vorbis through miniaudio; that codec/backend choice is not imposed on every language.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Initialize an engine, load two audio files as distinct playback instances, set their volumes and loops separately, periodically update the engine, dispose both instances, then shut down the engine.

## Related CLM modules

[clm_base](clm_base.md).
