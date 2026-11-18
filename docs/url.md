# clm_url

> Language-neutral capability overview. C implementation details are documented separately.

## Purpose

URL/URI/IRI parsing, normalization, resolution, and component manipulation.

## Behavior and constraints

Provides URL-specific string operations analogous in spirit to filesystem operations in the path module, but following URL/URI syntax instead of filesystem conventions. Functions include parsing and validation; reading, replacing and joining URL components; resolving relative references; working with path segments and query parameters; percent/form encoding; and converting file URLs to and from filesystem paths.

The supplied C implementation targets RFC 3986 syntax and resolution rather than implicitly following browser WHATWG URL behavior. These operations manipulate URL text and do **not** perform network requests or DNS lookups. The language implementation determines how the URL text and parsed components are represented.

## Integration

- Related capabilities: text/string operations, URI parsing, optional IDNA support.
- [C reference](../c/url.md) — signatures, concrete types, backend requirements, and error codes for one implementation.
- Capability availability can vary by language implementation and platform.
