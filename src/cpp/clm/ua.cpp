#pragma once

/*
 * =============================================================================
 * CLM â€” Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain â€” CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#include "base.hpp"
#include "string.hpp"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#define CLM_UA_NAME_MAX 32
#define CLM_UA_VERSION_MAX 64

#define CLM_UA_OPERATION_SUCCESS 0
#define CLM_UA_OPERATION_FAILURE -1

/** Coarse device category inferred from common User-Agent tokens. */
typedef enum clm_ua_device {
	CLM_UA_DESKTOP = 0,
	CLM_UA_MOBILE = 1,
	CLM_UA_TABLET = 2
} clm_ua_device;

/** Parsed User-Agent summary. Text members are bounded UTF-8 C buffers because
 * this is a caller-owned result record; raw_string_id identifies the source
 * CLM string without borrowing a raw pointer. _from parsing sets it to zero.
 */
typedef struct clm_ua_info {
	clm_id raw_string_id;
	char browser[CLM_UA_NAME_MAX];
	char browser_version[CLM_UA_VERSION_MAX];
	char os[CLM_UA_NAME_MAX];
	char os_version[CLM_UA_VERSION_MAX];
	clm_ua_device device_type;
	bool is_mobile;
	bool is_tablet;
	bool is_desktop;
	bool is_bot;
} clm_ua_info;

static inline bool clm_ua_internal_equal_case_insensitive(
	const char left,
	const char right)
{
	return tolower((unsigned char)left) == tolower((unsigned char)right);
}

static inline const char* clm_ua_internal_find_case_insensitive(
	const char* text,
	const char* token)
{
	if (text == NULL || token == NULL)
		return NULL;
	for (const char* start = text; *start != '\0'; ++start) {
		const char* left = start;
		const char* right = token;
		while (*left != '\0' && *right != '\0' &&
			clm_ua_internal_equal_case_insensitive(*left, *right)) {
			++left;
			++right;
		}
		if (*right == '\0')
			return start;
	}
	return NULL;
}

static inline void clm_ua_internal_copy(
	char* destination,
	const size_t capacity,
	const char* source)
{
	if (destination == NULL || capacity == 0)
		return;
	size_t index = 0;
	while (source != NULL && source[index] != '\0' && index + 1 < capacity) {
		destination[index] = source[index];
		++index;
	}
	destination[index] = '\0';
}

static inline bool clm_ua_internal_version_from(
	const char* user_agent,
	const char* token,
	char* version,
	const size_t capacity,
	const bool normalize_underscores)
{
	if (version == NULL || capacity == 0)
		return false;
	const char* position = clm_ua_internal_find_case_insensitive(user_agent,
		token);
	if (position == NULL)
		return false;
	position += strlen(token);
	size_t length = 0;
	while (*position != '\0' && length + 1 < capacity &&
		(isdigit((unsigned char)*position) || *position == '.' ||
		 (normalize_underscores && *position == '_'))) {
		version[length++] = *position == '_' ? '.' : *position;
		++position;
	}
	if (length == 0)
		return false;
	version[length] = '\0';
	return true;
}

static inline void clm_ua_internal_browser(
	const char* user_agent,
	char* name,
	const size_t name_capacity,
	char* version,
	const size_t version_capacity)
{
	clm_ua_internal_copy(name, name_capacity, "Unknown");
	clm_ua_internal_copy(version, version_capacity, "Unknown");
	if (user_agent == NULL || name == NULL || name_capacity == 0 ||
		version == NULL || version_capacity == 0)
		return;

	static const struct {
		const char* name;
		const char* token;
	} patterns[] = {
		{"Edge", "Edg/"},
		{"Edge", "Edge/"},
		{"Edge", "EdgA/"},
		{"Edge", "EdgiOS/"},
		{"Opera", "OPR/"},
		{"Opera", "Opera/"},
		{"Samsung Internet", "SamsungBrowser/"},
		{"Firefox", "Firefox/"},
		{"Firefox", "FxiOS/"},
		{"Chrome", "Chrome/"},
		{"Chrome", "CriOS/"}
	};
	for (size_t index = 0; index < sizeof(patterns) / sizeof(patterns[0]);
		++index) {
		if (clm_ua_internal_version_from(user_agent, patterns[index].token,
			version, version_capacity, false)) {
			clm_ua_internal_copy(name, name_capacity, patterns[index].name);
			return;
		}
	}
	if (clm_ua_internal_find_case_insensitive(user_agent, "Safari/") != NULL &&
		clm_ua_internal_version_from(user_agent, "Version/", version,
			version_capacity, false))
		clm_ua_internal_copy(name, name_capacity, "Safari");
}

