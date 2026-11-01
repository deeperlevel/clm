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
#include "file.h"
#include "table.h"
#include "tree.h"

#include <stdbool.h>
#include <errno.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/** Token categories shared by format-specific parsers. */
typedef enum {
	CLM_STRING_FORMAT_TOKEN_WORD = 1,
	CLM_STRING_FORMAT_TOKEN_QUOTED,
	CLM_STRING_FORMAT_TOKEN_DELIMITER,
	CLM_STRING_FORMAT_TOKEN_NEWLINE,
	CLM_STRING_FORMAT_TOKEN_INDENT,
	CLM_STRING_FORMAT_TOKEN_END
} clm_string_format_token_kind;

#define CLM_FORMAT_DESERIALIZE_SUCCESS (clm_int)0
#define CLM_FORMAT_DESERIALIZE_FAILURE (clm_int)1
#define CLM_FORMAT_SERIALIZE_SUCCESS (clm_int)0
#define CLM_FORMAT_SERIALIZE_FAILURE (clm_int)1

#define CLM_STRING_FORMAT_LEX_EMIT_NEWLINES 0x01u
#define CLM_STRING_FORMAT_LEX_TRACK_INDENT 0x02u

/** Configurable lexical rules shared by format-specific parsers.
 * Delimiters, line-comment markers, and quote characters are NUL-terminated
 * sets of single-byte ASCII characters. Block-comment delimiters may contain
 * multiple bytes. Comments are recognized outside quoted strings.
 */
typedef struct {
	const char* delimiters;
	const char* line_comment_markers;
	const char* block_comment_start;
	const char* block_comment_end;
	const char* quote_characters;
	char escape_character;
	clm_byte flags;
} clm_string_format_lex_rules;

/** A token refers to bytes in the original input; it does not own text. */
typedef struct {
	clm_string_format_token_kind kind;
	size_t offset;
	size_t length;
	size_t line;
	size_t column;
} clm_string_format_token;

/** Stateful lexer borrowing its UTF-8 input buffer and lexical rules.
 * Both must remain alive and unchanged until lexing is complete.
 */
typedef struct {
	const char* input;
	size_t input_length;
	size_t offset;
	size_t line;
	size_t column;
	size_t line_start;
	const clm_string_format_lex_rules* rules;
	bool at_line_start;
} clm_string_format_lexer;

/** Parser error location and category; zero code means no error is recorded. */
typedef struct {
	clm_int code;
	size_t offset;
	size_t line;
	size_t column;
} clm_string_format_error;

/** New clm_id mappings owned by one parse operation until commit or rollback. */
typedef struct {
    char** names;
    size_t count;
    size_t capacity;
} clm_string_format_key_registry;

/** Parser cursor with one-token lookahead and the most recent parse error. */
typedef struct {
	clm_string_format_lexer lexer;
	clm_string_format_token lookahead;
	clm_string_format_error error;
	bool has_lookahead;
    clm_string_format_key_registry* new_keys; /* NULL = strict pre-registered IDs */
} clm_string_format_parser;

typedef enum {
	CLM_STRING_FORMAT_ROOT_OBJECT = 1,
	CLM_STRING_FORMAT_ROOT_ARRAY = 2
} clm_string_format_root_kind;

/** Format-specific grammar callback for populating a new root tree object. */
typedef clm_int (*clm_string_format_tree_parser_fn)(
	clm_string_format_parser* parser,
	clm_id root_object_id);

/** Format-specific emitter callback for serializing a tree to a string block. */
typedef clm_int (*clm_string_format_tree_serializer_fn)(
	clm_id root_object_id,
	clm_id destination_string_id);

/** Growable UTF-8 output buffer used by format-specific serializers. */
typedef struct {
	char* data;
	size_t length;
	size_t capacity;
} clm_string_format_writer;

void clm_string_format_writer_init(
	clm_string_format_writer* writer);

void clm_string_format_writer_dispose(
	clm_string_format_writer* writer);

clm_int clm_string_format_writer_reserve(
	clm_string_format_writer* writer,
	const size_t additional);

