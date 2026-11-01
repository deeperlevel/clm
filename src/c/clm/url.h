#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

#include "base.h"
#include "bpa.h"
#include "string.h"
#include "platform.h"
#include "path.h"
#include <uriparser/Uri.h>
#if !defined(_WIN32)
#include <idn2.h>
#endif

/** @file
 * URI/IRI manipulation API using UTF-8 strings referenced by CLM IDs.
 * URL parsing and resolution target RFC 3986, not implicit WHATWG browser rules.
 * Output and in-place parameters describe referenced contents, not ID values.
 * Operations return 0 on success and 1 on error; predicates return 0/1 or -1
 * on error. Syntax validators return false for malformed text and -1 for invalid
 * string IDs. Outputs require existing string blocks and are unchanged on error.
 * Aliasing is supported except identical key/value outputs to query_get_at.
 * Encoded component text is preserved unless a function explicitly decodes it.
 * These operations do not perform HTTP requests, DNS lookups, or connectivity tests.
 * Uses uriparser for RFC 3986 syntax/resolution and native platform IDNA facilities.
 */

#define CLM_URL_OPERATION_SUCCESS 0
#define CLM_URL_OPERATION_FAILURE 1
#define CLM_URL_ABSENT_ID ((clm_id)~UINT64_C(0))

/** Selects a URI component for generic presence, extraction, and mutation APIs. */
typedef enum {
	CLM_URL_COMPONENT_SCHEME,
	CLM_URL_COMPONENT_AUTHORITY,
	CLM_URL_COMPONENT_USERINFO,
	CLM_URL_COMPONENT_USERNAME,
	CLM_URL_COMPONENT_PASSWORD,
	CLM_URL_COMPONENT_HOST,
	CLM_URL_COMPONENT_PORT,
	CLM_URL_COMPONENT_PATH,
	CLM_URL_COMPONENT_QUERY,
	CLM_URL_COMPONENT_FRAGMENT
} clm_url_component;

/** A component range in the original UTF-8 bytes, not a decoded character range. */
typedef struct {
	clm_uint offset; /**< Zero-based byte offset; meaningful only when present. */
	clm_uint length; /**< Byte length excluding surrounding component delimiters. */
	clm_int present; /**< Nonzero for a present component, including an empty one. */
} clm_url_span;

/** Parsed component spans; reparse after changing the source string's contents. */
typedef struct {
	clm_url_span scheme;
	clm_url_span authority;
	clm_url_span userinfo;
	clm_url_span username;
	clm_url_span password;
	clm_url_span host;
	clm_url_span port;
	clm_url_span path;
	clm_url_span query;
	clm_url_span fragment;
} clm_url_parts;

int clm_url_internal_parse(const char* text, UriUriA* uri);

clm_url_span clm_url_internal_range(const char* text, UriTextRangeA range);

int clm_url_internal_parts(const char* text, clm_url_parts* parts);

clm_url_span* clm_url_internal_component(clm_url_parts* parts, clm_url_component component);

char* clm_url_internal_substring(const char* text, size_t offset, size_t length);

char* clm_url_internal_splice(const char* text, size_t offset, size_t length,
										   const char* before, const char* value, const char* after);

char* clm_url_internal_edit(const char* text, clm_url_component component, const char* value);

int clm_url_internal_commit(clm_id destination, char* output);

int clm_url_internal_scheme(const char* text, const char* expected);

int clm_url_internal_hex(unsigned char character);

int clm_url_internal_unreserved(unsigned char character);

char* clm_url_internal_encode(const char* text, const char* allowed, int form);

char* clm_url_internal_decode(const char* text, int form, int unicode_only);

char* clm_url_internal_idna(const char* text, int ascii);

char* clm_url_internal_iri_ascii(const char* text);

char* clm_url_internal_serialize(UriUriA* uri);

char* clm_url_internal_normalized(const char* text, unsigned int mask);

int clm_url_internal_default_port(const char* text, size_t length, clm_uint* port);

int clm_url_internal_port(const char* text, clm_url_parts* parts, int effective, clm_uint* port);

