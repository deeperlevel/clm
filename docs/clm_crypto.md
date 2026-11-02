# CLM Crypto

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md), [clm_string](clm_string.md).
- **External libraries / platform facilities (C reference):** Monocypher (bundled in `ext/`); OS entropy via Windows BCrypt or `/dev/urandom` on Unix-like systems.

## Purpose

Cryptographic primitives for authenticated encryption, key material and secure random operations.

## Capabilities and operations

- Encrypt/decrypt with authenticated algorithms and verify authentication before accepting plaintext.
- Generate cryptographically secure random bytes and use explicit encoding/decoding helpers where provided.
- Keep keys and nonces governed by algorithm-specific requirements, including nonce uniqueness.
- Use designated clearing/erasure paths for secrets; do not confuse fast hashes with password hashing.

## Detailed module contract

**Module:** `clm_crypto`  
**Scope:** Authenticated encryption, secure randomness, password-based key
derivation, X25519 key agreement, and byte/text adapters.

## 1. Purpose and boundaries

This contract describes observable behavior independent of programming
language. Ports may use native facilities or another compatible cryptographic
backend while preserving algorithms, parameters, byte representations,
authentication behavior, and ownership rules.

The module is not a secure messaging protocol, TLS implementation, encrypted
file format, identity system, or secure-memory allocator. It does not negotiate
algorithms, manage encryption keys, prevent replay, track used nonces, or
serialize messages. Applications supply those policies.

The reference algorithm families include XChaCha20-Poly1305, Argon2id and X25519. Cryptographic randomness must use an operating-system-backed secure source (or an equivalent vetted cryptographic source), not an ordinary pseudorandom generator. When a secure source is unavailable, return a failure rather than silently weakening security.

## 2. Algorithms and representations

### Authenticated encryption

`CLM_CRYPTO_ALGORITHM_XCHACHA20_POLY1305` is the only supported encryption
identifier. It requires:

| Value | Size | Treatment |
|---|---:|---|
| Secret key | 32 bytes | Secret; securely erase when no longer needed |
| Nonce / IV | 24 bytes | Public; never reuse with the same key |
| Authentication tag | 16 bytes | Public; preserve without alteration |
| Ciphertext | Same byte count as plaintext | Public encrypted content |
| Associated data (AAD) | Caller-selected length | Authenticated but not encrypted |

Unknown identifiers have zero key, nonce, and tag sizes in metadata queries.
Algorithm-name lookup writes `XChaCha20-Poly1305` to an existing CLM string.

Encryption returns ciphertext and tag separately. It does not prepend the
nonce, append the tag, add a version field, or frame a network message. The
application must preserve the nonce and tag alongside the ciphertext and
provide exactly the same AAD when decrypting.

Every encryption under a particular key needs a fresh nonce. Random nonce
generation uses the OS CSPRNG but does not remember previous values or provide
an absolute uniqueness guarantee. Never use deterministic test nonces in
production. Decryption alone does not detect replay of an authentic message.

### Bytes and strings

- Explicit-length byte operations preserve all bytes, including embedded NUL
  and non-UTF-8 values. They do not append a terminator.
- C-string plaintext inputs end at the first NUL; the terminator is not encrypted.
- CLM strings hold UTF-8 bytes. No normalization or preferred-encoding export
  takes place before encryption or password derivation.
- Text encryption produces lowercase hexadecimal ciphertext, not arbitrary
  binary string content. The tag and nonce remain separate binary values.
- Text decryption authenticates first, then rejects invalid UTF-8 and embedded
  NUL. Callers handling binary plaintext must use byte-output operations.
- Caller-owned C text output includes a trailing NUL. CLM string content
  excludes its internal terminator.

## 3. Ownership and failure guarantees

Inputs are borrowed only for the duration of a call. Caller-owned output
buffers need adequate capacity, and pointer-based APIs assume every stated
input/output range is valid accessible memory.

ID adapters do not create or resize fixed-size BPA resources. String
destinations must already exist and have string type. Binary block operations
require existing non-string blocks; paired plaintext/ciphertext blocks must
have equal sizes and different IDs. String-to-string encryption and decryption
also require distinct source and destination IDs.