/** Appends exact UTF-8 bytes; the buffer is kept NUL-terminated. 
 * @param writer Writable output or in/out pointer.
 * @param text Input data or buffer; not modified.
 * @param length Length in bytes or elements, as specified above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_writer_append(
	clm_string_format_writer* writer,
	const char* text,
	const size_t length);

clm_int clm_string_format_writer_append_cstr(
	clm_string_format_writer* writer,
	const char* text);

clm_int clm_string_format_writer_append_char(
	clm_string_format_writer* writer,
	const char character);

/** Appends a JSON/YAML double-quoted UTF-8 string with JSON escapes. 
 * @param writer Writable output or in/out pointer.
 * @param text Input data or buffer; not modified.
 * @param length Length in bytes or elements, as specified above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_writer_append_quoted(
	clm_string_format_writer* writer,
	const char* text,
	const size_t length);

/** Atomically replaces an existing CLM string with the completed output. 
 * @param writer Writable output or in/out pointer.
 * @param destination_string_id CLM ID identifying the destination string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_writer_commit(
	clm_string_format_writer* writer,
	const clm_id destination_string_id);

/** Allocates an unused temporary CLM string ID outside both BPA registries. 
 * @param avoid_id CLM ID identifying the avoid.
 * @param out_id Receives the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_temp_string_new(
	const clm_id avoid_id,
	clm_id* out_id);

#define CLM_STRING_FORMAT_DELIMITED_HAS_HEADER 0x01u
#define CLM_STRING_FORMAT_DELIMITED_ALLOW_MULTILINE_FIELDS 0x02u
#define CLM_STRING_FORMAT_DELIMITED_WRITE_HEADER 0x01u
#define CLM_STRING_FORMAT_DELIMITED_QUOTE_ALL_FIELDS 0x02u

/** Rules for parsing delimited records into a string table.
 * If HAS_HEADER is clear, column_names supplies the mapped schema and must
 * remain alive for the duration of parsing. If HAS_HEADER is set, names come
 * from the first record and must already be registered in clm_id.
 */
typedef struct {
	char delimiter;
	char quote_character;
	clm_byte flags;
	const char* const* column_names;
	size_t column_name_count;
} clm_string_format_delimited_parse_rules;

/** Rules for serializing a string table as delimited text. */
typedef struct {
	char delimiter;
	char quote_character;
	clm_byte flags;
} clm_string_format_delimited_serialize_rules;

bool clm_string_format_internal_in_set(
	const char value,
	const char* set);

bool clm_string_format_internal_starts_with(
	const char* input,
	const size_t length,
	const size_t offset,
	const char* marker);

void clm_string_format_internal_advance(
	clm_string_format_lexer* lexer);

/** Parses delimited UTF-8 text into a newly created string table.
 * Rows must have the same number of fields. Quoted delimiters and doubled
 * quote characters are supported; multiline quoted fields are controlled by
 * ALLOW_MULTILINE_FIELDS. All parsed field values remain strings.
 * @param destination_table_id Unused ID for the table to create.
 * @param input Exact-length UTF-8 input; it is not NUL-terminated by contract.
 * @param input_length Input byte count.
 * @param rules Delimiter, quoting, and column-name rules.
 * @return CLM_FORMAT_DESERIALIZE_SUCCESS, or CLM_FORMAT_DESERIALIZE_FAILURE.
 */
clm_int clm_string_format_parse_delimited(
	const clm_id destination_table_id,
	const char* input,
	const size_t input_length,
	const clm_string_format_delimited_parse_rules* rules);

/** Serializes a string table into an existing CLM string block.
 * Fields are quoted when required by the delimiter, quote, or line breaks;
 * embedded quote characters are doubled. QUOTE_ALL_FIELDS forces quoting.
 * Missing cells serialize as empty fields. Line endings are LF.
 * @param table_id Table to serialize.
 * @param rules Delimiter, quoting, and optional-header rules.
 * @param destination_string_id Existing BPA string block to replace.
 * @return CLM_FORMAT_SERIALIZE_SUCCESS, or CLM_FORMAT_SERIALIZE_FAILURE.
 */
clm_int clm_string_format_serialize_delimited(
	const clm_id table_id,
	const clm_string_format_delimited_serialize_rules* rules,
	const clm_id destination_string_id);

/** Initializes a lexer over an exact-length UTF-8 input buffer. 
 * @param lexer Writable output or in/out pointer.
 * @param input Input data or buffer; not modified.
 * @param input_length Input parameter described above.
 * @param rules Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_lexer_init(
	clm_string_format_lexer* lexer,
	const char* input,
	const size_t input_length,
	const clm_string_format_lex_rules* rules);

/** Produces the next token, including CLM_STRING_FORMAT_TOKEN_END at EOF. 
 * @param lexer Writable output or in/out pointer.
 * @param token Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_lexer_next(
	clm_string_format_lexer* lexer,
	clm_string_format_token* token);

/** Initializes a parser cursor using the supplied lexical rules. 
 * @param parser Writable output or in/out pointer.
 * @param input Input data or buffer; not modified.
 * @param input_length Input parameter described above.
 * @param rules Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_parser_init(
	clm_string_format_parser* parser,
	const char* input,
	const size_t input_length,
	const clm_string_format_lex_rules* rules);

/** Returns the next token without consuming it. 
 * @param parser Writable output or in/out pointer.
 * @param token Writable output or in/out pointer.
 * @return The next token without consuming it.
 */
