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
#include "table.h"
#include "file.h"

#include <limits.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

/** Options shared by DSV parsing and serialization.
 * A NULL options pointer selects comma separation and a header row. A custom
 * separator is an existing CLM string containing one or two ASCII bytes; ID 0
 * also selects comma. With has_header enabled, the first record supplies
 * column names, which parsing auto-registers in clm_id. When has_header is
 * false, parsing retains all records and generates column_1, column_2, ...
 * from the first record's field count; serialization omits the header row.
 * Legacy mode (csv_quoting=false) supports single/double quotes with CLM
 * backslash escapes. CSV mode (csv_quoting=true) uses RFC 4180-style doubled
 * double quotes, literal backslashes, and multiline quoted fields. Both modes
 * continue to quote every field when serializing.
 */
typedef struct {
	clm_id separator_string_id;
	bool has_header;
	bool csv_quoting; /* false preserves CLM's existing backslash-escape mode */
} clm_string_dsv_options;

#define CLM_STRING_DSV_OPTIONS_DEFAULT \
	{0, true, false}

typedef struct {
	clm_id source_string_id;
	size_t length;
	size_t position;
	char separator[2];
	size_t separator_length;
	bool csv_quoting;
} clm_string_dsv_internal_parser;

typedef struct {
	char** names;
	size_t count;
	size_t capacity;
} clm_string_dsv_internal_registered_names;

typedef struct {
	char* data;
	size_t length;
	size_t capacity;
} clm_string_dsv_internal_writer;

clm_int clm_string_dsv_internal_new_temp_string(
	const clm_id avoid_a,
	const clm_id avoid_b,
	clm_id* out_id);

clm_int clm_string_dsv_internal_options(
	const clm_string_dsv_options* options,
	char separator[2],
	size_t* separator_length,
	bool* has_header);

bool clm_string_dsv_internal_is_separator(
	const clm_string_dsv_internal_parser* parser,
	const size_t position,
	const char* text);

/** Decodes one field into a CLM string and advances past its terminator. */
clm_int clm_string_dsv_internal_parse_field(
	clm_string_dsv_internal_parser* parser,
	const clm_id field_string_id,
	bool* has_separator,
	bool* record_end);

clm_int clm_string_dsv_internal_allocate_column_id(
	clm_id* out_id);

clm_int clm_string_dsv_internal_track_name(
	clm_string_dsv_internal_registered_names* registered,
	const char* name,
	const size_t length,
	char** copy_out);

/** Registers a column name if needed and adds it to the table schema.
 * New registrations are tracked for rollback if the parse fails. */
clm_int clm_string_dsv_internal_add_column_name(
    const clm_id table_id,
    const char* name,
    clm_string_dsv_internal_registered_names* registered);

clm_int clm_string_dsv_internal_add_header_column(
    const clm_id table_id,
    const clm_id name_string_id,
    clm_string_dsv_internal_registered_names* registered);

void clm_string_dsv_internal_release_names(
	clm_string_dsv_internal_registered_names* registered,
	const bool rollback);

clm_int clm_string_dsv_internal_parse(
	const clm_id source_string_id,
	const clm_id destination_table_id,
	const clm_string_dsv_options* options);

/** Parses an existing CLM string block into a new DSV table using defaults. 
 * @param source_string_id CLM ID identifying the source string.
 * @param destination_table_id CLM ID identifying the destination table.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_dsv_loads(
	const clm_id source_string_id,
	const clm_id destination_table_id);

/** Parses an existing CLM string block using explicit DSV options. 
 * @param source_string_id CLM ID identifying the source string.
 * @param destination_table_id CLM ID identifying the destination table.
 * @param options Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_dsv_loads_with_options(
	const clm_id source_string_id,
	const clm_id destination_table_id,
	const clm_string_dsv_options* options);

/** Reads UTF-8 DSV text from a CLM filename using explicit options. 
 * @param filename_string_id CLM ID identifying the filename string.
 * @param destination_table_id CLM ID identifying the destination table.
 * @param options Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_dsv_load_with_options(
	const clm_id filename_string_id,
	const clm_id destination_table_id,
	const clm_string_dsv_options* options);

/** Reads UTF-8 DSV text from a CLM filename using default options. 
 * @param filename_string_id CLM ID identifying the filename string.
 * @param destination_table_id CLM ID identifying the destination table.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_dsv_load(
	const clm_id filename_string_id,
	const clm_id destination_table_id);

clm_int clm_string_dsv_internal_writer_append(
	clm_string_dsv_internal_writer* writer,
	const char* text,
	const size_t length);

clm_int clm_string_dsv_internal_writer_append_quoted(
	clm_string_dsv_internal_writer* writer,
	const clm_id value_string_id,
    const bool csv_quoting);

clm_int clm_string_dsv_internal_load_column_name(
	const clm_id table_id,
	const clm_int column_index,
	const clm_id destination_string_id);

/** Serializes a DSV table into a CLM string using explicit options. 
 * @param table_id CLM ID identifying the table.
 * @param destination_string_id CLM ID identifying the destination string.
 * @param options Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_dsv_dumps_with_options(
	const clm_id table_id,
	const clm_id destination_string_id,
	const clm_string_dsv_options* options);

/** Serializes a DSV table into an existing CLM string block using defaults. 
 * @param table_id CLM ID identifying the table.
 * @param destination_string_id CLM ID identifying the destination string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_dsv_dumps(
	const clm_id table_id,
	const clm_id destination_string_id);

/** Writes a DSV table to a CLM filename using explicit options. 
 * @param table_id CLM ID identifying the table.
 * @param filename_string_id CLM ID identifying the filename string.
 * @param options Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_dsv_dump_with_options(
	const clm_id table_id,
	const clm_id filename_string_id,
	const clm_string_dsv_options* options);

/** Writes a DSV table to a CLM filename using default options. 
 * @param table_id CLM ID identifying the table.
 * @param filename_string_id CLM ID identifying the filename string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_dsv_dump(
	const clm_id table_id,
	const clm_id filename_string_id);