char* clm_url_internal_origin(const char* text);

typedef struct {
	size_t offset;
	size_t length;
	size_t key_length;
	int has_value;
} clm_url_internal_field;

clm_url_internal_field* clm_url_internal_fields(const char* text, clm_url_span query, size_t* count);

int clm_url_internal_field_matches(const char* text, clm_url_internal_field field, const char* key);

int clm_url_internal_query_edit(clm_id url, int mode, const char* key, const char* value,
											 int has_value, clm_uint selected);

int clm_url_internal_segment(clm_id url, int mode, clm_uint selected,
										  const char* value, clm_id destination, clm_uint* count_result);

/** Parse a URI reference into spans over its original encoded text.
 * @param source ID of the URI-reference string; it is not modified.
 * @param[out] parts Caller-owned parsed result; must not be NULL.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @note Presence distinguishes absent components from present empty components.
 */
clm_int clm_url_parse(clm_id source, clm_url_parts* parts);

/** Validate URI-reference syntax according to RFC 3986.
 * @param source ID of the encoded URI-reference string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 * @note Syntactic validity does not establish safety, existence, or accessibility.
 */
clm_int clm_url_is_valid(clm_id source);

/** Validate internationalized resource identifier syntax according to RFC 3987.
 * @param source ID of the UTF-8 IRI-reference string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_is_valid_iri(clm_id source);

/** Test whether a syntactically valid reference contains a scheme.
 * @param source ID of the URI-reference string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_is_absolute(clm_id source);

/** Test whether a syntactically valid reference has no scheme.
 * @param source ID of the URI-reference string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_is_relative(clm_id source);

/** Recognize a scheme-relative reference with an authority introduced by two slashes.
 * @param source ID of the URI-reference string, not a Windows UNC path.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_is_network_path_reference(clm_id source);

/** Test for an HTTP scheme using case-insensitive scheme comparison.
 * @param source ID of the URI string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_is_http(clm_id source);

/** Test for an HTTPS scheme using case-insensitive scheme comparison.
 * @param source ID of the URI string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_is_https(clm_id source);

/** Test for a file scheme without accessing the filesystem.
 * @param source ID of the URI string to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_is_file(clm_id source);

/** Test component presence independently of whether its text is empty.
 * @param source ID of the URI-reference string.
 * @param component Component selector to inspect.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_has_component(clm_id source, clm_url_component component);

/** Extract a raw encoded component without its surrounding delimiters.
 * @param source ID of the URI-reference string.
 * @param component Component selector to extract.
 * @param[out] destination ID of the result string; IPv6 host brackets are retained.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_component(clm_id source, clm_url_component component, clm_id destination);

/** Insert or replace a component using already encoded, validated text.
 * @param[in,out] url ID of the URI string to update.
 * @param component Component selector to insert or replace.
 * @param value ID of the raw encoded replacement string without surrounding delimiters.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_set_component(clm_id url, clm_url_component component, clm_id value);

/** Remove an optional component together with its structural delimiters.
 * @param[in,out] url ID of the URI string to update.
 * @param component Component selector to remove.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_remove_component(clm_id url, clm_url_component component);

/** Assemble a URI reference from already encoded components.
 * @param scheme ID of the scheme string without a trailing colon.
 * @param authority ID of the authority string without its introducing slashes.
 * @param path ID of the encoded URI path string.
 * @param query ID of the query string without its introducing question mark.
 * @param fragment ID of the fragment string without its introducing hash mark.
 * @param[out] destination ID of the assembled result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @note Use CLM_URL_ABSENT_ID for absent optional components; path must name a string.
 */
clm_int clm_url_build(clm_id scheme, clm_id authority, clm_id path, clm_id query, clm_id fragment, clm_id destination);

