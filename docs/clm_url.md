# CLM Url

> **Current module** · Language-neutral capability and behavioral reference · Source snapshot: 2026-11-01 (55-module snapshot)

## Dependencies

- **CLM modules (C reference):** [clm_base](clm_base.md), [clm_bpa](clm_bpa.md), [clm_path](clm_path.md), [clm_platform](clm_platform.md), [clm_string](clm_string.md).
- **External libraries / platform facilities (C reference):** uriparser (URI processing); libidn2 (non-Windows IDNA); native Windows IDN APIs on Windows.

## Purpose

URI, URL and IRI parsing, reconstruction and normalization utilities.

## Capabilities and operations

- Parse scheme, authority, user info, host, port, path, query and fragments.
- Resolve relative references, normalize parts and inspect origins.
- Handle percent encoding and internationalized identifiers through appropriate rules.
- Distinguish URL syntax from filesystem paths; normalization is not a security policy.

## Detailed module contract

## Status and Scope

This is a **language-neutral API catalog** for URI, URL, and IRI manipulation.
The reference binding implements all 63 listed operations using uriparser for RFC 3986
syntax, normalization, and resolution. Cross-language policy remains a draft
where bindings can differ; references below to pending policies concern that
broader specification. Current C behavior is defined in the binding contract
section and supersedes those draft notes for C callers.

The module handles syntax, components, reference resolution, encoding, query
fields, and file-URL conversion. It does not perform HTTP requests, DNS resolution,
TLS validation, connectivity checks, or filesystem existence checks. Filesystem
path manipulation belongs to [CLM Path](clm_path.md).

