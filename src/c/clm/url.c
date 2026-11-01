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

#include "url.h"

int clm_url_internal_parse(const char* text, UriUriA* uri) {
	memset(uri, 0, sizeof(*uri));
	if (!text || uriParseSingleUriA(uri, text, NULL) != URI_SUCCESS) {
		uriFreeUriMembersA(uri); memset(uri, 0, sizeof(*uri)); return 1;
	}
	return 0;
}

clm_url_span clm_url_internal_range(const char* text, UriTextRangeA range) {
	clm_url_span result = {0};
	if (range.first) {
		result.offset = (clm_uint)(range.first - text);
		result.length = (clm_uint)(range.afterLast - range.first);
		result.present = 1;
	}
	return result;
}

int clm_url_internal_parts(const char* text, clm_url_parts* parts) {
	UriUriA uri;
	if (!parts || clm_url_internal_parse(text, &uri)) return 1;
	clm_url_parts result = {0};
	result.scheme = clm_url_internal_range(text, uri.scheme);
	result.userinfo = clm_url_internal_range(text, uri.userInfo);
	result.host = clm_url_internal_range(text, uri.hostText);
	result.port = clm_url_internal_range(text, uri.portText);
	result.query = clm_url_internal_range(text, uri.query);
	result.fragment = clm_url_internal_range(text, uri.fragment);
	size_t cursor = result.scheme.present ? result.scheme.length + 1 : 0;
	if (text[cursor] == '/' && text[cursor + 1] == '/') {
		result.authority.present = 1;
		result.authority.offset = (clm_uint)(cursor += 2);
		cursor += strcspn(text + cursor, "/?#");
		result.authority.length = (clm_uint)(cursor - result.authority.offset);
	}
	if (result.host.present && (uri.hostData.ip6 || uri.hostData.ipFuture.first)) {
		--result.host.offset; result.host.length += 2;
	}
	result.path.present = 1;
	result.path.offset = (clm_uint)cursor;
	result.path.length = (clm_uint)strcspn(text + cursor, "?#");
	if (result.userinfo.present) {
		result.username = result.userinfo;
		const char* colon = (const char*)memchr(text + result.userinfo.offset, ':', result.userinfo.length);
		if (colon) {
			result.username.length = (clm_uint)(colon - text - result.userinfo.offset);
			result.password.present = 1;
			result.password.offset = (clm_uint)(colon - text + 1);
			result.password.length = result.userinfo.offset + result.userinfo.length - result.password.offset;
		}
	}
	uriFreeUriMembersA(&uri);
	*parts = result;
	return 0;
}

clm_url_span* clm_url_internal_component(clm_url_parts* parts, clm_url_component component) {
	switch (component) {
	case CLM_URL_COMPONENT_SCHEME: return &parts->scheme;
	case CLM_URL_COMPONENT_AUTHORITY: return &parts->authority;
	case CLM_URL_COMPONENT_USERINFO: return &parts->userinfo;
	case CLM_URL_COMPONENT_USERNAME: return &parts->username;
	case CLM_URL_COMPONENT_PASSWORD: return &parts->password;
	case CLM_URL_COMPONENT_HOST: return &parts->host;
	case CLM_URL_COMPONENT_PORT: return &parts->port;
	case CLM_URL_COMPONENT_PATH: return &parts->path;
	case CLM_URL_COMPONENT_QUERY: return &parts->query;
	case CLM_URL_COMPONENT_FRAGMENT: return &parts->fragment;
	default: return NULL;
	}
}

char* clm_url_internal_substring(const char* text, size_t offset, size_t length) {
	char* result = (char*)malloc(length + 1);
	if (result) { memcpy(result, text + offset, length); result[length] = 0; }
	return result;
}

char* clm_url_internal_splice(const char* text, size_t offset, size_t length,
										   const char* before, const char* value, const char* after) {
	const size_t original = strlen(text), first = strlen(before), middle = strlen(value), last = strlen(after);
	if (offset > original || length > original - offset || first + middle + last > (size_t)INT_MAX - original) return NULL;
	const size_t size = original - length + first + middle + last + 1;
	char* output = (char*)malloc(size);
	if (!output) return NULL;
	memcpy(output, text, offset);
	size_t used = offset;
	memcpy(output + used, before, first); used += first;
	memcpy(output + used, value, middle); used += middle;
	memcpy(output + used, after, last); used += last;
	memcpy(output + used, text + offset + length, original - offset - length + 1);
	return output;
}

char* clm_url_internal_edit(const char* text, clm_url_component component, const char* value) {
	clm_url_parts parts;
	if (!text || clm_url_internal_parts(text, &parts)) return NULL;
	clm_url_span* selected = clm_url_internal_component(&parts, component);
	if (!selected) return NULL;
	const int removing = value == NULL;
	if (removing && !selected->present) return clm_path_internal_copy(text);
	size_t offset = selected->offset, length = selected->length;
	const char* before = "";
	const char* after = "";
	if (removing) {
		switch (component) {
		case CLM_URL_COMPONENT_SCHEME: ++length; break;
		case CLM_URL_COMPONENT_AUTHORITY: offset -= 2; length += 2; break;
		case CLM_URL_COMPONENT_USERINFO: ++length; break;
		case CLM_URL_COMPONENT_USERNAME:
			if (!parts.password.present) ++length;
			break;
		case CLM_URL_COMPONENT_PASSWORD:
		case CLM_URL_COMPONENT_PORT:
		case CLM_URL_COMPONENT_QUERY:
		case CLM_URL_COMPONENT_FRAGMENT: --offset; ++length; break;
		case CLM_URL_COMPONENT_HOST: return NULL;
		default: break;
		}
	} else if (!selected->present) {
		length = 0;
		switch (component) {
		case CLM_URL_COMPONENT_SCHEME: offset = 0; after = ":"; break;
		case CLM_URL_COMPONENT_AUTHORITY:
		case CLM_URL_COMPONENT_HOST:
			offset = parts.scheme.present ? parts.scheme.length + 1 : 0; before = "//"; break;
		case CLM_URL_COMPONENT_USERINFO:
		case CLM_URL_COMPONENT_USERNAME:
			if (!parts.authority.present) return NULL;
			offset = parts.authority.offset; after = "@"; break;
		case CLM_URL_COMPONENT_PASSWORD:
			if (!parts.authority.present) return NULL;
			offset = parts.userinfo.present ? parts.userinfo.offset + parts.userinfo.length : parts.authority.offset;
			before = ":"; after = parts.userinfo.present ? "" : "@"; break;
		case CLM_URL_COMPONENT_PORT:
			if (!parts.host.present) return NULL;
			offset = parts.host.offset + parts.host.length; before = ":"; break;
		case CLM_URL_COMPONENT_QUERY:
			offset = parts.fragment.present ? parts.fragment.offset - 1 : strlen(text); before = "?"; break;
		case CLM_URL_COMPONENT_FRAGMENT: offset = strlen(text); before = "#"; break;
		default: return NULL;
		}
	}
	char* result = clm_url_internal_splice(text, offset, length, before, removing ? "" : value, after);
	clm_url_parts verified;
	if (!result || clm_url_internal_parts(result, &verified)) { free(result); return NULL; }
	clm_url_span* actual = clm_url_internal_component(&verified, component);
	if ((!removing && (!actual->present || actual->length != strlen(value) ||
					  memcmp(result + actual->offset, value, actual->length))) ||
		(removing && component != CLM_URL_COMPONENT_PATH && actual->present)) {
		free(result); return NULL;
	}
	return result;
}