static inline void clm_ua_internal_os(
	const char* user_agent,
	char* name,
	const size_t name_capacity,
	char* version,
	const size_t version_capacity)
{
	clm_ua_internal_copy(name, name_capacity, "Unknown");
	clm_ua_internal_copy(version, version_capacity, "Unknown");
	if (user_agent == NULL || name == NULL || name_capacity == 0 ||
		version == NULL || version_capacity == 0)
		return;

	static const struct {
		const char* name;
		const char* token;
		bool normalize_underscores;
	} patterns[] = {
		{"Windows", "Windows NT ", false},
		{"iOS", "iPhone OS ", true},
		{"iOS", "CPU iPhone OS ", true},
		{"iOS", "CPU OS ", true},
		{"Android", "Android ", false},
		{"Android", "Android/", false},
		{"macOS", "Mac OS X ", true}
	};
	for (size_t index = 0; index < sizeof(patterns) / sizeof(patterns[0]);
		++index) {
		if (clm_ua_internal_version_from(user_agent, patterns[index].token,
			version, version_capacity, patterns[index].normalize_underscores)) {
			clm_ua_internal_copy(name, name_capacity, patterns[index].name);
			return;
		}
	}
	if (clm_ua_internal_find_case_insensitive(user_agent, "Linux") != NULL)
		clm_ua_internal_copy(name, name_capacity, "Linux");
}

static inline bool clm_ua_internal_is_bot(const char* user_agent)
{
	static const char* const tokens[] = {
		"bot", "crawler", "spider", "slurp", "bingpreview"
	};
	for (size_t index = 0; index < sizeof(tokens) / sizeof(tokens[0]); ++index)
		if (clm_ua_internal_find_case_insensitive(user_agent, tokens[index]) != NULL)
			return true;
	return false;
}

static inline clm_ua_device clm_ua_internal_device_type(
	const char* user_agent)
{
	if (clm_ua_internal_find_case_insensitive(user_agent, "ipad") != NULL ||
		clm_ua_internal_find_case_insensitive(user_agent, "tablet") != NULL ||
		clm_ua_internal_find_case_insensitive(user_agent, "kindle") != NULL ||
		clm_ua_internal_find_case_insensitive(user_agent, "silk/") != NULL)
		return CLM_UA_TABLET;
	if (clm_ua_internal_find_case_insensitive(user_agent, "android") != NULL)
		return clm_ua_internal_find_case_insensitive(user_agent, "mobile") != NULL
			? CLM_UA_MOBILE : CLM_UA_TABLET;
	if (clm_ua_internal_find_case_insensitive(user_agent, "mobi") != NULL ||
		clm_ua_internal_find_case_insensitive(user_agent, "iphone") != NULL ||
		clm_ua_internal_find_case_insensitive(user_agent, "ipod") != NULL)
		return CLM_UA_MOBILE;
	return CLM_UA_DESKTOP;
}

