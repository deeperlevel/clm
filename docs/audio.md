# clm_audio

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Audio decoding, device playback, and playback controls.

## Behavior and constraints

Audio decoding, device playback, and playback controls.

Decoded sample storage and playback devices have separate lifetimes. Backend callbacks
consume samples while control operations update playback state.

## Integration

- C reference: [**clm_audio**](../c/audio.md).
- Related CLM declarations in this archive: `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