int clm_url_internal_commit(clm_id destination, char* output) {
	const int status = output ? clm_path_internal_store(destination, output) : 1;
	free(output);
	return status;
}

int clm_url_internal_scheme(const char* text, const char* expected) {
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return -1;
	const size_t length = strlen(expected);
	if (!parts.scheme.present || parts.scheme.length != length) return 0;
	for (size_t index = 0; index < length; ++index)
		if (tolower((unsigned char)text[index]) != expected[index]) return 0;
	return 1;
}

int clm_url_internal_hex(unsigned char character) {
	if (character >= '0' && character <= '9') return character - '0';
	if (character >= 'a' && character <= 'f') return character - 'a' + 10;
	if (character >= 'A' && character <= 'F') return character - 'A' + 10;
	return -1;
}

int clm_url_internal_unreserved(unsigned char character) {
	return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
		(character >= '0' && character <= '9') || strchr("-._~", character) != NULL;
}

char* clm_url_internal_encode(const char* text, const char* allowed, int form) {
	if (!text || strlen(text) > ((size_t)INT_MAX - 1) / 3) return NULL;
	char* output = (char*)malloc(strlen(text) * 3 + 1);
	if (!output) return NULL;
	size_t used = 0;
	for (const unsigned char* cursor = (const unsigned char*)text; *cursor; ++cursor) {
		if (clm_url_internal_unreserved(*cursor) || strchr(allowed, *cursor)) output[used++] = (char)*cursor;
		else if (form && *cursor == ' ') output[used++] = '+';
		else {
			output[used++] = '%'; output[used++] = "0123456789ABCDEF"[*cursor >> 4];
			output[used++] = "0123456789ABCDEF"[*cursor & 15];
		}
	}
	output[used] = 0;
	return output;
}

char* clm_url_internal_decode(const char* text, int form, int unicode_only) {
	if (!text) return NULL;
	char* output = (char*)malloc(strlen(text) + 1);
	if (!output) return NULL;
	size_t used = 0;
	for (size_t cursor = 0; text[cursor]; ++cursor) {
		unsigned char character = (unsigned char)text[cursor];
		if (character == '%') {
			if (!text[cursor + 1] || !text[cursor + 2]) { free(output); return NULL; }
			const int first = clm_url_internal_hex((unsigned char)text[cursor + 1]);
			const int second = clm_url_internal_hex((unsigned char)text[cursor + 2]);
			if (first < 0 || second < 0 || !(first || second)) { free(output); return NULL; }
			const unsigned char decoded = (unsigned char)(first * 16 + second);
			if (!unicode_only || decoded >= 128) { character = decoded; cursor += 2; }
		} else if (form && character == '+') character = ' ';
		output[used++] = (char)character;
	}
	output[used] = 0;
	size_t characters;
	if (clm_string_measure(output, used, CLM_STRING_ENCODING_UTF8, &characters) != CLM_BPA_SUCCESS) {
		free(output); return NULL;
	}
	return output;
}

char* clm_url_internal_idna(const char* text, int ascii) {
	if (!text || !*text || strpbrk(text, "/\\:@[]?#%")) return NULL;
#if defined(_WIN32)
	wchar_t* wide = clm_path_internal_wide(text);
	if (!wide) return NULL;
	const DWORD flags = ascii ? IDN_USE_STD3_ASCII_RULES : 0;
	const int count = ascii ? IdnToAscii(flags, wide, -1, NULL, 0) : IdnToUnicode(flags, wide, -1, NULL, 0);
	wchar_t* result = count ? (wchar_t*)malloc((size_t)count * sizeof(wchar_t)) : NULL;
	char* output = NULL;
	if (result && (ascii ? IdnToAscii(flags, wide, -1, result, count) : IdnToUnicode(flags, wide, -1, result, count)))
		output = clm_path_internal_utf8(result);
	free(result); free(wide);
	return output;
#else
	char* native = NULL;
	const int status = ascii ? idn2_lookup_u8((const uint8_t*)text, (uint8_t**)&native,
		IDN2_NFC_INPUT | IDN2_NONTRANSITIONAL | IDN2_USE_STD3_ASCII_RULES) : idn2_to_unicode_8z8z(text, &native, 0);
	char* output = status == IDN2_OK ? clm_path_internal_copy(native) : NULL;
	idn2_free(native);
	return output;
#endif
}