static inline clm_int clm_ua_internal_get_text(
	const clm_id user_agent_string_id,
	const char** text)
{
	clm_bpa_block_node* block;
	size_t byte_length;
	size_t character_count;
	if (text == NULL || clm_string_get_block(user_agent_string_id, &block,
			text, &byte_length, &character_count) != CLM_BPA_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	return CLM_UA_OPERATION_SUCCESS;
}

static inline clm_int clm_ua_internal_store_text(
	const clm_id destination_string_id,
	const char* text)
{
	return clm_string_set(destination_string_id, text) >= 0
		? CLM_UA_OPERATION_SUCCESS : CLM_UA_OPERATION_FAILURE;
}

/** Extracts browser name/version from a CLM string ID into CLM string IDs. */
static inline clm_int clm_ua_browser(
	const clm_id user_agent_string_id,
	const clm_id name_string_id,
	const clm_id version_string_id)
{
	const char* user_agent;
	char name[CLM_UA_NAME_MAX];
	char version[CLM_UA_VERSION_MAX];
	if (name_string_id == version_string_id ||
		clm_ua_internal_get_text(user_agent_string_id, &user_agent) !=
			CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	clm_ua_internal_browser(user_agent, name, sizeof(name), version,
		sizeof(version));
	if (clm_ua_internal_store_text(name_string_id, name) !=
		CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	return clm_ua_internal_store_text(version_string_id, version);
}

/** Extracts browser name/version from raw NUL-terminated UTF-8 input into CLM
 * string IDs. Use clm_ua_browser() when the source is already a CLM string.
 */
static inline clm_int clm_ua_browser_from(
	const char* user_agent,
	const clm_id name_string_id,
	const clm_id version_string_id)
{
	char name[CLM_UA_NAME_MAX];
	char version[CLM_UA_VERSION_MAX];
	if (user_agent == NULL || name_string_id == version_string_id)
		return CLM_UA_OPERATION_FAILURE;
	clm_ua_internal_browser(user_agent, name, sizeof(name), version,
		sizeof(version));
	if (clm_ua_internal_store_text(name_string_id, name) !=
		CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	return clm_ua_internal_store_text(version_string_id, version);
}

/** Extracts browser name/version into caller-provided NUL-terminated buffers. */
static inline clm_int clm_ua_browser_to(
	const clm_id user_agent_string_id,
	char* name,
	const size_t name_capacity,
	char* version,
	const size_t version_capacity)
{
	const char* user_agent;
	if (name == NULL || version == NULL || name_capacity == 0 ||
		version_capacity == 0 || clm_ua_internal_get_text(user_agent_string_id,
			&user_agent) != CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	clm_ua_internal_browser(user_agent, name, name_capacity, version,
		version_capacity);
	return CLM_UA_OPERATION_SUCCESS;
}

/** Extracts OS name/version from a CLM string ID into CLM string IDs. */
static inline clm_int clm_ua_os(
	const clm_id user_agent_string_id,
	const clm_id name_string_id,
	const clm_id version_string_id)
{
	const char* user_agent;
	char name[CLM_UA_NAME_MAX];
	char version[CLM_UA_VERSION_MAX];
	if (name_string_id == version_string_id ||
		clm_ua_internal_get_text(user_agent_string_id, &user_agent) !=
			CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	clm_ua_internal_os(user_agent, name, sizeof(name), version,
		sizeof(version));
	if (clm_ua_internal_store_text(name_string_id, name) !=
		CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	return clm_ua_internal_store_text(version_string_id, version);
}

/** Extracts OS name/version from raw NUL-terminated UTF-8 input into CLM
 * string IDs. Use clm_ua_os() when the source is already a CLM string.
 */
static inline clm_int clm_ua_os_from(
	const char* user_agent,
	const clm_id name_string_id,
	const clm_id version_string_id)
{
	char name[CLM_UA_NAME_MAX];
	char version[CLM_UA_VERSION_MAX];
	if (user_agent == NULL || name_string_id == version_string_id)
		return CLM_UA_OPERATION_FAILURE;
	clm_ua_internal_os(user_agent, name, sizeof(name), version,
		sizeof(version));
	if (clm_ua_internal_store_text(name_string_id, name) !=
		CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	return clm_ua_internal_store_text(version_string_id, version);
}

/** Extracts OS name/version into caller-provided NUL-terminated buffers. */
static inline clm_int clm_ua_os_to(
	const clm_id user_agent_string_id,
	char* name,
	const size_t name_capacity,
	char* version,
	const size_t version_capacity)
{
	const char* user_agent;
	if (name == NULL || version == NULL || name_capacity == 0 ||
		version_capacity == 0 || clm_ua_internal_get_text(user_agent_string_id,
			&user_agent) != CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	clm_ua_internal_os(user_agent, name, name_capacity, version,
		version_capacity);
	return CLM_UA_OPERATION_SUCCESS;
}

/** Checks bot markers in a CLM string; out_is_bot is unchanged on failure. */
static inline clm_int clm_ua_is_bot(
	const clm_id user_agent_string_id,
	bool* out_is_bot)
{
	const char* user_agent;
	if (out_is_bot == NULL || clm_ua_internal_get_text(user_agent_string_id,
			&user_agent) != CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	const bool is_bot = clm_ua_internal_is_bot(user_agent);
	*out_is_bot = is_bot;
	return CLM_UA_OPERATION_SUCCESS;
}

/** Checks bot markers in raw NUL-terminated UTF-8 input. */
static inline clm_int clm_ua_is_bot_from(
	const char* user_agent,
	bool* out_is_bot)
{
	if (user_agent == NULL || out_is_bot == NULL)
		return CLM_UA_OPERATION_FAILURE;
	const bool is_bot = clm_ua_internal_is_bot(user_agent);
	*out_is_bot = is_bot;
	return CLM_UA_OPERATION_SUCCESS;
}

/** Detects a coarse device type from a CLM string. */
static inline clm_int clm_ua_device_type(
	const clm_id user_agent_string_id,
	clm_ua_device* out_device_type)
{
	const char* user_agent;
	if (out_device_type == NULL || clm_ua_internal_get_text(user_agent_string_id,
			&user_agent) != CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	const clm_ua_device device_type = clm_ua_internal_device_type(user_agent);
	*out_device_type = device_type;
	return CLM_UA_OPERATION_SUCCESS;
}

/** Detects a coarse device type from raw NUL-terminated UTF-8 input. */
static inline clm_int clm_ua_device_type_from(
	const char* user_agent,
	clm_ua_device* out_device_type)
{
	if (user_agent == NULL || out_device_type == NULL)
		return CLM_UA_OPERATION_FAILURE;
	const clm_ua_device device_type = clm_ua_internal_device_type(user_agent);
	*out_device_type = device_type;
	return CLM_UA_OPERATION_SUCCESS;
}

/** Parses a CLM string into a summary record. The raw source is represented by
 * its ID; all fixed text fields are copied into out. The record contains no
 * borrowed C-string pointer.
 */
static inline clm_int clm_ua_parse(
	const clm_id user_agent_string_id,
	clm_ua_info* out)
{
	const char* user_agent;
	if (out == NULL || clm_ua_internal_get_text(user_agent_string_id,
			&user_agent) != CLM_UA_OPERATION_SUCCESS)
		return CLM_UA_OPERATION_FAILURE;
	clm_ua_info parsed = {0};
	parsed.raw_string_id = user_agent_string_id;
	clm_ua_internal_browser(user_agent, parsed.browser, sizeof(parsed.browser),
		parsed.browser_version, sizeof(parsed.browser_version));
	clm_ua_internal_os(user_agent, parsed.os, sizeof(parsed.os),
		parsed.os_version, sizeof(parsed.os_version));
	parsed.device_type = clm_ua_internal_device_type(user_agent);
	parsed.is_mobile = parsed.device_type == CLM_UA_MOBILE;
	parsed.is_tablet = parsed.device_type == CLM_UA_TABLET;
	parsed.is_desktop = parsed.device_type == CLM_UA_DESKTOP;
	parsed.is_bot = clm_ua_internal_is_bot(user_agent);
	*out = parsed;
	return CLM_UA_OPERATION_SUCCESS;
}

/** Parses raw NUL-terminated UTF-8 input into a summary record. raw_string_id
 * is zero because the source did not come from a CLM string block.
 */
static inline clm_int clm_ua_parse_from(
	const char* user_agent,
	clm_ua_info* out)
{
	if (user_agent == NULL || out == NULL)
		return CLM_UA_OPERATION_FAILURE;
	clm_ua_info parsed = {0};
	clm_ua_internal_browser(user_agent, parsed.browser, sizeof(parsed.browser),
		parsed.browser_version, sizeof(parsed.browser_version));
	clm_ua_internal_os(user_agent, parsed.os, sizeof(parsed.os),
		parsed.os_version, sizeof(parsed.os_version));
	parsed.device_type = clm_ua_internal_device_type(user_agent);
	parsed.is_mobile = parsed.device_type == CLM_UA_MOBILE;
	parsed.is_tablet = parsed.device_type == CLM_UA_TABLET;
	parsed.is_desktop = parsed.device_type == CLM_UA_DESKTOP;
	parsed.is_bot = clm_ua_internal_is_bot(user_agent);
	*out = parsed;
	return CLM_UA_OPERATION_SUCCESS;
}