clm_int clm_string_format_parser_peek(
	clm_string_format_parser* parser,
	clm_string_format_token* token);

/** Consumes and returns the next token. 
 * @param parser Writable output or in/out pointer.
 * @param token Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_parser_take(
	clm_string_format_parser* parser,
	clm_string_format_token* token);

/** Consumes a token only when its category and optional literal match. 
 * @param parser Writable output or in/out pointer.
 * @param expected_kind Input parameter described above.
 * @param expected_text Input data or buffer; not modified.
 * @param token Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_parser_expect(
	clm_string_format_parser* parser,
	const clm_string_format_token_kind expected_kind,
	const char* expected_text,
	clm_string_format_token* token);

/** Compares a token's source bytes with an exact literal. 
 * @param parser Input data or buffer; not modified.
 * @param token Input data or buffer; not modified.
 * @param literal Input data or buffer; not modified.
 * @return Whether the condition described above holds.
 */
bool clm_string_format_token_equals(
	const clm_string_format_parser* parser,
	const clm_string_format_token* token,
	const char* literal);

/** Copies token source bytes into an owned NUL-terminated buffer. 
 * @param parser Input data or buffer; not modified.
 * @param token Input data or buffer; not modified.
 * @param output Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_token_copy(
	const clm_string_format_parser* parser,
	const clm_string_format_token* token,
	char** output);

int clm_string_format_internal_hex(const char value);

/** Decodes a JSON-style double-quoted token into allocated UTF-8 text. 
 * @param parser Input data or buffer; not modified.
 * @param token Input data or buffer; not modified.
 * @param output Writable output or in/out pointer.
 * @param output_length Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_decode_quoted(
	const clm_string_format_parser* parser,
	const clm_string_format_token* token,
	char** output,
	size_t* output_length);

/** Finds a collision-free BPA/BPAA ID for a format-created tree child. */
clm_int clm_string_format_internal_new_node_id(clm_id* out_id);

/** Trims ASCII horizontal whitespace from a byte span in place. */
void clm_string_format_internal_trim_span(
	const char* text,
	size_t* offset,
	size_t* length);

/** Resolves an exact UTF-8 field-name span through the clm_id registry. */
clm_int clm_string_format_internal_resolve_key(
	const char* name,
	const size_t length,
	clm_id* out_id);

/** Resolve a mapped key or optionally register one new unique ID.
 * Registration is parse-scoped and automatically rolled back on failure.
 * Existing names retain their previous ID and are never rolled back. */
clm_int clm_string_format_internal_resolve_key_with_parser(
    clm_string_format_parser* parser,
    const char* name,
    const size_t length,
    clm_id* out_id);

void clm_string_format_internal_release_keys(
    clm_string_format_key_registry* registry,
    const bool rollback);

/** Adds a newly allocated child object and returns its ID. */
clm_int clm_string_format_internal_add_object(
	const clm_id parent_id,
	const clm_id key,
	const bool parent_is_array,
	clm_id* child_id);

/** Adds a newly allocated child array and returns its ID. */
clm_int clm_string_format_internal_add_array(
	const clm_id parent_id,
	const clm_id key,
	const bool parent_is_array,
	clm_id* child_id);

/** Inserts a typed scalar into an object or ordered array parent. */
clm_int clm_string_format_internal_set_leaf(
	const clm_id parent_id,
	const bool parent_is_array,
	const clm_id key,
	const clm_byte type,
	const void* value,
	const size_t size);

clm_int clm_string_format_internal_set_boolean(
	const clm_id parent_id,
	const bool parent_is_array,
	const clm_id key,
	const bool value);

clm_int clm_string_format_internal_set_null(
	const clm_id parent_id,
	const bool parent_is_array,
	const clm_id key);

/** Parses a numeric literal and stores the narrowest supported numeric leaf. */
clm_int clm_string_format_internal_parse_number(
	const char* text,
	const size_t length,
	const clm_id parent_id,
	const bool parent_is_array,
	const clm_id key);

/** Resolves and JSON-quotes a mapped object key for a serializer. 
 * @param writer Writable output or in/out pointer.
 * @param key Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_writer_append_key(
	clm_string_format_writer* writer,
	const clm_id key);

/** Appends the registered spelling of a mapped key without quoting it. 
 * @param writer Writable output or in/out pointer.
 * @param key Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_writer_append_key_raw(
	clm_string_format_writer* writer,
	const clm_id key);

/** Appends the textual spelling of a supported tree leaf without quotes. 
 * @param writer Writable output or in/out pointer.
 * @param value_id CLM ID identifying the value.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_writer_append_leaf_text(
	clm_string_format_writer* writer,
	const clm_id value_id);

/** Initializes shared parser state and a new object/array root, then invokes a codec.
 * If parsing fails, the newly created tree root and descendants are deleted.
 * @param destination_root_id Unused ID for the root container.
 * @param root_kind Object or array root requested by the codec.
 * @param parse_format Format-specific grammar callback.
 * @return CLM_FORMAT_DESERIALIZE_SUCCESS or CLM_FORMAT_DESERIALIZE_FAILURE.
 * @param input Input data or buffer; not modified.
 * @param input_length Input parameter described above.
 * @param rules Input data or buffer; not modified.
 * @param auto_register_keys Input parameter described above.
 */