char* clm_url_internal_iri_ascii(const char* text) {
	if (!text) return NULL;
	size_t offset = 0, bytes = strlen(text);
	while (offset < bytes) {
		uint32_t point;
		const size_t first = offset;
		if (clm_string_utf8_decode_one((const unsigned char*)text, bytes, &offset, &point) != CLM_BPA_SUCCESS) return NULL;
		if (point >= 128) {
			const char* query = strchr(text, '?');
			const char* fragment = strchr(text, '#');
			const int in_query = query && (size_t)(query - text) < first && (!fragment || (size_t)(fragment - text) > first);
			const int private_point = (point >= 0xe000 && point <= 0xf8ff) ||
				(point >= 0xf0000 && point <= 0xffffd) || (point >= 0x100000 && point <= 0x10fffd);
			const int allowed = (point >= 0xa0 && point <= 0xd7ff) ||
				(point >= 0xf900 && point <= 0xfdcf) || (point >= 0xfdf0 && point <= 0xffef) ||
				(point >= 0x10000 && point <= 0xdfffd && (point & 0xffff) <= 0xfffd) ||
				(point >= 0xe1000 && point <= 0xefffd);
			if (!allowed && !(in_query && private_point)) return NULL;
		}
	}
	if (bytes > ((size_t)INT_MAX - 1) / 3) return NULL;
	char* encoded = (char*)malloc(bytes * 3 + 1);
	if (!encoded) return NULL;
	size_t used = 0;
	for (size_t cursor = 0; cursor < bytes; ++cursor) {
		const unsigned char character = (unsigned char)text[cursor];
		if (character < 128) encoded[used++] = (char)character;
		else { encoded[used++] = '%'; encoded[used++] = "0123456789ABCDEF"[character >> 4]; encoded[used++] = "0123456789ABCDEF"[character & 15]; }
	}
	encoded[used] = 0;
	clm_url_parts parts;
	if (clm_url_internal_parts(encoded, &parts)) { free(encoded); return NULL; }
	if (parts.host.present && parts.host.length && encoded[parts.host.offset] != '[') {
		char* host = clm_url_internal_substring(encoded, parts.host.offset, parts.host.length);
		char* unicode_host = clm_url_internal_decode(host, 0, 0);
		int international = 0;
		for (const unsigned char* cursor = (const unsigned char*)unicode_host; cursor && *cursor; ++cursor)
			international |= *cursor >= 128;
		if (unicode_host && !international) { free(host); free(unicode_host); return encoded; }
		char* ascii_host = clm_url_internal_idna(unicode_host, 1);
		free(host); free(unicode_host);
		char* result = ascii_host ? clm_url_internal_edit(encoded, CLM_URL_COMPONENT_HOST, ascii_host) : NULL;
		free(ascii_host); free(encoded);
		return result;
	}
	return encoded;
}

char* clm_url_internal_serialize(UriUriA* uri) {
	int count;
	if (uriToStringCharsRequiredA(uri, &count) != URI_SUCCESS || count < 0 || count == INT_MAX) return NULL;
	char* output = (char*)malloc((size_t)count + 1);
	if (output && uriToStringA(output, uri, count + 1, NULL) != URI_SUCCESS) { free(output); return NULL; }
	return output;
}

char* clm_url_internal_normalized(const char* text, unsigned int mask) {
	UriUriA uri;
	if (clm_url_internal_parse(text, &uri)) return NULL;
	const int status = mask == URI_NORMALIZED ? uriNormalizeSyntaxA(&uri) : uriNormalizeSyntaxExA(&uri, mask);
	char* output = status == URI_SUCCESS ? clm_url_internal_serialize(&uri) : NULL;
	uriFreeUriMembersA(&uri);
	return output;
}

int clm_url_internal_default_port(const char* text, size_t length, clm_uint* port) {
	static const char* const schemes[] = {"http", "https", "ws", "wss", "ftp", "ssh", "gopher"};
	static const clm_uint ports[] = {80, 443, 80, 443, 21, 22, 70};
	if (!port) return 1;
	for (size_t index = 0; index < sizeof(ports) / sizeof(ports[0]); ++index) {
		if (strlen(schemes[index]) != length) continue;
		size_t cursor = 0;
		while (cursor < length && tolower((unsigned char)text[cursor]) == schemes[index][cursor]) ++cursor;
		if (cursor == length) { *port = ports[index]; return 0; }
	}
	return 1;
}

int clm_url_internal_port(const char* text, clm_url_parts* parts, int effective, clm_uint* port) {
	if (!port) return 1;
	if (!parts->port.present) return effective && parts->scheme.present
		? clm_url_internal_default_port(text, parts->scheme.length, port) : 1;
	if (!parts->port.length) return 1;
	clm_uint result = 0;
	for (size_t index = 0; index < parts->port.length; ++index) {
		const unsigned char character = (unsigned char)text[parts->port.offset + index];
		if (character < '0' || character > '9' || result > (65535U - (character - '0')) / 10) return 1;
		result = result * 10 + character - '0';
	}
	*port = result;
	return 0;
}

char* clm_url_internal_origin(const char* text) {
	char* normalized = clm_url_internal_normalized(text, URI_NORMALIZE_SCHEME | URI_NORMALIZE_HOST);
	clm_url_parts parts;
	clm_uint port;
	if (!normalized || clm_url_internal_parts(normalized, &parts) || !parts.host.length ||
		!(clm_url_internal_scheme(normalized, "http") == 1 || clm_url_internal_scheme(normalized, "https") == 1 ||
		  clm_url_internal_scheme(normalized, "ws") == 1 || clm_url_internal_scheme(normalized, "wss") == 1 ||
		  clm_url_internal_scheme(normalized, "ftp") == 1) || clm_url_internal_port(normalized, &parts, 1, &port)) {
		free(normalized); return NULL;
	}
	const size_t size = parts.scheme.length + parts.host.length + 12;
	char* output = (char*)malloc(size);
	if (output) snprintf(output, size, "%.*s://%.*s:%u", (int)parts.scheme.length, normalized,
		(int)parts.host.length, normalized + parts.host.offset, port);
	free(normalized);
	return output;
}

clm_url_internal_field* clm_url_internal_fields(const char* text, clm_url_span query, size_t* count) {
	*count = 0;
	const size_t capacity = query.length + 1;
	clm_url_internal_field* fields = (clm_url_internal_field*)calloc(capacity, sizeof(*fields));
	if (!fields) return NULL;
	if (!query.present || !query.length) return fields;
	size_t cursor = query.offset, end = cursor + query.length;
	do {
		clm_url_internal_field* field = &fields[(*count)++];
		field->offset = cursor;
		while (cursor < end && text[cursor] != '&') ++cursor;
		field->length = cursor - field->offset;
		const char* equals = (const char*)memchr(text + field->offset, '=', field->length);
		field->has_value = equals != NULL;
		field->key_length = equals ? (size_t)(equals - text) - field->offset : field->length;
	} while (cursor++ < end);
	return fields;
}

int clm_url_internal_field_matches(const char* text, clm_url_internal_field field, const char* key) {
	return key && strlen(key) == field.key_length && !memcmp(text + field.offset, key, field.key_length);
}

