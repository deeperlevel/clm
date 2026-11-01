# CLM Hash

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Purpose

Computes supported message digests and provides hash formatting/conversion utilities.

## Capabilities and operations

- Hash binary data and text using declared supported algorithms.
- Use digest metadata and hexadecimal encoders for portable interchange.
- Do not treat ordinary fast hashes as password hashes or message-authentication substitutes.
- Compare byte sequences with security-appropriate methods when required.

## Detailed module contract

**Module:** `clm_hash`  
**Scope:** Hash-algorithm metadata, byte/text adapters, and digest formatting.

## 1. Purpose and current status

`clm_hash` defines a common contract for hashing byte sequences and strings. It
separates the digest (binary bytes) from its optional lowercase hexadecimal
representation, and offers adapters for caller-owned buffers and CLM string
IDs.

Algorithms are selected using CLM-level identifiers, not backend-specific indices. Digest operations return binary bytes or lowercase hexadecimal as requested.

## 2. Data and text model

Hash input is a sequence of bytes. Hashing does not normalize text, change
letter case, remove whitespace, or add a terminator. For a NUL-terminated text
input, the terminator is excluded. For an explicit byte sequence, every byte
within the specified length is included, including zero-valued bytes.

CLM strings contain validated UTF-8. A CLM-string adapter hashes the string's
UTF-8 bytes, not its character count and not a converted external encoding.
Text-oriented adapters validate UTF-8 when required; raw byte adapters preserve the exact input bytes.

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

The reference implementation maps every listed algorithm to its freehash descriptor.
Unknown identifiers have no digest size and are reported as unsupported by
digest operations.

## 4. Operations

The module API expresses the operations below as inline functions; other language
ports may use native functions or methods while preserving the same observable
behavior.

| Operation | Input | Output |
|---|---|---|
| Digest-size query | Algorithm identifier | Binary digest size, or zero when unknown |
| Hex-size query | Algorithm identifier | Required hex-buffer size including terminator, or zero when unknown |
| Algorithm-name lookup | Algorithm identifier and an existing CLM string ID | Display name stored in that string |
| Hex encoding | Arbitrary bytes | Lowercase hexadecimal text in a caller-owned buffer |
| `bytes` | Arbitrary bytes and explicit length | Binary digest in a caller-owned buffer |
| `bytes_as` | NUL-terminated null-terminated text, interpreted as bytes | Binary digest in a caller-owned buffer |
| `bytes_from` | CLM string ID | Binary digest in a caller-owned buffer |
| `bytes_to` | Arbitrary bytes and explicit length | Hex digest stored in an existing CLM string |
| `string_as` | NUL-terminated UTF-8 null-terminated text | Hex digest in a caller-owned character buffer |
| `string_to` | NUL-terminated UTF-8 null-terminated text | Hex digest stored in an existing CLM string |
| `string_from` | Source and destination CLM string IDs | Hex digest stored in the destination string |

The suffix conventions are:

- **`_as`**: accept a null-terminated text directly.
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
requesting computation. The reference implementation computes into temporary storage
and copies the digest to caller output only on success. String-output adapters
validate the destination before hashing and replace its content only after
successful computation and formatting. Backend errors map to
`CLM_HASH_ERROR_BACKEND`; backend allocation failures map to
`CLM_HASH_ERROR_NO_MEMORY`.

Hex encoding takes an arbitrary input byte count. Its output capacity must be
at least `2 * input_length + 1`; invalid sizes or insufficient capacity fail
before any output is written.

## 6. Integration and dependencies

The module depends on `clm_base` for shared types and on `clm_string` for
string-ID adapters and UTF-8 validation. Applications using CLM string IDs
must initialize the string/BPA implementation according to those modules'
normal requirements.

Algorithm registration with a third-party backend is an implementation detail; CLM consumers use the documented CLM algorithm selectors.

## 7. Security guidance

The module is a digest interface, not a password-storage or message-
authentication protocol. MD2, MD4, MD5, and SHA1 are legacy algorithms and
must not be selected for new collision-resistant designs. A general-purpose
digest alone does not provide authenticity, secrecy, or password-hardening.
Use a vetted keyed construction for authentication and a dedicated password
hashing scheme for password storage. Algorithm choice and backend
availability must be evaluated by the application for its security needs.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Related CLM modules

**Supporting modules:** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md), [clm_string](clm_string.md).

## Example use cases

Hash binary data and text using declared supported algorithms.
