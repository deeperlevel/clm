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
#include "string.h"

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

bool clm_ua_internal_equal_case_insensitive(
	const char left,
	const char right);

const char* clm_ua_internal_find_case_insensitive(
	const char* text,
	const char* token);

void clm_ua_internal_copy(
	char* destination,
	const size_t capacity,
	const char* source);

bool clm_ua_internal_version_from(
	const char* user_agent,
	const char* token,
	char* version,
	const size_t capacity,
	const bool normalize_underscores);

void clm_ua_internal_browser(
	const char* user_agent,
	char* name,
	const size_t name_capacity,
	char* version,
	const size_t version_capacity);

void clm_ua_internal_os(
	const char* user_agent,
	char* name,
	const size_t name_capacity,
	char* version,
	const size_t version_capacity);

bool clm_ua_internal_is_bot(const char* user_agent);

clm_ua_device clm_ua_internal_device_type(
	const char* user_agent);

clm_int clm_ua_internal_get_text(
	const clm_id user_agent_string_id,
	const char** text);

clm_int clm_ua_internal_store_text(
	const clm_id destination_string_id,
	const char* text);

/** Extracts browser name/version from a CLM string ID into CLM string IDs. 
 * @param user_agent_string_id CLM ID identifying the user agent string.
 * @param name_string_id CLM ID identifying the name string.
 * @param version_string_id CLM ID identifying the version string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_browser(
	const clm_id user_agent_string_id,
	const clm_id name_string_id,
	const clm_id version_string_id);

/** Extracts browser name/version from raw NUL-terminated UTF-8 input into CLM
 * string IDs. Use clm_ua_browser() when the source is already a CLM string.
 * @param user_agent Input data or buffer; not modified.
 * @param name_string_id CLM ID identifying the name string.
 * @param version_string_id CLM ID identifying the version string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_browser_from(
	const char* user_agent,
	const clm_id name_string_id,
	const clm_id version_string_id);

/** Extracts browser name/version into caller-provided NUL-terminated buffers. 
 * @param user_agent_string_id CLM ID identifying the user agent string.
 * @param name Writable output or in/out pointer.
 * @param name_capacity Input parameter described above.
 * @param version Writable output or in/out pointer.
 * @param version_capacity Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_browser_to(
	const clm_id user_agent_string_id,
	char* name,
	const size_t name_capacity,
	char* version,
	const size_t version_capacity);

/** Extracts OS name/version from a CLM string ID into CLM string IDs. 
 * @param user_agent_string_id CLM ID identifying the user agent string.
 * @param name_string_id CLM ID identifying the name string.
 * @param version_string_id CLM ID identifying the version string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_os(
	const clm_id user_agent_string_id,
	const clm_id name_string_id,
	const clm_id version_string_id);

/** Extracts OS name/version from raw NUL-terminated UTF-8 input into CLM
 * string IDs. Use clm_ua_os() when the source is already a CLM string.
 * @param user_agent Input data or buffer; not modified.
 * @param name_string_id CLM ID identifying the name string.
 * @param version_string_id CLM ID identifying the version string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_os_from(
	const char* user_agent,
	const clm_id name_string_id,
	const clm_id version_string_id);

/** Extracts OS name/version into caller-provided NUL-terminated buffers. 
 * @param user_agent_string_id CLM ID identifying the user agent string.
 * @param name Writable output or in/out pointer.
 * @param name_capacity Input parameter described above.
 * @param version Writable output or in/out pointer.
 * @param version_capacity Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_os_to(
	const clm_id user_agent_string_id,
	char* name,
	const size_t name_capacity,
	char* version,
	const size_t version_capacity);

/** Checks bot markers in a CLM string; out_is_bot is unchanged on failure. 
 * @param user_agent_string_id CLM ID identifying the user agent string.
 * @param out_is_bot Receives the is bot.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_is_bot(
	const clm_id user_agent_string_id,
	bool* out_is_bot);

/** Checks bot markers in raw NUL-terminated UTF-8 input. 
 * @param user_agent Input data or buffer; not modified.
 * @param out_is_bot Receives the is bot.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_is_bot_from(
	const char* user_agent,
	bool* out_is_bot);

/** Detects a coarse device type from a CLM string. 
 * @param user_agent_string_id CLM ID identifying the user agent string.
 * @param out_device_type Receives the device type.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_device_type(
	const clm_id user_agent_string_id,
	clm_ua_device* out_device_type);

/** Detects a coarse device type from raw NUL-terminated UTF-8 input. 
 * @param user_agent Input data or buffer; not modified.
 * @param out_device_type Receives the device type.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_device_type_from(
	const char* user_agent,
	clm_ua_device* out_device_type);

/** Parses a CLM string into a summary record. The raw source is represented by
 * its ID; all fixed text fields are copied into out. The record contains no
 * borrowed C-string pointer.
 * @param user_agent_string_id CLM ID identifying the user agent string.
 * @param out Writable output buffer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_parse(
	const clm_id user_agent_string_id,
	clm_ua_info* out);

/** Parses raw NUL-terminated UTF-8 input into a summary record. raw_string_id
 * is zero because the source did not come from a CLM string block.
 * @param user_agent Input data or buffer; not modified.
 * @param out Writable output buffer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_ua_parse_from(
	const char* user_agent,
	clm_ua_info* out);