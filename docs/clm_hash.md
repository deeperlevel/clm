# CLM Hash — Language-Neutral Module Contract

**Module:** `clm_hash`  
**C header:** `clm/hash.h`  
**Scope:** Hash-algorithm metadata, byte/text adapters, and digest formatting.

## 1. Purpose and current status

`clm_hash` defines a common contract for hashing byte sequences and strings. It
separates the digest (binary bytes) from its optional lowercase hexadecimal
representation, and offers adapters for caller-owned buffers and CLM string
IDs.

The C implementation uses **freehash** for digest computation:
<https://github.com/ghaberek/freehash>. Its source is included from
`clm/ext/freehash.c` and the selected algorithms are enabled by
`clm/ext/freehash.h`. The CLM layer maps its algorithm descriptors to the
stable `CLM_HASH_ALGORITHM_*` identifiers and returns binary or lowercase-hex
results through the same public operations.

The identifiers are CLM-level values, not freehash registry indices. Each C
identifier maps directly to its corresponding freehash descriptor.

## 2. Data and text model

Hash input is a sequence of bytes. Hashing does not normalize text, change
letter case, remove whitespace, or add a terminator. For a NUL-terminated text
input, the terminator is excluded. For an explicit byte sequence, every byte
within the specified length is included, including zero-valued bytes.

CLM strings contain validated UTF-8. A CLM-string adapter hashes the string's
UTF-8 bytes, not its character count and not a converted external encoding.
String-oriented C adapters validate UTF-8. The byte-oriented C-string adapter
does not validate encoding and treats the bytes up to the first NUL as-is.

Binary digest output is distinct from its printable representation:

- **Binary digest:** exactly the algorithm's digest size in bytes.
- **Hex digest:** two lowercase hexadecimal characters per digest byte,
  followed by a NUL terminator when written to a C character buffer.
- CLM string output stores the lowercase hex characters as string content;
  its internal terminator is not part of the digest text.

## 3. Algorithms and digest sizes

| Identifier | Algorithm name | Digest size |
|---|---|---:|
| `CLM_HASH_ALGORITHM_MD2` | MD2 | 16 bytes |
| `CLM_HASH_ALGORITHM_MD4` | MD4 | 16 bytes |
| `CLM_HASH_ALGORITHM_MD5` | MD5 | 16 bytes |
| `CLM_HASH_ALGORITHM_SHA1` | SHA1 | 20 bytes |
| `CLM_HASH_ALGORITHM_SHA224` | SHA2-224 | 28 bytes |
| `CLM_HASH_ALGORITHM_SHA256` | SHA2-256 | 32 bytes |
| `CLM_HASH_ALGORITHM_SHA384` | SHA2-384 | 48 bytes |
| `CLM_HASH_ALGORITHM_SHA512` | SHA2-512 | 64 bytes |
| `CLM_HASH_ALGORITHM_SHA512_224` | SHA2-512/224 | 28 bytes |
| `CLM_HASH_ALGORITHM_SHA512_256` | SHA2-512/256 | 32 bytes |
| `CLM_HASH_ALGORITHM_SHA3_224` | SHA3-224 | 28 bytes |
| `CLM_HASH_ALGORITHM_SHA3_256` | SHA3-256 | 32 bytes |
| `CLM_HASH_ALGORITHM_SHA3_384` | SHA3-384 | 48 bytes |
| `CLM_HASH_ALGORITHM_SHA3_512` | SHA3-512 | 64 bytes |

The C implementation maps every listed algorithm to its freehash descriptor.
Unknown identifiers have no digest size and are reported as unsupported by
digest operations.

## 4. Operations

The C API expresses the operations below as inline functions; other language
ports may use native functions or methods while preserving the same observable
behavior.