/** Parse an explicit numeric port from a URI authority.
 * @param source ID of the URI string, not an isolated port string.
 * @param[out] port Caller-owned numeric result in the range 0 through 65535.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_port(clm_id source, clm_uint* port);

/** Insert or replace an explicit numeric authority port.
 * @param[in,out] url ID of the URI string to update.
 * @param port Numeric port in the range 0 through 65535.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_set_port(clm_id url, clm_uint port);

/** Look up a default port using a documented scheme mapping.
 * @param scheme ID of the scheme name string without a colon.
 * @param[out] port Caller-owned default numeric port result.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_default_port(clm_id scheme, clm_uint* port);

/** Obtain an explicit port or the URI scheme's documented default port.
 * @param source ID of the URI string.
 * @param[out] port Caller-owned effective numeric port result.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_effective_port(clm_id source, clm_uint* port);

/** Remove an explicit port when it equals the scheme's documented default.
 * @param[in,out] url ID of the URI string to update.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_remove_default_port(clm_id url);

/** Validate an isolated host component as a strict IPv4 address literal.
 * @param source ID of the host string, not an entire URI.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_host_is_ipv4(clm_id source);

/** Validate an isolated bracketed IPv6 host literal.
 * @param source ID of the bracketed host string, not an entire URI.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_host_is_ipv6(clm_id source);

/** Convert an internationalized hostname to an ASCII representation.
 * @param source ID of the Unicode hostname string, excluding port and user information.
 * @param[out] destination ID of the ASCII hostname result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_host_to_ascii(clm_id source, clm_id destination);

/** Convert a validated IDNA hostname to a Unicode representation.
 * @param source ID of the ASCII hostname string, excluding port and user information.
 * @param[out] destination ID of the Unicode hostname result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @warning Unicode display does not protect against visually confusable hostnames.
 */
clm_int clm_url_host_to_unicode(clm_id source, clm_id destination);

/** Serialize a supported tuple origin from scheme, canonical host, and effective port.
 * @param source ID of the URI string.
 * @param[out] destination ID of the serialized origin result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_origin(clm_id source, clm_id destination);

/** Compare supported tuple origins without comparing paths, queries, or fragments.
 * @param left ID of the first URI string.
 * @param right ID of the second URI string.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_is_same_origin(clm_id left, clm_id right);

/** Resolve a URI reference against an absolute base using RFC 3986 rules.
 * @param base ID of the absolute base URI string.
 * @param reference ID of the URI-reference string to resolve.
 * @param[out] destination ID of the resolved URI result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_resolve(clm_id base, clm_id reference, clm_id destination);

/** Derive a reference that resolves against a base back to the source URI.
 * @param source ID of the target URI string.
 * @param base ID of the absolute base URI string.
 * @param[out] destination ID of the relative-reference result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_relative_reference(clm_id source, clm_id base, clm_id destination);

/** Apply uriparser syntax normalization without sorting queries or removing default ports.
 * @param[in,out] url ID of the URI string to normalize.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @warning Normalization must not be assumed safe for signed URLs without their signing rules.
 */
clm_int clm_url_normalize(clm_id url);

/** Apply RFC 3986 dot-segment removal to a hierarchical URI path.
 * @param[in,out] url ID of the URI string to update.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_remove_dot_segments(clm_id url);

/** Normalize escape hex casing and decode only permitted unreserved characters.
 * @param[in,out] url ID of the URI string to update.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_normalize_percent_encoding(clm_id url);

/** Compare URI strings under the module's documented normalization policy.
 * @param left ID of the first URI string; not modified.
 * @param right ID of the second URI string; not modified.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 * @note Equality here cannot establish that servers return the same resource.
 */
clm_int clm_url_equals(clm_id left, clm_id right);

/** Convert an internationalized reference to its ASCII URI representation.
 * @param source ID of the UTF-8 IRI-reference string.
 * @param[out] destination ID of the encoded ASCII URI-reference result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_iri_to_uri(clm_id source, clm_id destination);

/** Convert permitted URI escapes and host text to an IRI representation.
 * @param source ID of the encoded URI-reference string.
 * @param[out] destination ID of the UTF-8 IRI-reference result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_uri_to_iri(clm_id source, clm_id destination);

/** Percent-encode UTF-8 text according to a selected component's allowed characters.
 * @param source ID of the unencoded input text string, not an entire URI.
 * @param component Target component context used to select encoding rules.
 * @param[out] destination ID of the encoded result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_encode_component(clm_id source, clm_url_component component, clm_id destination);

/** Decode one layer of percent escapes from isolated component data.
 * @param source ID of the percent-encoded text string; literal plus is not a space.
 * @param[out] destination ID of the decoded UTF-8 result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @warning Do not decode a whole URI before parsing its structural delimiters.
 */