The planned parser and reference resolver target
[RFC 3986](https://www.rfc-editor.org/rfc/rfc3986). Internationalized references
are addressed separately using [RFC 3987](https://www.rfc-editor.org/rfc/rfc3987).
Browser-style WHATWG URL behavior must not be assumed implicitly. Scheme-specific
host, origin, and file-URL policies require additional explicit decisions.

## Language-Neutral Model

### Portable behavior

- Text arguments and output destinations are existing BPA string IDs. Operations
	return 0 on success and 1 on failure; predicates return 0/1 or -1 on error.
	Syntax validators return false for malformed text and -1 for invalid string IDs.
- Mutations preserve outputs on failure and support source/destination aliasing.
	`query_get_at` requires different key and value output IDs; either can alias the
	source. Scalar outputs are assigned only on success. Successful text writes
	select UTF-8 export encoding.
- `CLM_URL_ABSENT_ID` represents omitted optional builder components. A present
	empty string is distinct from an absent component. The builder path must always
	refer to a string. Missing component extraction fails without writing output.
- Generic setters accept encoded text, validate the resulting URI and component,
	and reject delimiter injection. Removing a nonexistent optional component is
	a no-op; host removal is rejected. Username removal while a password remains
	is rejected because the user-information syntax still requires a username slot.
- Port getters reject absent, empty, nonnumeric, and out-of-range explicit ports.
	Default ports are HTTP/WS 80, HTTPS/WSS 443, FTP 21, SSH 22, and Gopher 70.
	Unknown defaults fail; removing an unknown scheme's default port is a no-op.
- Tuple origins support HTTP, HTTPS, WS, WSS, and FTP. They serialize with an
	explicit effective port; unsupported or opaque origins fail. Origins do not
	perform network or credential checks.
- Host literal helpers use strict IPv4 and bracketed IPv6 syntax. IPv6 zone
	identifiers are not accepted. IDNA conversion uses Windows Normaliz with STD3
	ASCII rules, or libidn2 with nontransitional/STD3 processing on POSIX. Native
	IDNA versions can differ; identical Unicode mapping across hosts is not promised.
- IRI conversion validates UTF-8 scalar ranges and component context, applies
	IDNA to internationalized hosts, and percent-encodes non-ASCII data. Already
	ASCII URI registered names are not subjected to additional DNS restrictions.
	URI-to-IRI conversion decodes valid non-ASCII escapes, preserving reserved ones.
- Relative-reference generation falls back to an absolute reference when needed
	for incompatible authorities. Both target and base must have schemes.
- `normalize` uses uriparser syntax normalization without sorting queries or
	removing default ports. `equals` compares that normalized text, not resources.
	Dot-segment removal also normalizes the path's percent escapes through the
	library path-normalization operation. Percent-only normalization does not
	remove dot segments or lowercase hosts.
- Encoders treat literal percent signs as data. Component encoding supports
	scheme, user information, username, password, path, query, and fragment;
	authority/host/port use their dedicated validation or conversion APIs.
	Segment and query-value encoding retain only unreserved characters. Form
	helpers additionally convert space to plus and plus back to space; line breaks
	are encoded as-is rather than normalized.
- A zero-length path and `/` have zero segments. Other paths preserve repeated
	and trailing empty segments; indices are zero-based. Segment mutation encodes
	decoded values. Non-hierarchical scheme paths reject segment tooling.
- Filename/extension extraction keeps encoded text; trailing slash gives an empty
	filename. Literal dots, not encoded dots, delimit extensions, which include
	the leading dot. Parent URLs drop query and fragment; root is preserved.
	Extension changes accept decoded text and an optional leading dot.
- Queries use raw ordered ampersand-delimited fields. Absent and present-empty
	queries contain zero fields; interior/trailing empty fields otherwise remain.
	Bare-key retrieval returns empty text; `query_get_at` exposes equals presence.
	Append retains duplicates; set replaces all matches at the first match position
	or appends if absent; removing the final field removes the query delimiter.
	Stable sorting compares encoded key bytes and preserves no-op empty queries.
	When `has_value` is false, append/set ignore the value ID.
- Redaction keeps only scheme and authority host/port with root path, stripping
	credentials, path data, query, and fragment. References without such an authority
	become `[redacted]`. Hostnames themselves can still be sensitive.
- File URLs must have absolute paths and no query, fragment, credentials, or port.
	Encoded NUL, slash, and backslash are rejected. Localhost is treated as local;
	other hosts are supported only as native Windows UNC paths. Extended Windows
	device paths are rejected rather than silently converted. Conversion does not
	access the filesystem or establish containment.

CMake fetches pinned uriparser 0.9.9 and links it statically. Standalone consumers
must link uriparser and native IDNA support; POSIX builds require libidn2.
Windows has been runtime-tested; the POSIX branch requires separate runtime
validation. Other language bindings may use different libraries but must declare
any semantic differences rather than implying browser compatibility.

A **URL ID** identifies text containing a URI reference. Component, hostname,
path-segment, query-key, and query-value arguments also identify text; they are
not necessarily complete URLs. Bindings may expose native strings or other text
references, but must make representation and ownership rules explicit.

The current design uses UTF-8 text, byte-based parsed spans, and zero-based
indices. The specification does not require C pointers, structure layouts,
memory allocators, or an object-oriented interface.

- **Input** references are inspected without changing their text.
- **In place** replaces the text associated with the primary URL reference.
- **Output** denotes a separate text destination or scalar/record result.
- **Predicate** denotes a conceptual Boolean, distinct from an operation status.
- **Encoded** text retains percent escapes and is not automatically form-decoded.
- **Decoded** text is ordinary UTF-8 data to encode for a particular context.

Bindings choose result codes, exceptions, or native result types. Invalid input
must not be silently interpreted as a negative classification, an empty component,
or port zero. Predicate error representation, destination creation, aliasing,
failure atomicity, and detailed optional-component outcomes remain undecided.
Numeric C status constants do not define cross-language Boolean values.

## Components and Parsing

```text
https://user:password@example.com:8443/reports/annual.pdf?year=2026#summary
```

| Component | Text in the example |
| --- | --- |
| Scheme | `https` |
| Authority | `user:password@example.com:8443` |
| User information | `user:password` |
| Username | `user` |
| Password | `password` |
| Host | `example.com` |
| Port | `8443` |
| Path | `/reports/annual.pdf` |
| Query | `year=2026` |
| Fragment | `summary` |

Username/password subdivision is an API convention over user information, not
the only possible scheme interpretation of that field. Password-bearing URLs
are discouraged because text can leak through logs and other interfaces.

The component selector covers scheme, authority, user information, username,
password, host, port, path, query, and fragment. Generic extraction omits the
selected component's surrounding syntax, such as the scheme colon or query
question mark; IPv6 host brackets are retained. The path's own leading slash is
part of its value, not an external delimiter to discard.

Each parsed component span records:

| Field | Meaning |
| --- | --- |
| `offset` | Zero-based offset into the original UTF-8 bytes; meaningful when present. |
| `length` | Number of bytes in the selected component, excluding its surrounding delimiters. |
| `present` | Whether the component exists, even when its length is zero. |

Offsets do not count Unicode characters or decoded percent-escape values.
Reparse after changing the source text; old spans no longer describe the modified
text reliably. A missing query and a present empty query differ: `/report` has no
query, whereas `/report?` has one with zero characters.

## Parsing and Classification API

All `source` arguments below refer to complete URI/IRI references unless noted.

| Operation | Parameters | Intended result |
| --- | --- | --- |
| `clm_url_parse` | `source, parts` | Populate output component spans without modifying source text. |
| `clm_url_is_valid` | `source` | Predicate: valid RFC 3986 URI-reference syntax. |
| `clm_url_is_valid_iri` | `source` | Predicate: valid RFC 3987 internationalized reference syntax. |
| `clm_url_is_absolute` | `source` | Predicate: a valid reference has a scheme; see the terminology note below. |
| `clm_url_is_relative` | `source` | Predicate: a valid reference has no scheme. |
| `clm_url_is_network_path_reference` | `source` | Predicate: scheme-relative authority syntax beginning with `//`. |
| `clm_url_is_http` | `source` | Predicate: scheme is HTTP, compared case-insensitively. |
| `clm_url_is_https` | `source` | Predicate: scheme is HTTPS; not a TLS security test. |
| `clm_url_is_file` | `source` | Predicate: scheme is file; not a regular-file existence test. |

The current `is_absolute` stub means **scheme present**, including when a fragment
is present. RFC 3986's narrower `absolute-URI` grammar excludes fragments; the
name and documentation must retain this distinction or be revised explicitly.
A valid reference need not have an authority or use HTTP: `mailto:team@example.com`
is not a hierarchical web URL.

## Component API

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_url_has_component` | `source, component` | Predicate: component is present, independently of its length. |
| `clm_url_get_component` | `source, component, destination` | Copy encoded component text to an output destination. |
| `clm_url_set_component` | `url, component, value` | Validate encoded replacement text and update the URL in place. |
| `clm_url_remove_component` | `url, component` | Remove a supported optional component and its surrounding delimiters in place. |
| `clm_url_build` | `scheme, authority, path, query, fragment, destination` | Assemble validated encoded components into an output URI reference. |

Use the generic component family rather than assuming separate named getters
for every selector. Callers must not use a whole-URL encoder as a replacement for
component-specific validation. Assembly still needs a binding-neutral convention
for absent versus present-empty optional inputs; an empty string cannot represent
both without additional information.

## Ports, Hosts, and Origins

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_url_get_port` | `source, port` | Parse an explicit numeric port from a complete URI into output `port`. |
| `clm_url_set_port` | `url, port` | Set a numeric port in place after range validation. |
| `clm_url_get_default_port` | `scheme, port` | Look up a documented scheme default into output `port`; input is a scheme name without colon. |
| `clm_url_get_effective_port` | `source, port` | Return an explicit port or known scheme default into output `port`. |
| `clm_url_remove_default_port` | `url` | Remove an explicit port only when it matches the documented scheme default. |
| `clm_url_host_is_ipv4` | `source` | Predicate on an isolated host component: strict IPv4 literal syntax. |
| `clm_url_host_is_ipv6` | `source` | Predicate on an isolated bracketed IPv6 host component. |
| `clm_url_host_to_ascii` | `source, destination` | Convert an isolated internationalized hostname to an output ASCII hostname. |
| `clm_url_host_to_unicode` | `source, destination` | Convert an isolated validated IDNA hostname to output Unicode text. |
| `clm_url_get_origin` | `source, destination` | Serialize a supported tuple origin into output text. |
| `clm_url_is_same_origin` | `left, right` | Predicate comparing supported tuple origins of two complete URIs. |

Numeric port helpers accept values from 0 through 65535. This is a helper-level
constraint, not a claim that generic RFC 3986 port syntax alone enforces that
numeric range. Missing, empty, invalid, and unknown-default ports must not be
conflated. Port zero is a value, not an error sentinel.

Hostname conversion requires a selected IDNA version and processing policy.
Unicode display is not protection against visually confusable names. IPv6 zone
identifiers, IPvFuture forms, and nonstandard numeric-host compatibility need
explicit decisions rather than ad hoc colon splitting.

A supported tuple origin is based on scheme, canonical host, and effective port,
not user information, path, query, or fragment. The initial helpers reject
unsupported and opaque origins rather than inventing tuple values. Supported
schemes and canonicalization rules remain to be specified; file URLs must not
automatically receive web-origin semantics.

## Resolution and Normalization API

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_url_resolve` | `base, reference, destination` | Resolve a reference against an absolute base using RFC 3986 into output text. |
| `clm_url_get_relative_reference` | `source, base, destination` | Produce an output reference that resolves back to the target source. |
| `clm_url_normalize` | `url` | Normalize in place under a documented URI policy without implicit query sorting. |
| `clm_url_remove_dot_segments` | `url` | Remove hierarchical path dot segments in place using RFC 3986 rules. |
| `clm_url_normalize_percent_encoding` | `url` | Normalize escape hex case and decode permitted unreserved characters in place. |
| `clm_url_equals` | `left, right` | Predicate comparing URI text under the selected normalization policy. |

Resolution is not string concatenation or filesystem joining. The trailing slash
of a base URL determines whether its final segment is replaced or treated as a
directory context. Cross-authority relative-reference behavior remains pending.

Do not decode reserved characters such as an encoded slash while normalizing:
`a%2Fb` must not silently become two path segments. Do not equate normalization
with server resource identity. Even syntax-preserving changes can invalidate
signature schemes that cover the original request text.

## Encoding and IRI API

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_url_iri_to_uri` | `source, destination` | Convert an IRI reference to an output ASCII URI reference. |
| `clm_url_uri_to_iri` | `source, destination` | Convert permitted URI escapes and host text to an output IRI representation. |
| `clm_url_encode_component` | `source, component, destination` | Encode decoded UTF-8 data for a selected component context. |
| `clm_url_decode_component` | `source, destination` | Decode exactly one layer of component percent escapes; keep literal plus unchanged. |
| `clm_url_encode_path_segment` | `source, destination` | Encode decoded UTF-8 as one segment, including escaping a literal slash. |
| `clm_url_encode_query_value` | `source, destination` | Encode decoded UTF-8 as one query value without form plus-for-space rules. |
| `clm_url_form_encode` | `source, destination` | Encode one form field with spaces represented by plus and literal plus escaped. |
| `clm_url_form_decode` | `source, destination` | Decode one form field, treating plus as space. |

All destinations are output text references. The field encoders do not assemble
complete URLs or complete form documents. An encoder must know its context:
characters safe in a whole path may not be safe in one segment or query value.
Supported generic encoding contexts and existing-percent-sign treatment remain
to be finalized.

Decoded text must reject malformed escapes, embedded NUL, and invalid UTF-8 under
the planned text model. Parse structure before decoding individual components;
decoding an entire URL first can turn escaped data into structural delimiters.
Avoid repeated decoding: `%252F` becomes `%2F` after one pass, not `/`.

## Hierarchical Path API

These operations act on URL paths, not native filesystem paths. Segment setters
take **decoded** text; segment and filename getters return **encoded** text.

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_url_get_path_segment_count` | `source, count` | Populate output segment count under a root/empty-segment policy still pending. |
| `clm_url_get_path_segment` | `source, index, destination` | Copy one indexed encoded segment into output text. |
| `clm_url_set_path_segment` | `url, index, value` | Encode a decoded value and replace one segment in place. |
| `clm_url_append_path_segment` | `url, value` | Encode a decoded value and append one segment in place. |
| `clm_url_remove_path_segment` | `url, index` | Remove one segment in place, retaining valid URI structure. |
| `clm_url_get_parent` | `source, destination` | Write a hierarchical parent URI; query/fragment retention policy is pending. |
| `clm_url_get_filename` | `source, destination` | Write the final encoded path segment, excluding query and fragment text. |
| `clm_url_get_extension` | `source, destination` | Write the final path-segment extension under a dot and encoding policy still pending. |
| `clm_url_change_extension` | `url, extension` | Change the final segment extension in place without changing query or fragment. |
| `clm_url_ensure_ending_separator` | `url` | Ensure a hierarchical path ends in a slash, preserving query and fragment. |
| `clm_url_trim_ending_separator` | `url` | Trim a hierarchical path trailing slash under a root policy still pending. |

Indices are zero-based. Encoded slashes are not segment separators. Empty paths,
repeated slashes, trailing empty segments, dotfiles, encoded dots, and opaque or
non-hierarchical inputs require explicit policies. Appending a slash can change
the identified resource, unlike cosmetic formatting.

## Ordered Query API

Query-field operations interpret query text as ordered ampersand-delimited fields.
This is an explicit module convention: not every URI scheme assigns key/value
meaning to its query. Accessors take a complete URI, not an isolated query string.
Keys and values are encoded text and are not implicitly percent- or form-decoded.

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_url_query_count` | `source, count` | Populate output field count, preserving duplicates and empty values. |
| `clm_url_query_get_at` | `source, index, key, value, has_value` | Populate encoded key/value outputs and whether equals was present. |
| `clm_url_query_has` | `source, key` | Predicate matching an exact, case-sensitive encoded key. |
| `clm_url_query_get` | `source, key, occurrence, destination` | Copy the value of one matching occurrence into output text. |
| `clm_url_query_append` | `url, key, value, has_value` | Append one validated encoded field in place without deduplicating. |
| `clm_url_query_set` | `url, key, value, has_value` | Replace all matching occurrences with one field at the first match position. |
| `clm_url_query_remove` | `url, key` | Remove all matching fields in place without reordering others. |
| `clm_url_query_remove_at` | `url, index` | Remove one indexed field in place. |
| `clm_url_query_sort` | `url` | Explicitly stable-sort encoded keys in place; ordering rules remain pending. |

Field indices and duplicate-key occurrence indices are zero-based. A bare key
such as `debug` differs from `debug=`: the latter includes equals and has an empty
value. `has_value` describes that syntactic distinction, not truthiness of the
value. When false on append/set, the value argument is not used as field text;
its required binding-level validity remains to be specified.

Empty-query and empty-field counts, bare-key retrieval, insertion of previously
absent keys, and removal of the final field still need decisions. Sorting is never
implicit: it can alter server semantics or invalidate signed URLs.

## Credentials and File URLs

| Operation | Parameters | Intended behavior |
| --- | --- | --- |
| `clm_url_strip_credentials` | `url` | Remove authority user information in place; preserve other components. |
| `clm_url_redact` | `source, destination` | Write a logging representation under a redaction policy still undefined. |
| `clm_url_from_file_path` | `path, destination` | Convert an absolute native path, including supported UNC forms, into output file-URL text. |
| `clm_url_to_file_path` | `source, destination` | Convert a supported file URL into output native path text. |

Credential stripping does not remove tokens from queries, paths, or fragments.
No generic redaction function can promise to detect every secret. Redaction
configuration and sensitive-field policy must be defined before implementation.

File-URL conversion is syntactic and does not check existence. Authority handling,
localhost, drive letters, supported UNC forms, query/fragment treatment, and
extended-path representability require explicit platform rules. Unsupported
authorities and encoded separators must not be silently converted into local paths.

## Illustrative Examples

These examples describe intended concepts, not executable bindings or verified
results from the unimplemented functions.

```text
base:       https://example.com/docs/index.html
reference:  ../images/logo.png
resolved:   https://example.com/images/logo.png
```

```text
decoded segment:      reports/annual
encoded one segment:  reports%2Fannual

encoded component:    a+b
component decoded:    a+b
form decoded:         a b
```

```text
query: tag=red&tag=blue&debug&empty=
field 0: key=tag,   value=red,  has_value=true
field 1: key=tag,   value=blue, has_value=true
field 2: key=debug,            has_value=false
field 3: key=empty, value="",   has_value=true
```

## Safety Boundaries

- Valid syntax does not authorize requests or establish that a host is safe.
- Same-origin classification does not itself implement an application's access policy.
- HTTPS scheme detection does not verify a certificate or encrypted connection.
- Hostname normalization and IDNA conversion do not prevent visual spoofing.
- URL checks do not replace SSRF defenses at the network connection and redirect boundaries.
- Avoid logging raw credentials or tokens, even when URLs are syntactically valid.
- Preserve query ordering and encoded delimiters unless the caller explicitly requests changes.
- File-URL conversion does not enforce filesystem containment or permissions.

## Decisions Required Before Implementation

1. Define operation errors, predicate errors, output validity, ownership, creation, and aliasing.
2. Finalize URI-reference versus absolute-URI terminology and scheme-specific validation.
3. Define absent/empty build arguments, component removal, and unsupported encoding contexts.
4. Select IDNA processing, Unicode rules, IPv6 zones, IPvFuture, and compatibility policies.
5. Document supported default ports, origin schemes, and unknown/opaque-origin handling.
6. Specify normalization, equality, relative-reference fallback, and signed-URL caveats.
7. Define path-segment counting, empty paths, dot/extension rules, and non-hierarchical inputs.
8. Define query empty-field rules, bare keys, missing matches, insertion, and sorting order.
9. Define redaction configuration and limits without implying comprehensive secret detection.
10. Define platform-specific file-URL conversion and raw-text/buffer binding conveniences.

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.

## Example use cases

Parse scheme, authority, user info, host, port, path, query and fragments.