int clm_url_internal_query_edit(clm_id url, int mode, const char* key, const char* value,
											 int has_value, clm_uint selected) {
	const char* text = clm_path_internal_text(url);
	clm_url_parts parts;
	if (!text || clm_url_internal_parts(text, &parts)) return 1;
	if ((mode == 0 || mode == 1) && (!key || (has_value && !value) || strchr(key, '&') || strchr(key, '=') ||
		!uriIsWellFormedQueryA(key, key + strlen(key)) || (has_value && (strchr(value, '&') ||
		!uriIsWellFormedQueryA(value, value + strlen(value)))))) return 1;
	size_t count;
	clm_url_internal_field* fields = clm_url_internal_fields(text, parts.query, &count);
	if (!fields || (mode == 3 && selected >= count)) { free(fields); return 1; }
	if (!count && (mode == 2 || mode == 4)) { free(fields); return 0; }
	if (mode == 4) {
		for (size_t index = 1; index < count; ++index) {
			const clm_url_internal_field item = fields[index];
			size_t cursor = index;
			while (cursor) {
				const clm_url_internal_field previous = fields[cursor - 1];
				const size_t common = previous.key_length < item.key_length ? previous.key_length : item.key_length;
				const int compared = memcmp(text + previous.offset, text + item.offset, common);
				if (compared < 0 || (!compared && previous.key_length <= item.key_length)) break;
				fields[cursor] = previous; --cursor;
			}
			fields[cursor] = item;
		}
	}
	const size_t capacity = strlen(text) + (key ? strlen(key) : 0) + (value ? strlen(value) : 0) + 4;
	char* query = (char*)malloc(capacity);
	if (!query) { free(fields); return 1; }
	size_t used = 0, kept = 0;
	int inserted = 0;
	for (size_t index = 0; index <= count; ++index) {
		const int match = index < count && clm_url_internal_field_matches(text, fields[index], key);
		const int insert = !inserted && ((mode == 0 && index == count) ||
			(mode == 1 && (match || index == count)));
		if (insert) {
			if (kept++) query[used++] = '&';
			const size_t key_length = strlen(key);
			memcpy(query + used, key, key_length); used += key_length;
			if (has_value) { query[used++] = '='; const size_t value_length = strlen(value); memcpy(query + used, value, value_length); used += value_length; }
			inserted = 1;
		}
		if (index == count) break;
		if (((mode == 1 || mode == 2) && match) || (mode == 3 && index == selected)) continue;
		if (kept++) query[used++] = '&';
		memcpy(query + used, text + fields[index].offset, fields[index].length); used += fields[index].length;
	}
	query[used] = 0;
	char* output = clm_url_internal_edit(text, CLM_URL_COMPONENT_QUERY, kept ? query : NULL);
	free(query); free(fields);
	return clm_url_internal_commit(url, output);
}

int clm_url_internal_segment(clm_id url, int mode, clm_uint selected,
										  const char* value, clm_id destination, clm_uint* count_result) {
	const char* text = clm_path_internal_text(url);
	clm_url_parts parts;
	if (!text || clm_url_internal_parts(text, &parts)) return 1;
	const char* path = text + parts.path.offset;
	const size_t length = parts.path.length;
	if (length && path[0] != '/' && parts.scheme.present && !parts.authority.present) return 1;
	const int absolute = length && path[0] == '/';
	size_t count = 0;
	clm_url_internal_field* fields = (clm_url_internal_field*)calloc(length + 1, sizeof(*fields));
	if (!fields) return 1;
	if (length > (size_t)absolute) {
		size_t cursor = (size_t)absolute;
		do {
			fields[count].offset = cursor;
			while (cursor < length && path[cursor] != '/') ++cursor;
			fields[count].length = cursor - fields[count].offset; ++count;
		} while (cursor++ < length);
	}
	if (mode == 0) { if (!count_result) { free(fields); return 1; } *count_result = (clm_uint)count; free(fields); return 0; }
	if (mode != 3 && selected >= count) { free(fields); return 1; }
	if (mode == 1) {
		const int status = clm_path_internal_slice(destination, path + fields[selected].offset, fields[selected].length);
		free(fields); return status;
	}
	char* encoded = value ? clm_url_internal_encode(value, "", 0) : NULL;
	if ((mode == 2 || mode == 3) && !encoded) { free(fields); return 1; }
	char* output_path = (char*)malloc(length + (encoded ? strlen(encoded) : 0) + 3);
	if (!output_path) { free(encoded); free(fields); return 1; }
	size_t used = 0, kept = 0;
	if (absolute || parts.authority.present) output_path[used++] = '/';
	for (size_t index = 0; index < count; ++index) {
		if (mode == 4 && index == selected) continue;
		if (kept++) output_path[used++] = '/';
		const char* segment = mode == 2 && index == selected ? encoded : path + fields[index].offset;
		const size_t size = mode == 2 && index == selected ? strlen(encoded) : fields[index].length;
		memcpy(output_path + used, segment, size); used += size;
	}
	if (mode == 3) {
		if (kept) output_path[used++] = '/';
		memcpy(output_path + used, encoded, strlen(encoded)); used += strlen(encoded);
	}
	output_path[used] = 0;
	char* output = clm_url_internal_edit(text, CLM_URL_COMPONENT_PATH, output_path);
	free(output_path); free(encoded); free(fields);
	return clm_url_internal_commit(url, output);
}

clm_int clm_url_parse(clm_id source, clm_url_parts* parts) {
	return clm_url_internal_parts(clm_path_internal_text(source), parts);
}

clm_int clm_url_is_valid(clm_id source) {
	const char* text = clm_path_internal_text(source);
	if (!text) return -1;
	clm_url_parts parts;
	return clm_url_internal_parts(text, &parts) == 0;
}

clm_int clm_url_is_valid_iri(clm_id source) {
	const char* text = clm_path_internal_text(source);
	if (!text) return -1;
	char* ascii = clm_url_internal_iri_ascii(text);
	const int valid = ascii != NULL;
	free(ascii);
	return valid;
}

clm_int clm_url_is_absolute(clm_id source) {
	clm_url_parts parts;
	return clm_url_parse(source, &parts) == 0 ? parts.scheme.present : -1;
}

clm_int clm_url_is_relative(clm_id source) {
	const int absolute = clm_url_is_absolute(source);
	return absolute < 0 ? -1 : !absolute;
}

clm_int clm_url_is_network_path_reference(clm_id source) {
	clm_url_parts parts;
	return clm_url_parse(source, &parts) == 0 ? !parts.scheme.present && parts.authority.present : -1;
}