clm_int clm_url_decode_component(clm_id source, clm_id destination);

/** Encode decoded text as exactly one URI path segment.
 * @param source ID of the decoded UTF-8 segment string.
 * @param[out] destination ID of the encoded result string, including escaped literal slashes.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_encode_path_segment(clm_id source, clm_id destination);

/** Encode decoded text as an individual query field value.
 * @param source ID of the decoded UTF-8 value string.
 * @param[out] destination ID of the encoded query-value result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_encode_query_value(clm_id source, clm_id destination);

/** Encode one field using application/x-www-form-urlencoded rules.
 * @param source ID of the decoded UTF-8 field string.
 * @param[out] destination ID of the encoded result string; spaces become plus signs.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_form_encode(clm_id source, clm_id destination);

/** Decode one application/x-www-form-urlencoded field.
 * @param source ID of the encoded field string, with plus interpreted as space.
 * @param[out] destination ID of the decoded UTF-8 result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_form_decode(clm_id source, clm_id destination);

/** Count encoded segments in a hierarchical URI path.
 * @param source ID of the URI string, not an isolated decoded path.
 * @param[out] count Caller-owned segment-count result.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_path_segment_count(clm_id source, clm_uint* count);

/** Extract an encoded hierarchical path segment by index.
 * @param source ID of the URI string.
 * @param index Zero-based segment index under the module's segment-counting policy.
 * @param[out] destination ID of the encoded segment result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_path_segment(clm_id source, clm_uint index, clm_id destination);

/** Encode decoded text and replace one hierarchical path segment.
 * @param[in,out] url ID of the URI string to update.
 * @param index Zero-based segment index to replace.
 * @param value ID of the decoded UTF-8 replacement segment string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_set_path_segment(clm_id url, clm_uint index, clm_id value);

/** Encode decoded text and append exactly one hierarchical path segment.
 * @param[in,out] url ID of the URI string to update.
 * @param value ID of the decoded UTF-8 segment string to append.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_append_path_segment(clm_id url, clm_id value);

/** Remove one indexed segment from a hierarchical URI path.
 * @param[in,out] url ID of the URI string to update.
 * @param index Zero-based segment index to remove.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_remove_path_segment(clm_id url, clm_uint index);

/** Derive the parent of a hierarchical URI path without filesystem access.
 * @param source ID of the URI string.
 * @param[out] destination ID of the parent URI result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_parent(clm_id source, clm_id destination);

/** Extract the final encoded URI path segment without query or fragment text.
 * @param source ID of the URI string.
 * @param[out] destination ID of the encoded filename result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_filename(clm_id source, clm_id destination);

/** Extract an extension from the final URI path segment only.
 * @param source ID of the URI string.
 * @param[out] destination ID of the extension result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_get_extension(clm_id source, clm_id destination);

/** Change the final path segment's extension without changing query or fragment.
 * @param[in,out] url ID of the URI string to update.
 * @param extension ID of decoded extension text; an optional leading dot is accepted.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_change_extension(clm_id url, clm_id extension);

/** Ensure that a hierarchical URI path ends in a slash.
 * @param[in,out] url ID of the URI string to update, preserving query and fragment.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @warning Adding a slash may identify a different resource.
 */
clm_int clm_url_ensure_ending_separator(clm_id url);

/** Trim a hierarchical URI path's trailing slash under a documented root policy.
 * @param[in,out] url ID of the URI string to update, preserving query and fragment.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @warning Removing a slash may identify a different resource.
 */
