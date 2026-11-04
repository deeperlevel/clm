# CLM Compress

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md), [clm_bytes](clm_bytes.md), [clm_stream](clm_stream.md)
- **External libraries / platform facilities (C reference):** Brotli, LZ4, miniz/zlib support as selected by the current C implementation; exact codec availability depends on build.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Compression and decompression of bytes, streams and in-memory ZIP archives using interoperable standard encodings.

## Capabilities and operations

- Encode or decode raw DEFLATE, zlib, gzip, LZ4 frames and Brotli data.
- Convert content-coding names to algorithms and recognize selected stream signatures.
- Provide one-shot and incremental codecs and source/sink adapters; inspect ZIP archive entries.
- Bound decompression output and distinguish truncated, corrupt, unsupported and output-limit failures.

## Encodings

| Format | Typical use |
|---|---|
| Raw DEFLATE | Raw compressed blocks |
| Zlib | Wrapped DEFLATE; also used for HTTP `deflate` |
| Gzip | File and HTTP compression |
| LZ4 frame | Fast framed compression |
| Brotli | Web content coding `br` |
| ZIP archive | Multiple named entries in one archive |

## Decode safety

Decoders report corrupt input, unexpected truncation, unsupported formats and output-size-limit violations as distinct results. The reference snapshot includes a nonzero default decoded-output cap. Incremental adapters allow bounded progress and integration with [clm_stream](clm_stream.md).

ZIP helpers can write, enumerate, find and extract entries; archive entry names should be validated before using them as filesystem paths.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- Default decompression size caps protect against decompression bombs; callers can request an explicit limit.

## Example use cases

Stream-decompress a bounded HTTP body using the content coding named by the response.