clm_int clm_url_is_http(clm_id source) {
	return clm_url_internal_scheme(clm_path_internal_text(source), "http");
}

clm_int clm_url_is_https(clm_id source) {
	return clm_url_internal_scheme(clm_path_internal_text(source), "https");
}

clm_int clm_url_is_file(clm_id source) {
	return clm_url_internal_scheme(clm_path_internal_text(source), "file");
}

clm_int clm_url_has_component(clm_id source, clm_url_component component) {
	clm_url_parts parts;
	if (clm_url_parse(source, &parts)) return -1;
	clm_url_span* selected = clm_url_internal_component(&parts, component);
	return selected ? selected->present : -1;
}

clm_int clm_url_get_component(clm_id source, clm_url_component component, clm_id destination) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	clm_url_span* selected = clm_url_internal_component(&parts, component);
	return selected && selected->present ? clm_path_internal_slice(destination, text + selected->offset, selected->length) : 1;
}

clm_int clm_url_set_component(clm_id url, clm_url_component component, clm_id value) {
	const char* replacement = clm_path_internal_text(value);
	return replacement ? clm_url_internal_commit(url, clm_url_internal_edit(clm_path_internal_text(url), component, replacement)) : 1;
}

clm_int clm_url_remove_component(clm_id url, clm_url_component component) {
	return clm_url_internal_commit(url, clm_url_internal_edit(clm_path_internal_text(url), component, NULL));
}

clm_int clm_url_build(clm_id scheme, clm_id authority, clm_id path, clm_id query, clm_id fragment, clm_id destination) {
	const clm_id ids[5] = {scheme, authority, path, query, fragment};
	const char* pieces[5] = {0};
	size_t capacity = 9;
	for (size_t index = 0; index < 5; ++index) {
		if (ids[index] == CLM_URL_ABSENT_ID && index != 2) continue;
		pieces[index] = clm_path_internal_text(ids[index]);
		if (!pieces[index] || strlen(pieces[index]) > (size_t)INT_MAX - capacity) return 1;
		capacity += strlen(pieces[index]);
	}
	char* output = (char*)malloc(capacity);
	if (!output) return 1;
	size_t used = 0;
	for (size_t index = 0; index < 5; ++index) {
		if (!pieces[index]) continue;
		if (index == 1) { output[used++] = '/'; output[used++] = '/'; }
		if (index == 3) output[used++] = '?';
		if (index == 4) output[used++] = '#';
		const size_t length = strlen(pieces[index]);
		memcpy(output + used, pieces[index], length); used += length;
		if (index == 0) output[used++] = ':';
	}
	output[used] = 0;
	clm_url_parts parts;
	if (clm_url_internal_parts(output, &parts)) { free(output); return 1; }
	const clm_url_span spans[5] = {parts.scheme, parts.authority, parts.path, parts.query, parts.fragment};
	for (size_t index = 0; index < 5; ++index) {
		if ((!pieces[index] && spans[index].present) || (pieces[index] && (!spans[index].present ||
			spans[index].length != strlen(pieces[index]) || memcmp(output + spans[index].offset, pieces[index], spans[index].length)))) {
			free(output); return 1;
		}
	}
	return clm_url_internal_commit(destination, output);
}

clm_int clm_url_get_port(clm_id source, clm_uint* port) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	return clm_url_internal_parts(text, &parts) ? 1 : clm_url_internal_port(text, &parts, 0, port);
}

clm_int clm_url_set_port(clm_id url, clm_uint port) {
	if (port > 65535) return 1;
	char text[6];
	snprintf(text, sizeof(text), "%u", port);
	return clm_url_internal_commit(url, clm_url_internal_edit(clm_path_internal_text(url), CLM_URL_COMPONENT_PORT, text));
}

clm_int clm_url_get_default_port(clm_id scheme, clm_uint* port) {
	const char* text = clm_path_internal_text(scheme);
	return text ? clm_url_internal_default_port(text, strlen(text), port) : 1;
}

clm_int clm_url_get_effective_port(clm_id source, clm_uint* port) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	return clm_url_internal_parts(text, &parts) ? 1 : clm_url_internal_port(text, &parts, 1, port);
}

clm_int clm_url_remove_default_port(clm_id url) {
	const char* text = clm_path_internal_text(url);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	if (!parts.port.present) return 0;
	clm_uint actual, expected;
	if (clm_url_internal_port(text, &parts, 0, &actual)) return 1;
	if (!parts.scheme.present || clm_url_internal_default_port(text, parts.scheme.length, &expected) || actual != expected) return 0;
	return clm_url_remove_component(url, CLM_URL_COMPONENT_PORT);
}

clm_int clm_url_host_is_ipv4(clm_id source) {
	const char* text = clm_path_internal_text(source);
	return text ? uriIsWellFormedHostIp4A(text, text + strlen(text)) != 0 : -1;
}

clm_int clm_url_host_is_ipv6(clm_id source) {
	const char* text = clm_path_internal_text(source);
	if (!text) return -1;
	const size_t length = strlen(text);
	return length > 2 && text[0] == '[' && text[length - 1] == ']' &&
		uriParseIpSixAddressA(NULL, text + 1, text + length - 1) == URI_SUCCESS;
}

clm_int clm_url_host_to_ascii(clm_id source, clm_id destination) {
	return clm_url_internal_commit(destination, clm_url_internal_idna(clm_path_internal_text(source), 1));
}

clm_int clm_url_host_to_unicode(clm_id source, clm_id destination) {
	return clm_url_internal_commit(destination, clm_url_internal_idna(clm_path_internal_text(source), 0));
}

clm_int clm_url_get_origin(clm_id source, clm_id destination) {
	return clm_url_internal_commit(destination, clm_url_internal_origin(clm_path_internal_text(source)));
}

clm_int clm_url_is_same_origin(clm_id left, clm_id right) {
	char* first = clm_url_internal_origin(clm_path_internal_text(left));
	char* second = clm_url_internal_origin(clm_path_internal_text(right));
	const int equal = first && second ? !strcmp(first, second) : -1;
	free(first); free(second);
	return equal;
}