Raw encryption and decryption reject overlapping input/output ranges rather
than offering in-place operation. C text and hex adapters likewise reject
prohibited overlap. Treat keys, nonces, AAD, tags, source data, and destinations
as separate application storage; ID adapters are not a general aliasing API.

Byte decryption stages plaintext privately and publishes it only after
successful authentication. Failed authentication leaves the caller's output
unchanged. Text adapters additionally validate the plaintext representation
before replacing the destination. Encryption adapters commit their
ciphertext/tag results only after their validation and staging succeed.
Random generation also stages output so a source failure does not expose
partially generated results.

Temporary sensitive buffers are wiped before release. Ordinary BPA and CLM
strings are **not secure memory**: earlier contents, caller-owned copies,
swap, crash dumps, and unrelated application buffers are not erased by this
module. Secure erasure affects only the supplied writable range; deleting an
ID is not a substitute for wiping its secret payload first.

CLM string and BPA access is not internally synchronized. Callers must
coordinate ID operations and must not mutate or delete borrowed resources
concurrently. Independent byte operations use per-call state, but sharing
caller-owned buffers still requires synchronization.

## 4. Encryption operation families

The operation names below omit the `clm_crypto_` prefix. Other ports may use
native names while preserving these input and output semantics.

| Operation | Input | Output |
|---|---|---|
| `encrypt_bytes` | Explicit-length plaintext bytes | Binary ciphertext and tag |
| `decrypt_bytes` | Binary ciphertext and tag | Authenticated plaintext bytes |
| `encrypt_bytes_as` | NUL-terminated null-terminated text, no UTF-8 validation | Binary ciphertext and tag |
| `encrypt_bytes_from` | CLM plaintext string ID | Binary ciphertext and tag |
| `encrypt_bytes_to` | Explicit-length plaintext bytes | Ciphertext hex in an existing string ID; binary tag |
| `decrypt_bytes_as` | NUL-terminated ciphertext hex and binary tag | Plaintext bytes |
| `decrypt_bytes_from` | Ciphertext hex string ID and binary tag | Plaintext bytes |
| `encrypt_string_as` | NUL-terminated UTF-8 plaintext | Caller-owned ciphertext hex and binary tag |
| `decrypt_string_as` | NUL-terminated ciphertext hex and binary tag | Caller-owned NUL-terminated UTF-8 plaintext |
| `encrypt_string_to` | NUL-terminated UTF-8 plaintext | Ciphertext hex string ID and binary tag |
| `decrypt_string_to` | NUL-terminated ciphertext hex and binary tag | Existing UTF-8 plaintext string ID |
| `encrypt_string_from` | UTF-8 plaintext string ID | Different ciphertext hex string ID and binary tag |
| `decrypt_string_from` | Ciphertext hex string ID and binary tag | Different UTF-8 plaintext string ID |
| `encrypt_from` | Fixed-size plaintext BPA block | Different equal-size ciphertext BPA block and binary tag |
| `decrypt_from` | Fixed-size ciphertext BPA block and binary tag | Different equal-size plaintext BPA block |

All encryption/decryption operations also take the algorithm, key, nonce, and
optional AAD. Byte lengths are not Unicode character counts. Empty messages
are valid and still have an authentication tag. Zero-length AAD may be absent.
The byte APIs permit absent zero-length message/output buffers; C text APIs
still require a valid string pointer and room for their terminator.

The suffixes denote adapter direction: `_as` uses C text, `_from` reads ID
storage, and `_to` writes ID storage. Inspect the operation family to determine
whether that text represents plaintext or ciphertext hex.

## 5. Randomness, erasure, and hex utilities

- `random_bytes` fills exactly the requested output range from the OS CSPRNG.
  An absent output is valid only for a zero-length request.
- `key_generate` fills 32 bytes for the supported algorithm; `nonce_generate`
  fills 24 bytes. `iv_generate` is an alias for nonce generation.
- `random_to` fills an entire existing non-string BPA block.
  `key_generate_to` and `nonce_generate_to` require exactly 32-byte and
  24-byte blocks respectively.
- `secure_zero` uses the backend's explicit wipe primitive to erase a writable
  byte range. An absent pointer is valid only for a zero-length range.
- `hex_size(n)` returns `2*n + 1`, or zero on size overflow.
- `hex_encode` writes lowercase hex plus a terminator. `hex_decode` accepts
  either letter case but no prefix, whitespace, separators, odd length, or
  other characters. Decoding can optionally return the decoded byte count;
  that count is written only on success. Empty hex represents zero bytes.
  Hex source/destination buffers must not overlap.