clm_int clm_url_trim_ending_separator(clm_id url);

/** Count ordered ampersand-delimited fields in a URI query.
 * @param source ID of the URI string, not an isolated query string.
 * @param[out] count Caller-owned query-field count result.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @note Duplicate keys and empty values remain separate fields.
 */
clm_int clm_url_query_count(clm_id source, clm_uint* count);

/** Extract one ordered query field without percent or form decoding.
 * @param source ID of the URI string.
 * @param index Zero-based field index under the query-counting policy.
 * @param[out] key ID of the encoded query-key result string.
 * @param[out] value ID of the encoded query-value result string.
 * @param[out] has_value Nonzero if the field includes an equals sign, even with an empty value.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_query_get_at(clm_id source, clm_uint index, clm_id key, clm_id value, clm_int* has_value);

/** Test for an exact, case-sensitive encoded query key.
 * @param source ID of the URI string.
 * @param key ID of the encoded query-key string to match.
 * @return 1 if true, 0 if false, or -1 on invalid input or query failure.
 */
clm_int clm_url_query_has(clm_id source, clm_id key);

/** Extract the value of a selected occurrence of an encoded query key.
 * @param source ID of the URI string.
 * @param key ID of the exact case-sensitive encoded query-key string.
 * @param occurrence Zero-based occurrence among matching keys, preserving query order.
 * @param[out] destination ID of the encoded query-value result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_query_get(clm_id source, clm_id key, clm_uint occurrence, clm_id destination);

/** Append a query field without removing duplicate keys or reordering existing fields.
 * @param[in,out] url ID of the URI string to update.
 * @param key ID of the validated encoded key string.
 * @param value ID of the validated encoded value string when has_value is nonzero.
 * @param has_value Nonzero to include equals and value; zero to append a bare key.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_query_append(clm_id url, clm_id key, clm_id value, clm_int has_value);

/** Replace all matching query-key occurrences with one field at the first match position.
 * @param[in,out] url ID of the URI string to update.
 * @param key ID of the exact encoded key string to replace or insert.
 * @param value ID of the encoded replacement value string when has_value is nonzero.
 * @param has_value Nonzero to include equals and value; zero for a bare key.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_query_set(clm_id url, clm_id key, clm_id value, clm_int has_value);

/** Remove every occurrence of an encoded query key without reordering other fields.
 * @param[in,out] url ID of the URI string to update.
 * @param key ID of the exact case-sensitive encoded key string to remove.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_query_remove(clm_id url, clm_id key);

/** Remove exactly one query field by its ordered index.
 * @param[in,out] url ID of the URI string to update.
 * @param index Zero-based field index to remove.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_query_remove_at(clm_id url, clm_uint index);

/** Explicitly stable-sort query fields by their encoded keys.
 * @param[in,out] url ID of the URI string to update.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @warning Sorting can change server semantics or invalidate signed URLs.
 */
clm_int clm_url_query_sort(clm_id url);

/** Remove authority user information while preserving other URI components.
 * @param[in,out] url ID of the URI string to update.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_strip_credentials(clm_id url);

/** Produce a logging-oriented representation under an explicit redaction policy.
 * @param source ID of the original URI string, which is not modified.
 * @param[out] destination ID of the redacted result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @warning No general policy can identify every secret in a path, query, or fragment.
 */
clm_int clm_url_redact(clm_id source, clm_id destination);

/** Convert an absolute native filesystem path into a file URL.
 * @param path ID of the filesystem path string, including supported Windows UNC forms.
 * @param[out] destination ID of the encoded file-URL result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 */
clm_int clm_url_from_file_path(clm_id path, clm_id destination);

/** Convert a supported file URL into a native filesystem path.
 * @param source ID of the file-URL string.
 * @param[out] destination ID of the native path result string.
 * @return CLM_URL_OPERATION_SUCCESS on success; CLM_URL_OPERATION_FAILURE on error.
 * @note Query, fragment, user information, ports, and encoded separators are rejected.
 */
clm_int clm_url_to_file_path(clm_id source, clm_id destination);