clm_int clm_url_resolve(clm_id base, clm_id reference, clm_id destination) {
	UriUriA origin = {0}, relative = {0}, result = {0};
	char* output = NULL;
	if (!clm_url_internal_parse(clm_path_internal_text(base), &origin) && origin.scheme.first &&
		!clm_url_internal_parse(clm_path_internal_text(reference), &relative) &&
		uriAddBaseUriA(&result, &relative, &origin) == URI_SUCCESS) output = clm_url_internal_serialize(&result);
	uriFreeUriMembersA(&origin); uriFreeUriMembersA(&relative); uriFreeUriMembersA(&result);
	return clm_url_internal_commit(destination, output);
}

clm_int clm_url_get_relative_reference(clm_id source, clm_id base, clm_id destination) {
	UriUriA target = {0}, origin = {0}, result = {0};
	char* output = NULL;
	if (!clm_url_internal_parse(clm_path_internal_text(source), &target) && target.scheme.first &&
		!clm_url_internal_parse(clm_path_internal_text(base), &origin) && origin.scheme.first &&
		uriRemoveBaseUriA(&result, &target, &origin, URI_FALSE) == URI_SUCCESS) output = clm_url_internal_serialize(&result);
	uriFreeUriMembersA(&target); uriFreeUriMembersA(&origin); uriFreeUriMembersA(&result);
	return clm_url_internal_commit(destination, output);
}

clm_int clm_url_normalize(clm_id url) {
	return clm_url_internal_commit(url, clm_url_internal_normalized(clm_path_internal_text(url), URI_NORMALIZED));
}

clm_int clm_url_remove_dot_segments(clm_id url) {
	return clm_url_internal_commit(url, clm_url_internal_normalized(clm_path_internal_text(url), URI_NORMALIZE_PATH));
}

clm_int clm_url_normalize_percent_encoding(clm_id url) {
	const char* text = clm_path_internal_text(url);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	char* output = (char*)malloc(strlen(text) + 1);
	if (!output) return 1;
	size_t used = 0;
	for (size_t cursor = 0; text[cursor]; ++cursor) {
		if (text[cursor] != '%') output[used++] = text[cursor];
		else {
			const unsigned char character = (unsigned char)(clm_url_internal_hex((unsigned char)text[cursor + 1]) * 16 +
				clm_url_internal_hex((unsigned char)text[cursor + 2]));
			if (character && clm_url_internal_unreserved(character)) output[used++] = (char)character;
			else { output[used++] = '%'; output[used++] = (char)toupper((unsigned char)text[cursor + 1]); output[used++] = (char)toupper((unsigned char)text[cursor + 2]); }
			cursor += 2;
		}
	}
	output[used] = 0;
	return clm_url_internal_commit(url, output);
}

clm_int clm_url_equals(clm_id left, clm_id right) {
	char* first = clm_url_internal_normalized(clm_path_internal_text(left), URI_NORMALIZED);
	char* second = clm_url_internal_normalized(clm_path_internal_text(right), URI_NORMALIZED);
	const int equal = first && second ? !strcmp(first, second) : -1;
	free(first); free(second);
	return equal;
}

clm_int clm_url_iri_to_uri(clm_id source, clm_id destination) {
	return clm_url_internal_commit(destination, clm_url_internal_iri_ascii(clm_path_internal_text(source)));
}

clm_int clm_url_uri_to_iri(clm_id source, clm_id destination) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	char* intermediate = clm_path_internal_copy(text);
	if (parts.host.length && text[parts.host.offset] != '[') {
		char* host = clm_url_internal_substring(text, parts.host.offset, parts.host.length);
		int international = 0;
		for (size_t index = 0; host && index + 3 < strlen(host); ++index)
			international |= tolower((unsigned char)host[index]) == 'x' && tolower((unsigned char)host[index + 1]) == 'n' &&
				host[index + 2] == '-' && host[index + 3] == '-';
		char* unicode_host = international ? clm_url_internal_idna(host, 0) : clm_path_internal_copy(host);
		free(host); free(intermediate);
		intermediate = unicode_host ? clm_url_internal_splice(text, parts.host.offset, parts.host.length, "", unicode_host, "") : NULL;
		free(unicode_host);
	}
	char* output = clm_url_internal_decode(intermediate, 0, 1);
	free(intermediate);
	char* verified = output ? clm_url_internal_iri_ascii(output) : NULL;
	if (!verified) { free(output); return 1; }
	free(verified);
	return clm_url_internal_commit(destination, output);
}

clm_int clm_url_encode_component(clm_id source, clm_url_component component, clm_id destination) {
	const char* allowed;
	switch (component) {
	case CLM_URL_COMPONENT_SCHEME: allowed = "+-."; break;
	case CLM_URL_COMPONENT_USERINFO: allowed = "!$&'()*+,;=:"; break;
	case CLM_URL_COMPONENT_USERNAME: allowed = "!$&'()*+,;="; break;
	case CLM_URL_COMPONENT_PASSWORD: allowed = "!$&'()*+,;=:"; break;
	case CLM_URL_COMPONENT_PATH: allowed = "!$&'()*+,;=:@/"; break;
	case CLM_URL_COMPONENT_QUERY:
	case CLM_URL_COMPONENT_FRAGMENT: allowed = "!$&'()*+,;=:@/?"; break;
	default: return 1;
	}
	const char* text = clm_path_internal_text(source);
	if (component == CLM_URL_COMPONENT_SCHEME && (!text || !uriIsWellFormedSchemeA(text, text + strlen(text)))) return 1;
	return clm_url_internal_commit(destination, clm_url_internal_encode(text, allowed, 0));
}

clm_int clm_url_decode_component(clm_id source, clm_id destination) {
	return clm_url_internal_commit(destination, clm_url_internal_decode(clm_path_internal_text(source), 0, 0));
}

clm_int clm_url_encode_path_segment(clm_id source, clm_id destination) {
	return clm_url_internal_commit(destination, clm_url_internal_encode(clm_path_internal_text(source), "", 0));
}

clm_int clm_url_encode_query_value(clm_id source, clm_id destination) {
	return clm_url_internal_commit(destination, clm_url_internal_encode(clm_path_internal_text(source), "", 0));
}

clm_int clm_url_form_encode(clm_id source, clm_id destination) {
	return clm_url_internal_commit(destination, clm_url_internal_encode(clm_path_internal_text(source), "", 1));
}

clm_int clm_url_form_decode(clm_id source, clm_id destination) {
	return clm_url_internal_commit(destination, clm_url_internal_decode(clm_path_internal_text(source), 1, 0));
}