## 6. Password-based derivation

`derive_key_bytes` uses **Argon2id version 1.3**, one lane, with no optional
secret key or extra associated data. Its parameters are:

| Parameter | Contract |
|---|---|
| Password | Explicit bytes; may be empty; at most 2^32 - 1 bytes |
| Salt | At least 16 bytes, at most 2^32 - 1 bytes |
| Memory cost | At least 8 KiB, expressed in 1024-byte blocks |
| Pass count | Positive 32-bit count |
| Derived-key length | At least 4 bytes, at most 2^32 - 1 bytes |

Work memory must fit the platform address space and available memory.
Monocypher rounds its effective block count down to a multiple of four for
the single-lane configuration. Preserve the requested parameters and output
length for reproducibility.

`derive_key_as` accepts password bytes up to the first C-string NUL.
`derive_key_from` reads UTF-8 bytes from a CLM string ID.
`derive_key_to` takes password bytes and writes to an existing non-string BPA
block whose size determines the derived-key length.

Generate a fresh random salt for each independently protected password-derived
key, and store it with the parameters and algorithm version. The minimum
accepted costs are validation floors, **not recommended deployment settings**.
Choose costs using a measured memory/latency budget and contemporary password
security guidance. Enforce resource limits on untrusted requests: this
synchronous operation can deliberately consume substantial memory and CPU.
Do not run it directly on a latency-sensitive event loop.

## 7. X25519 key agreement

`x25519_key_pair` generates a 32-byte private key using OS randomness and
computes its 32-byte public key. Output ranges must be distinct.
`x25519_shared` accepts a private key and peer public key and produces a
32-byte raw shared secret. It rejects an all-zero result with an authentication
error and leaves output unchanged. Inputs are fixed-width values; callers
must provide the full 32 bytes even though these input signatures have no
explicit length parameters.

X25519 does not authenticate peer identities. Its raw output is **not a
ready-to-use AEAD key**. Use an appropriate transcript-bound session-key KDF,
bind both public keys and protocol context, authenticate peers, and wipe
ephemeral secrets. This module does not supply that protocol or session KDF.
The Argon2id interface is for password derivation, not a replacement for a
protocol-specific session-key schedule.

No signing or signature-verification API is currently exposed. In particular,
Monocypher core's BLAKE2b-based EdDSA is not standard Ed25519; the adapter
deliberately avoids presenting it as interoperable Ed25519. The declared
EdDSA size constants do not imply signature support.

## 8. Results

| Result | Value | Meaning |
|---|---:|---|
| `SUCCESS` | 0 | Completed successfully |
| `ERROR_INVALID_ARGUMENT` | -1 | Invalid pointer, length, resource type/ID, or prohibited overlap |
| `ERROR_UNSUPPORTED_ALGORITHM` | -2 | Unknown encryption identifier |
| `ERROR_BUFFER_TOO_SMALL` | -3 | Insufficient caller-owned output capacity |
| `ERROR_BACKEND` | -4 | OS randomness or underlying storage operation failed |
| `ERROR_AUTHENTICATION` | -5 | Ciphertext authentication failed or X25519 returned all zero |
| `ERROR_NO_MEMORY` | -6 | Staging or derivation allocation failed |
| `ERROR_INVALID_TEXT` | -7 | Malformed hex or plaintext incompatible with UTF-8 string output |

Status identifiers use the `CLM_CRYPTO_` prefix. Some composite ID adapters classify
an inadequate tag destination as an invalid argument rather than a buffer-size
error. Error-check order is not standardized across operation families.
Metadata-size queries return a size or zero, rather than a status.

## 9. Portability and backend choice

Each CLM language implementation may bind to a suitable vetted cryptographic backend while preserving algorithm names, input/output formats, authentication and error behavior. Vendor code and cryptographic libraries retain their own licensing terms; their source is not part of the language-neutral contract. Review the selected backend's current security guidance before deploying it.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- Algorithm/primitive selection should be documented by each language port; third-party cryptography remains subject to its own license.

## Example use cases

Encrypt/decrypt with authenticated algorithms and verify authentication before accepting plaintext.
