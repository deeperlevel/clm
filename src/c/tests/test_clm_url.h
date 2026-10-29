#pragma once

#include "test.h"
#include "../clm/url.h"

static int clm_url_test_set(clm_id id, const char* text)
{
    return clm_bpa_set_string(id, text, (clm_uint)strlen(text) + 1) == (int)strlen(text);
}

static int clm_url_test_equals(clm_id id, const char* expected)
{
    char buffer[2048];
    return clm_bpa_get_string(id, buffer, sizeof(buffer)) == (int)strlen(expected) && !strcmp(buffer, expected);
}

static int clm_url_tests(void)
{
    clm_test_result result = {0};
    clm_url_parts parts = {0};
    clm_test_begin("CLM - URL");
    CLM_ASSERT(clm_bpa_create(8201, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS &&
               clm_bpa_set_string(8201, "https://example.com/a?", 23) == 22,
               "create a URL containing an empty query");
    CLM_ASSERT(clm_url_parse(8201, &parts) == CLM_URL_OPERATION_SUCCESS &&
               parts.scheme.present && parts.scheme.length == 5 &&
               parts.query.present && parts.query.length == 0 && !parts.fragment.present,
               "parse spans and distinguish absent from empty URL components");
    CLM_ASSERT(clm_bpa_create(8202, CLM_BPA_TYPE_STRING) == 0 &&
               clm_bpa_create(8203, CLM_BPA_TYPE_STRING) == 0 &&
               clm_bpa_create(8204, CLM_BPA_TYPE_STRING) == 0 &&
               clm_bpa_create(8205, CLM_BPA_TYPE_INT) == 0,
               "create URL destinations and an invalid-type fixture");
    CLM_ASSERT(clm_url_is_valid(8201) == 1 && clm_url_is_absolute(8201) == 1 &&
               clm_url_is_relative(8201) == 0 && clm_url_is_https(8201) == 1 &&
               clm_url_is_http(8201) == 0 && clm_url_is_file(8201) == 0,
               "classify an absolute HTTPS URL without network access");
    CLM_ASSERT(clm_url_test_set(8201, "//example.com/path") &&
               clm_url_is_network_path_reference(8201) == 1 && clm_url_is_relative(8201) == 1,
               "classify scheme-relative network references");
    CLM_ASSERT(clm_url_test_set(8201, "https://user:pass@[::1]:8443/a%2Fb?x=1#frag") &&
               clm_url_parse(8201, &parts) == 0 && parts.host.length == 5 && parts.password.length == 4,
               "parse credentials, bracketed IPv6, ports, and encoded path data");
    const char* components[] = {"https", "user:pass@[::1]:8443", "user:pass", "user", "pass", "[::1]", "8443", "/a%2Fb", "x=1", "frag"};
    int component_results = 1;
    for (int component = 0; component < 10; ++component)
        component_results &= clm_url_has_component(8201, (clm_url_component)component) == 1 &&
            clm_url_get_component(8201, (clm_url_component)component, 8202) == 0 &&
            clm_url_test_equals(8202, components[component]);
    CLM_ASSERT(component_results, "extract every raw component without decoding encoded delimiters");
    clm_uint port = 0;
    CLM_ASSERT(clm_url_get_port(8201, &port) == 0 && port == 8443 &&
               clm_url_get_effective_port(8201, &port) == 0 && port == 8443 &&
               clm_url_set_port(8201, 443) == 0 && clm_url_remove_default_port(8201) == 0 &&
               clm_url_has_component(8201, CLM_URL_COMPONENT_PORT) == 0,
               "read, replace, and remove a default numeric port");
    CLM_ASSERT(clm_url_test_set(8202, "https") && clm_url_get_default_port(8202, &port) == 0 && port == 443 &&
               clm_url_set_port(8201, 65536) == 1, "validate port ranges and known scheme defaults");
    CLM_ASSERT(clm_url_test_set(8202, "example.org") && clm_url_set_component(8201, CLM_URL_COMPONENT_HOST, 8202) == 0 &&
               clm_url_strip_credentials(8201) == 0 && clm_url_has_component(8201, CLM_URL_COMPONENT_USERINFO) == 0,
               "set components and remove credentials without changing URL structure");
    CLM_ASSERT(clm_url_test_set(8202, "bad/host") && clm_url_set_component(8201, CLM_URL_COMPONENT_HOST, 8202) == 1 &&
               clm_url_get_component(8201, CLM_URL_COMPONENT_HOST, 8202) == 0 && clm_url_test_equals(8202, "example.org"),
               "reject component delimiter injection atomically");
    CLM_ASSERT(clm_url_remove_component(8201, CLM_URL_COMPONENT_FRAGMENT) == 0 &&
               clm_url_has_component(8201, CLM_URL_COMPONENT_FRAGMENT) == 0,
               "remove optional components and their delimiters");
    CLM_ASSERT(clm_url_get_origin(8201, 8202) == 0 && clm_url_test_equals(8202, "https://example.org:443") &&
               clm_url_test_set(8203, "HTTPS://EXAMPLE.ORG:443/other") && clm_url_is_same_origin(8201, 8203) == 1,
               "compare normalized tuple origins using effective ports");
    CLM_ASSERT(clm_url_test_set(8202, "127.0.0.1") && clm_url_host_is_ipv4(8202) == 1 &&
               clm_url_test_set(8202, "127.1") && clm_url_host_is_ipv4(8202) == 0 &&
               clm_url_test_set(8202, "[2001:db8::1]") && clm_url_host_is_ipv6(8202) == 1,
               "validate strict IPv4 and bracketed IPv6 host literals");
    CLM_ASSERT(clm_url_test_set(8202, "b\xc3\xbc" "cher.example") && clm_url_host_to_ascii(8202, 8203) == 0 &&
               clm_url_test_equals(8203, "xn--bcher-kva.example") && clm_url_host_to_unicode(8203, 8204) == 0 &&
               clm_url_test_equals(8204, "b\xc3\xbc" "cher.example"), "convert Unicode and IDNA hostnames using native libraries");
    CLM_ASSERT(clm_url_test_set(8201, "https://b\xc3\xbc" "cher.example/caf\xc3\xa9") &&
               clm_url_is_valid_iri(8201) == 1 && clm_url_iri_to_uri(8201, 8202) == 0 &&
               clm_url_test_equals(8202, "https://xn--bcher-kva.example/caf%C3%A9") &&
               clm_url_uri_to_iri(8202, 8203) == 0 && clm_url_test_equals(8203, "https://b\xc3\xbc" "cher.example/caf\xc3\xa9"),
               "convert internationalized URL hosts and UTF-8 path text");
    CLM_ASSERT(clm_url_test_set(8201, "https://example.com/a/b") && clm_url_test_set(8202, "../c?x#f") &&
               clm_url_resolve(8201, 8202, 8203) == 0 && clm_url_test_equals(8203, "https://example.com/c?x#f") &&
               clm_url_get_relative_reference(8203, 8201, 8204) == 0 &&
               clm_url_resolve(8201, 8204, 8204) == 0 && clm_url_equals(8203, 8204) == 1,
               "resolve and relativize references with aliased destinations");
    CLM_ASSERT(clm_url_test_set(8201, "HTTP://EXAMPLE.COM/a/../b/%7e?q=%2f#f") &&
               clm_url_normalize(8201) == 0 && clm_url_test_equals(8201, "http://example.com/b/~?q=%2F#f"),
               "normalize syntax without decoding reserved slashes or sorting queries");
    CLM_ASSERT(clm_url_test_set(8201, "https://example.com/a/../b") && clm_url_remove_dot_segments(8201) == 0 &&
               clm_url_test_equals(8201, "https://example.com/b") &&
               clm_url_test_set(8201, "/%7e/%2f") && clm_url_normalize_percent_encoding(8201) == 0 &&
               clm_url_test_equals(8201, "/~/%2F"), "normalize path dot segments and percent escapes independently");
    CLM_ASSERT(clm_url_test_set(8201, "a/b +") && clm_url_encode_path_segment(8201, 8202) == 0 &&
               clm_url_test_equals(8202, "a%2Fb%20%2B") && clm_url_decode_component(8202, 8203) == 0 &&
               clm_url_test_equals(8203, "a/b +"), "encode one segment and decode exactly once");
    CLM_ASSERT(clm_url_encode_query_value(8201, 8202) == 0 && clm_url_form_encode(8201, 8203) == 0 &&
               clm_url_test_equals(8203, "a%2Fb+%2B") && clm_url_form_decode(8203, 8203) == 0 &&
               clm_url_test_equals(8203, "a/b +"), "keep form plus handling separate from URI encoding");
    CLM_ASSERT(clm_url_encode_component(8201, CLM_URL_COMPONENT_PATH, 8202) == 0 &&
               clm_url_test_equals(8202, "a/b%20+") &&
               clm_url_test_set(8201, "%252F") && clm_url_decode_component(8201, 8202) == 0 &&
               clm_url_test_equals(8202, "%2F"), "encode by component context and avoid double decoding");
    CLM_ASSERT(clm_url_test_set(8201, "%00") && clm_url_decode_component(8201, 8202) == 1 &&
               clm_url_test_equals(8202, "%2F") && clm_url_test_set(8201, "%FF") && clm_url_decode_component(8201, 8202) == 1,
               "reject embedded NUL and malformed UTF-8 without changing outputs");
    CLM_ASSERT(clm_url_test_set(8201, "https://example.com/a/b.txt?x#f") &&
               clm_url_get_path_segment_count(8201, &port) == 0 && port == 2 &&
               clm_url_get_path_segment(8201, 1, 8202) == 0 && clm_url_test_equals(8202, "b.txt") &&
               clm_url_get_filename(8201, 8202) == 0 && clm_url_test_equals(8202, "b.txt") &&
               clm_url_get_extension(8201, 8202) == 0 && clm_url_test_equals(8202, ".txt"),
               "extract encoded URL path segments, filenames, and extensions");
    CLM_ASSERT(clm_url_test_set(8202, "new/data") && clm_url_set_path_segment(8201, 1, 8202) == 0 &&
               clm_url_test_equals(8201, "https://example.com/a/new%2Fdata?x#f") &&
               clm_url_append_path_segment(8201, 8202) == 0 && clm_url_remove_path_segment(8201, 1) == 0 &&
               clm_url_test_equals(8201, "https://example.com/a/new%2Fdata?x#f"),
               "edit decoded segments without introducing structural slashes");
    CLM_ASSERT(clm_url_test_set(8202, "json") && clm_url_change_extension(8201, 8202) == 0 &&
               clm_url_test_equals(8201, "https://example.com/a/new%2Fdata.json?x#f") &&
               clm_url_ensure_ending_separator(8201) == 0 && clm_url_trim_ending_separator(8201) == 0 &&
               clm_url_get_parent(8201, 8202) == 0 && clm_url_test_equals(8202, "https://example.com/a/"),
               "change URL extensions and derive parents without filesystem rules");
    CLM_ASSERT(clm_url_test_set(8201, "/p?tag=red&tag=blue&debug&empty=#f") &&
               clm_url_query_count(8201, &port) == 0 && port == 4 &&
               clm_url_test_set(8202, "tag") && clm_url_query_has(8201, 8202) == 1 &&
               clm_url_query_get(8201, 8202, 1, 8203) == 0 && clm_url_test_equals(8203, "blue"),
               "preserve duplicate query keys and select an occurrence");
    clm_int has_value = -1;
    CLM_ASSERT(clm_url_query_get_at(8201, 2, 8203, 8204, &has_value) == 0 &&
               !has_value && clm_url_test_equals(8203, "debug") && clm_url_test_equals(8204, "") &&
               clm_url_query_get_at(8201, 3, 8203, 8204, &has_value) == 0 && has_value,
               "distinguish bare query keys from present empty values");
    CLM_ASSERT(clm_url_test_set(8203, "green") && clm_url_query_set(8201, 8202, 8203, 1) == 0 &&
               clm_url_test_equals(8201, "/p?tag=green&debug&empty=#f") &&
               clm_url_query_append(8201, 8202, 8203, 1) == 0 &&
               clm_url_query_remove_at(8201, 1) == 0 && clm_url_query_sort(8201) == 0 &&
               clm_url_test_equals(8201, "/p?empty=&tag=green&tag=green#f") &&
               clm_url_query_remove(8201, 8202) == 0 && clm_url_test_equals(8201, "/p?empty=#f"),
               "append, replace, remove, and stable-sort query fields explicitly");
    CLM_ASSERT(clm_url_test_set(8201, "https://user:pass@example.com/secret?token=abc#private") &&
               clm_url_redact(8201, 8202) == 0 && clm_url_test_equals(8202, "https://example.com/"),
               "redact credentials and all resource-specific text for logging");
    CLM_ASSERT(clm_url_test_set(8201, "https") && clm_url_test_set(8202, "example.com") &&
               clm_url_test_set(8203, "/a") && clm_url_test_set(8204, "") &&
               clm_url_build(8201, 8202, 8203, 8204, CLM_URL_ABSENT_ID, 8201) == 0 &&
               clm_url_test_equals(8201, "https://example.com/a?"), "assemble encoded components with an explicit absent sentinel");
#if defined(_WIN32)
    CLM_ASSERT(clm_url_test_set(8201, "C:\\work\\caf\xc3\xa9.txt") && clm_url_from_file_path(8201, 8202) == 0 &&
               clm_url_test_equals(8202, "file:///C:/work/caf%C3%A9.txt") && clm_url_to_file_path(8202, 8203) == 0 &&
               clm_url_test_equals(8203, "C:\\work\\caf\xc3\xa9.txt"), "round-trip a Unicode native path through a file URL");
#else
    CLM_ASSERT(clm_url_test_set(8201, "/work/caf\xc3\xa9.txt") && clm_url_from_file_path(8201, 8202) == 0 &&
               clm_url_to_file_path(8202, 8203) == 0 && clm_url_test_equals(8203, "/work/caf\xc3\xa9.txt"),
               "round-trip a Unicode native path through a file URL");
#endif
    CLM_ASSERT(clm_url_test_set(8201, "file:///a%2Fb") && clm_url_to_file_path(8201, 8203) == 1 &&
               clm_url_test_set(8201, "https://[bad/") && clm_url_is_valid(8201) == 0 &&
               clm_url_is_valid(8205) == -1 && clm_url_parse(8201, NULL) == 1 &&
               clm_url_has_component(8202, (clm_url_component)99) == -1,
               "reject malformed URLs, invalid selectors, invalid IDs, and unsafe file escapes");
    CLM_ASSERT(clm_url_test_set(8201, "file:///path?") && clm_url_get_component(8201, CLM_URL_COMPONENT_HOST, 8202) == 0 &&
               clm_url_test_equals(8202, "") && clm_url_get_component(8201, CLM_URL_COMPONENT_QUERY, 8202) == 0 &&
               clm_url_test_equals(8202, "") && clm_url_query_count(8201, &port) == 0 && port == 0 &&
               clm_url_query_sort(8201) == 0 && clm_url_test_equals(8201, "file:///path?"),
               "preserve present-empty hosts and queries during no-op operations");
    CLM_ASSERT(clm_url_test_set(8201, "/?=x&&key=&") && clm_url_query_count(8201, &port) == 0 && port == 4 &&
               clm_url_query_get_at(8201, 0, 8201, 8202, &has_value) == 0 && has_value &&
               clm_url_test_equals(8201, "") && clm_url_test_equals(8202, "x"),
               "retain empty query fields and commit multiple aliased outputs atomically");
    CLM_ASSERT(clm_url_test_set(8201, "https://example.com/") && clm_url_test_set(8202, "8443") &&
               clm_url_set_component(8201, CLM_URL_COMPONENT_PORT, 8202) == 0 &&
               clm_url_get_port(8201, &port) == 0 && port == 8443 &&
               clm_url_test_set(8202, "user") && clm_url_set_component(8201, CLM_URL_COMPONENT_USERNAME, 8202) == 0 &&
               clm_url_test_set(8202, "pass") && clm_url_set_component(8201, CLM_URL_COMPONENT_PASSWORD, 8202) == 0 &&
               clm_url_test_equals(8201, "https://user:pass@example.com:8443/") &&
               clm_url_remove_component(8201, CLM_URL_COMPONENT_PASSWORD) == 0 &&
               clm_url_remove_component(8201, CLM_URL_COMPONENT_USERNAME) == 0,
               "insert missing nested authority components and remove their delimiters");
    CLM_ASSERT(clm_url_test_set(8201, "https://example.com:999999999999999/") &&
               clm_url_get_port(8201, &port) == 1 && clm_url_get_effective_port(8201, &port) == 1 &&
               clm_url_test_set(8201, "https://example.com:/") && clm_url_get_port(8201, &port) == 1,
               "reject overflowing and present-empty numeric ports");
    CLM_ASSERT(clm_url_test_set(8201, "http://a_b.example/%FF") && clm_url_is_valid(8201) == 1 &&
               clm_url_is_valid_iri(8201) == 1 && clm_url_iri_to_uri(8201, 8202) == 0 &&
               clm_url_test_equals(8202, "http://a_b.example/%FF"),
               "do not impose DNS IDNA rules on an already ASCII URI registered name");
    CLM_ASSERT(clm_url_test_set(8201, "%") && clm_url_decode_component(8201, 8202) == 1 &&
               clm_url_test_set(8201, "%GG") && clm_url_decode_component(8201, 8202) == 1 &&
               clm_url_test_set(8201, "%C0%AF") && clm_url_decode_component(8201, 8202) == 1 &&
               clm_url_test_set(8201, "mailto:user@example.com") && clm_url_get_origin(8201, 8202) == 1 &&
               clm_url_get_path_segment_count(8201, &port) == 1,
               "reject malformed escapes, overlong UTF-8, and unsupported hierarchical operations");
    const char* references[] = {"g", "./g", "g/", "/g", "//g", "?y", "#s", "", ".", "..", "../../../g"};
    const char* resolved[] = {"http://a/b/c/g", "http://a/b/c/g", "http://a/b/c/g/", "http://a/g", "http://g",
        "http://a/b/c/d;p?y", "http://a/b/c/d;p?q#s", "http://a/b/c/d;p?q", "http://a/b/c/", "http://a/b/", "http://a/g"};
    int resolution_valid = clm_url_test_set(8201, "http://a/b/c/d;p?q");
    for (size_t index = 0; index < sizeof(references) / sizeof(references[0]); ++index)
        resolution_valid &= clm_url_test_set(8202, references[index]) && clm_url_resolve(8201, 8202, 8203) == 0 &&
            clm_url_test_equals(8203, resolved[index]);
    CLM_ASSERT(resolution_valid, "match RFC 3986 reference-resolution examples");
    CLM_ASSERT(clm_bpa_delete(8202) == 0 && clm_bpa_delete(8203) == 0 &&
               clm_bpa_delete(8204) == 0 && clm_bpa_delete(8205) == 0,
               "release all additional URL fixtures");
    CLM_ASSERT(clm_bpa_delete(8201) == CLM_BPA_SUCCESS,
               "release the URL fixture");
    clm_test_summary(result.passed, result.failed);
    return result.failed != 0;
}