clm_int clm_url_get_path_segment_count(clm_id source, clm_uint* count) {
	return clm_url_internal_segment(source, 0, 0, NULL, 0, count);
}

clm_int clm_url_get_path_segment(clm_id source, clm_uint index, clm_id destination) {
	return clm_url_internal_segment(source, 1, index, NULL, destination, NULL);
}

clm_int clm_url_set_path_segment(clm_id url, clm_uint index, clm_id value) {
	return clm_url_internal_segment(url, 2, index, clm_path_internal_text(value), 0, NULL);
}

clm_int clm_url_append_path_segment(clm_id url, clm_id value) {
	return clm_url_internal_segment(url, 3, 0, clm_path_internal_text(value), 0, NULL);
}

clm_int clm_url_remove_path_segment(clm_id url, clm_uint index) {
	return clm_url_internal_segment(url, 4, index, NULL, 0, NULL);
}

clm_int clm_url_get_parent(clm_id source, clm_id destination) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	const char* path = text + parts.path.offset;
	if (parts.scheme.present && !parts.authority.present && parts.path.length && *path != '/') return 1;
	size_t end = parts.path.length;
	while (end > 1 && path[end - 1] == '/') --end;
	while (end && path[end - 1] != '/') --end;
	char* parent = clm_url_internal_substring(path, 0, end);
	char* output = parent ? clm_url_internal_edit(text, CLM_URL_COMPONENT_PATH, parent) : NULL;
	free(parent);
	if (output) { char* next = clm_url_internal_edit(output, CLM_URL_COMPONENT_QUERY, NULL); free(output); output = next; }
	if (output) { char* next = clm_url_internal_edit(output, CLM_URL_COMPONENT_FRAGMENT, NULL); free(output); output = next; }
	return clm_url_internal_commit(destination, output);
}

clm_int clm_url_get_filename(clm_id source, clm_id destination) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	size_t first = parts.path.offset + parts.path.length;
	while (first > parts.path.offset && text[first - 1] != '/') --first;
	return clm_path_internal_slice(destination, text + first, parts.path.offset + parts.path.length - first);
}

clm_int clm_url_get_extension(clm_id source, clm_id destination) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	const size_t end = parts.path.offset + parts.path.length;
	size_t first = end, extension = end;
	while (first > parts.path.offset && text[first - 1] != '/') --first;
	for (size_t cursor = end; cursor > first + 1; --cursor) if (text[cursor - 1] == '.') { extension = cursor - 1; break; }
	return clm_path_internal_slice(destination, text + extension, end - extension);
}

clm_int clm_url_change_extension(clm_id url, clm_id extension) {
	const char* text = clm_path_internal_text(url);
	const char* value = clm_path_internal_text(extension);
	clm_url_parts parts;
	if (!value || strpbrk(value, "/\\") || clm_url_internal_parts(text, &parts)) return 1;
	const size_t end = parts.path.offset + parts.path.length;
	size_t first = end, dot = end;
	while (first > parts.path.offset && text[first - 1] != '/') --first;
	if (first == end) return 1;
	for (size_t cursor = end; cursor > first + 1; --cursor) if (text[cursor - 1] == '.') { dot = cursor - 1; break; }
	if (*value == '.') ++value;
	char* encoded = clm_url_internal_encode(value, "", 0);
	char* output = encoded ? clm_url_internal_splice(text, dot, end - dot, *encoded ? "." : "", encoded, "") : NULL;
	free(encoded);
	return clm_url_internal_commit(url, output);
}

clm_int clm_url_ensure_ending_separator(clm_id url) {
	const char* text = clm_path_internal_text(url);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	const char* path = text + parts.path.offset;
	if (parts.scheme.present && !parts.authority.present && parts.path.length && *path != '/') return 1;
	if (parts.path.length && path[parts.path.length - 1] == '/') return 0;
	char* updated = clm_url_internal_splice(text, parts.path.offset + parts.path.length, 0, "", "/", "");
	return clm_url_internal_commit(url, updated);
}

clm_int clm_url_trim_ending_separator(clm_id url) {
	const char* text = clm_path_internal_text(url);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	const char* path = text + parts.path.offset;
	if (parts.scheme.present && !parts.authority.present && parts.path.length && *path != '/') return 1;
	size_t length = parts.path.length;
	while (length > 1 && path[length - 1] == '/') --length;
	return clm_url_internal_commit(url, clm_url_internal_splice(text, parts.path.offset + length, parts.path.length - length, "", "", ""));
}

clm_int clm_url_query_count(clm_id source, clm_uint* count) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	if (!count || clm_url_internal_parts(text, &parts)) return 1;
	size_t size;
	clm_url_internal_field* fields = clm_url_internal_fields(text, parts.query, &size);
	if (!fields) return 1;
	free(fields); *count = (clm_uint)size;
	return 0;
}

clm_int clm_url_query_get_at(clm_id source, clm_uint index, clm_id key, clm_id value, clm_int* has_value) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	if (!has_value || key == value || !clm_path_internal_text(key) || !clm_path_internal_text(value) || clm_url_internal_parts(text, &parts)) return 1;
	size_t count;
	clm_url_internal_field* fields = clm_url_internal_fields(text, parts.query, &count);
	if (!fields || index >= count) { free(fields); return 1; }
	const clm_url_internal_field field = fields[index];
	free(fields);
	char* key_copy = NULL;
	char* value_copy = NULL;
	clm_uint key_length, value_length;
	const size_t offset = field.offset + field.key_length + (size_t)field.has_value;
	const size_t size = field.length - field.key_length - (size_t)field.has_value;
	if (clm_bpa_copy_string(field.key_length ? text + field.offset : "", field.key_length ? (clm_uint)field.key_length : 1,
		&key_copy, &key_length) != 0) return 1;
	if (clm_bpa_copy_string(size ? text + offset : "", size ? (clm_uint)size : 1, &value_copy, &value_length) != 0) {
		clm_bpa_pool_free(key_copy, (size_t)key_length + 1); return 1;
	}
	clm_bpa_block_node* key_block = clm_bpa_find_block(key);
	clm_bpa_block_node* value_block = clm_bpa_find_block(value);
	clm_bpa_pool_free(key_block->data, (size_t)key_block->length + 1);
	clm_bpa_pool_free(value_block->data, (size_t)value_block->length + 1);
	key_block->data = key_copy; key_block->length = key_length; key_block->encoding = CLM_STRING_ENCODING_UTF8;
	value_block->data = value_copy; value_block->length = value_length; value_block->encoding = CLM_STRING_ENCODING_UTF8;
	*has_value = field.has_value;
	return 0;
}