clm_int clm_string_format_parse_node_with_options(
	const char* input,
	const size_t input_length,
	const clm_string_format_lex_rules* rules,
	const clm_id destination_root_id,
	const clm_string_format_root_kind root_kind,
	const clm_string_format_tree_parser_fn parse_format,
    const bool auto_register_keys);

/** Original strict pre-registered-name behavior; unchanged by default. 
 * @param input Input data or buffer; not modified.
 * @param input_length Input parameter described above.
 * @param rules Input data or buffer; not modified.
 * @param destination_root_id CLM ID identifying the destination root.
 * @param root_kind Input parameter described above.
 * @param parse_format Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_parse_node(
    const char* input,
    const size_t input_length,
    const clm_string_format_lex_rules* rules,
    const clm_id destination_root_id,
    const clm_string_format_root_kind root_kind,
    const clm_string_format_tree_parser_fn parse_format);

/** Convenience wrapper that parses with an object root. 
 * @param input Input data or buffer; not modified.
 * @param input_length Input parameter described above.
 * @param rules Input data or buffer; not modified.
 * @param destination_root_id CLM ID identifying the destination root.
 * @param parse_format Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_parse_tree(
	const char* input,
	const size_t input_length,
	const clm_string_format_lex_rules* rules,
	const clm_id destination_root_id,
	const clm_string_format_tree_parser_fn parse_format);

/** Parses the contents of an existing CLM UTF-8 string into a new tree root. 
 * @param source_string_id CLM ID identifying the source string.
 * @param destination_root_id CLM ID identifying the destination root.
 * @param rules Input data or buffer; not modified.
 * @param root_kind Input parameter described above.
 * @param parse_format Input parameter described above.
 * @param auto_register_keys Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_loads_tree_with_options(
	const clm_id source_string_id,
	const clm_id destination_root_id,
	const clm_string_format_lex_rules* rules,
	const clm_string_format_root_kind root_kind,
	const clm_string_format_tree_parser_fn parse_format,
    const bool auto_register_keys);

clm_int clm_string_format_loads_tree(
    const clm_id source_string_id,
    const clm_id destination_root_id,
    const clm_string_format_lex_rules* rules,
    const clm_string_format_root_kind root_kind,
    const clm_string_format_tree_parser_fn parse_format);

/** Validates a tree root and string destination, then invokes a format emitter.
 * @return CLM_FORMAT_SERIALIZE_SUCCESS or CLM_FORMAT_SERIALIZE_FAILURE.
 * @param root_object_id CLM ID identifying the root object.
 * @param destination_string_id CLM ID identifying the destination string.
 * @param serialize_format Input parameter described above.
 */
clm_int clm_string_format_serialize_tree(
	const clm_id root_object_id,
	const clm_id destination_string_id,
	const clm_string_format_tree_serializer_fn serialize_format);

clm_int clm_string_format_internal_load_file_with_options(
	const clm_id path_id,
	const clm_id destination_root_id,
	const clm_string_format_lex_rules* rules,
	const clm_string_format_root_kind root_kind,
	const clm_string_format_tree_parser_fn parse_format,
    const bool auto_register_keys);

clm_int clm_string_format_internal_load_file(
    const clm_id path_id,
    const clm_id destination_root_id,
    const clm_string_format_lex_rules* rules,
    const clm_string_format_root_kind root_kind,
    const clm_string_format_tree_parser_fn parse_format);

clm_int clm_string_format_internal_dump_file(
	const clm_id path_id,
	const clm_id root_id,
	const clm_string_format_tree_serializer_fn serialize_format);

/** Serializes into a caller buffer; insufficient capacity leaves it unchanged.
 * Capacity includes the terminating NUL. The returned status does not include
 * a byte count; callers can use a temporary CLM string when they need sizing.
 * @param root_object_id CLM ID identifying the root object.
 * @param destination Writable output or in/out pointer.
 * @param capacity Available capacity.
 * @param serialize_format Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_format_serialize_tree_to(
	const clm_id root_object_id,
	char* destination,
	const size_t capacity,
	const clm_string_format_tree_serializer_fn serialize_format);

