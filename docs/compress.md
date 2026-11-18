# clm_compress

> Language-neutral module documentation. Derived from the supplied C implementation; equivalent APIs in other languages may differ.

## Purpose

Compression codecs, incremental transforms, and ZIP helpers.

## Behavior and constraints

Compression codecs, incremental transforms, and ZIP helpers.

Codec-specific state is adapted to common buffer and stream interfaces. Callers must
distinguish output exhaustion from completed input.

Compression for bytes, strings, streams and ZIP archives.

Formats (vendored libraries under ext/):
RAW_DEFLATE, ZLIB, GZIP   miniz     HTTP "deflate" is ZLIB, "gzip" is GZIP
LZ4                       lz4       LZ4 frame format (with content checksum)
BROTLI                    brotli    HTTP "br"
ZIP archives              miniz     in memory, see clm_compress_zip_*
Output is the standard format of each library, so it interoperates with other
tools. memlz and uzlib are not used: memlz is marked beta and uzlib duplicates
what miniz already provides.

Decoding treats its input as hostile: output is capped (max_output) so a tiny
input cannot expand without bound, corrupt, truncated and trailing data are
reported, and nothing is written past the cap. The incremental codec and the
stream adapters work in bounded steps, so they suit nonblocking transfers.

## Integration

- C reference: [**clm_compress**](../c/compress.md).
- Related CLM declarations in this archive: `clm_bytes`, `clm_stream`, `clm_types`.
- Portability note: module behavior is derived from the supplied implementation. Platform/backend coverage should be verified in each language port.