clm_int clm_url_query_has(clm_id source, clm_id key) {
	const char* text = clm_path_internal_text(source);
	const char* name = clm_path_internal_text(key);
	clm_url_parts parts;
	if (!name || clm_url_internal_parts(text, &parts)) return -1;
	size_t count;
	clm_url_internal_field* fields = clm_url_internal_fields(text, parts.query, &count);
	if (!fields) return -1;
	int found = 0;
	for (size_t index = 0; index < count; ++index) found |= clm_url_internal_field_matches(text, fields[index], name);
	free(fields);
	return found;
}

clm_int clm_url_query_get(clm_id source, clm_id key, clm_uint occurrence, clm_id destination) {
	const char* text = clm_path_internal_text(source);
	const char* name = clm_path_internal_text(key);
	clm_url_parts parts;
	if (!name || clm_url_internal_parts(text, &parts)) return 1;
	size_t count;
	clm_url_internal_field* fields = clm_url_internal_fields(text, parts.query, &count);
	if (!fields) return 1;
	int status = 1;
	for (size_t index = 0; index < count; ++index) {
		if (!clm_url_internal_field_matches(text, fields[index], name)) continue;
		if (occurrence) { --occurrence; continue; }
		const clm_url_internal_field field = fields[index];
		const size_t prefix = field.key_length + (size_t)field.has_value;
		status = clm_path_internal_slice(destination, text + field.offset + prefix, field.length - prefix);
		break;
	}
	free(fields);
	return status;
}

clm_int clm_url_query_append(clm_id url, clm_id key, clm_id value, clm_int has_value) {
	return clm_url_internal_query_edit(url, 0, clm_path_internal_text(key), has_value ? clm_path_internal_text(value) : NULL, has_value != 0, 0);
}

clm_int clm_url_query_set(clm_id url, clm_id key, clm_id value, clm_int has_value) {
	return clm_url_internal_query_edit(url, 1, clm_path_internal_text(key), has_value ? clm_path_internal_text(value) : NULL, has_value != 0, 0);
}

clm_int clm_url_query_remove(clm_id url, clm_id key) {
	const char* name = clm_path_internal_text(key);
	return name ? clm_url_internal_query_edit(url, 2, name, NULL, 0, 0) : 1;
}

clm_int clm_url_query_remove_at(clm_id url, clm_uint index) {
	return clm_url_internal_query_edit(url, 3, NULL, NULL, 0, index);
}

clm_int clm_url_query_sort(clm_id url) {
	return clm_url_internal_query_edit(url, 4, NULL, NULL, 0, 0);
}

clm_int clm_url_strip_credentials(clm_id url) {
	return clm_url_remove_component(url, CLM_URL_COMPONENT_USERINFO);
}

clm_int clm_url_redact(clm_id source, clm_id destination) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts)) return 1;
	if (!parts.scheme.present || !parts.authority.present || !parts.host.length) return clm_path_internal_store(destination, "[redacted]");
	char* output = clm_url_internal_edit(text, CLM_URL_COMPONENT_USERINFO, NULL);
	const clm_url_component selected[] = {CLM_URL_COMPONENT_PATH, CLM_URL_COMPONENT_QUERY, CLM_URL_COMPONENT_FRAGMENT};
	for (size_t index = 0; output && index < 3; ++index) {
		char* next = clm_url_internal_edit(output, selected[index], index == 0 ? "/" : NULL);
		free(output); output = next;
	}
	return clm_url_internal_commit(destination, output);
}

clm_int clm_url_from_file_path(clm_id path, clm_id destination) {
	const char* text = clm_path_internal_text(path);
	if (!text || !clm_path_internal_qualified(text) || !clm_path_internal_valid(text) || strlen(text) > ((size_t)INT_MAX - 9) / 3) return 1;
#if defined(_WIN32)
	if (!strncmp(text, "\\\\?\\", 4)) return 1;
#endif
	char* output = (char*)malloc(strlen(text) * 3 + 9);
	if (!output) return 1;
#if defined(_WIN32)
	const int status = uriWindowsFilenameToUriStringA(text, output);
#else
	const int status = uriUnixFilenameToUriStringA(text, output);
#endif
	if (status != URI_SUCCESS) { free(output); return 1; }
	return clm_url_internal_commit(destination, output);
}

clm_int clm_url_to_file_path(clm_id source, clm_id destination) {
	const char* text = clm_path_internal_text(source);
	clm_url_parts parts;
	if (clm_url_internal_parts(text, &parts) || clm_url_internal_scheme(text, "file") != 1 ||
		parts.query.present || parts.fragment.present || parts.userinfo.present || parts.port.present || !parts.path.length ||
		text[parts.path.offset] != '/') return 1;
	for (size_t cursor = parts.path.offset; cursor < parts.path.offset + parts.path.length; ++cursor) {
		if (text[cursor] != '%') continue;
		const int character = clm_url_internal_hex((unsigned char)text[cursor + 1]) * 16 + clm_url_internal_hex((unsigned char)text[cursor + 2]);
		if (!character || character == '/' || character == '\\') return 1;
		cursor += 2;
	}
	char* input = clm_path_internal_copy(text);
	if (!input) return 1;
	if (parts.host.length == 9) {
		char* host = clm_url_internal_substring(text, parts.host.offset, parts.host.length);
		int local = host != NULL;
		for (size_t index = 0; host && index < 9; ++index) local &= tolower((unsigned char)host[index]) == "localhost"[index];
		free(host);
		if (local) { free(input); input = clm_url_internal_edit(text, CLM_URL_COMPONENT_HOST, ""); parts.host.length = 0; }
	}
#if !defined(_WIN32)
	if (parts.host.length) { free(input); return 1; }
#endif
	if (!input) return 1;
	char* output = (char*)malloc(strlen(input) + 3);
	if (!output) { free(input); return 1; }
#if defined(_WIN32)
	const int status = uriUriStringToWindowsFilenameA(input, output);
#else
	const int status = uriUriStringToUnixFilenameA(input, output);
#endif
	free(input);
	if (status != URI_SUCCESS || !clm_path_internal_qualified(output) || !clm_path_internal_valid(output)) { free(output); return 1; }
	return clm_url_internal_commit(destination, output);
}