| Operation | Input | Output |
|---|---|---|
| Digest-size query | Algorithm identifier | Binary digest size, or zero when unknown |
| Hex-size query | Algorithm identifier | Required hex-buffer size including terminator, or zero when unknown |
| Algorithm-name lookup | Algorithm identifier and an existing CLM string ID | Display name stored in that string |
| Hex encoding | Arbitrary bytes | Lowercase hexadecimal text in a caller-owned buffer |
| `bytes` | Arbitrary bytes and explicit length | Binary digest in a caller-owned buffer |
| `bytes_as` | NUL-terminated C string, interpreted as bytes | Binary digest in a caller-owned buffer |
| `bytes_from` | CLM string ID | Binary digest in a caller-owned buffer |
| `bytes_to` | Arbitrary bytes and explicit length | Hex digest stored in an existing CLM string |
| `string_as` | NUL-terminated UTF-8 C string | Hex digest in a caller-owned character buffer |
| `string_to` | NUL-terminated UTF-8 C string | Hex digest stored in an existing CLM string |
| `string_from` | Source and destination CLM string IDs | Hex digest stored in the destination string |

The suffix conventions are:

- **`_as`**: accept a C string directly.
- **`_from`**: read input from a CLM string ID.
- **`_to`**: write the hexadecimal result to a CLM string ID.

The `bytes_*` operations preserve byte semantics, except that `bytes_as`
necessarily uses C-string length and therefore stops at the first NUL.
`string_*` operations treat their text input as UTF-8. Byte-output operations
return raw digest bytes; `_to` operations return textual hexadecimal output.

The hex encoder is independent of the hashing backend and may be used to
format any byte sequence. Its source and destination must not overlap. It
checks capacity before writing and leaves the output unchanged on failure.

## 5. Results and failure behavior

| Result | Meaning |
|---|---|
| `CLM_HASH_SUCCESS` | Operation completed successfully |
| `CLM_HASH_ERROR_INVALID_ARGUMENT` | Invalid pointer, string ID, or input |
| `CLM_HASH_ERROR_UNSUPPORTED_ALGORITHM` | Algorithm identifier is not recognized |
| `CLM_HASH_ERROR_BUFFER_TOO_SMALL` | Caller-provided output capacity is insufficient |
| `CLM_HASH_ERROR_BACKEND` | Hash computation is unavailable or failed in the backend |
| `CLM_HASH_ERROR_NO_MEMORY` | Reserved for backend allocation failures |

Digest operations validate input, algorithm, and output capacity before
requesting computation. The C implementation computes into temporary storage
and copies the digest to caller output only on success. String-output adapters
validate the destination before hashing and replace its content only after
successful computation and formatting. Backend errors map to
`CLM_HASH_ERROR_BACKEND`; freehash allocation failures map to
`CLM_HASH_ERROR_NO_MEMORY`.

Hex encoding takes an arbitrary input byte count. Its output capacity must be
at least `2 * input_length + 1`; invalid sizes or insufficient capacity fail
before any output is written.

## 6. Integration and dependencies

The C header depends on `clm_base` for shared types and on `clm_string` for
string-ID adapters and UTF-8 validation. Applications using CLM string IDs
must initialize the string/BPA implementation according to those modules'
normal requirements.

`clm/hash.h` includes the freehash declarations, and the C build must compile
and link `clm/ext/freehash.c`. Hash descriptors are selected directly by CLM's
algorithm mapping; callers do not need to register freehash algorithms in its
global descriptor table. The vendored freehash files are available under the
upstream project's public-domain/unlicense terms; see
`clm/ext/freehash.license.md` for the included license notice.

## 7. Security guidance

The module is a digest interface, not a password-storage or message-
authentication protocol. MD2, MD4, MD5, and SHA1 are legacy algorithms and
must not be selected for new collision-resistant designs. A general-purpose
digest alone does not provide authenticity, secrecy, or password-hardening.
Use a vetted keyed construction for authentication and a dedicated password
hashing scheme for password storage. Algorithm choice and backend
availability must be evaluated by the application for its security needs.
