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

#include "bpa.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Unicode text is ALWAYS stored internally as validated UTF-8.
 * Encoding values describe external byte representations only.
 * UTF-16LE exports are not NUL-terminated. */
#define CLM_STRING_ENCODING_UTF8 1u
#define CLM_STRING_ENCODING_LATIN1 2u
#define CLM_STRING_ENCODING_UTF16LE 3u

/* Shared runtime default, defined once via CLM_STRING_IMPLEMENTATION.
 * It affects new strings' preferred output encoding; existing strings retain theirs.
 * Not thread-safe: configure before concurrent use. */
extern clm_byte clm_string_global_default_encoding;

bool clm_string_encoding_supported(const clm_byte encoding);

/** Changes the preferred byte-export encoding for subsequently created strings.
 * Does not change Unicode text or existing strings' preferences.
 * @param encoding Encoding used for input or output bytes.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_set_default_encoding(const clm_byte encoding);

/** Returns the runtime default byte-export encoding (initially UTF-8). 
 * @return The runtime default byte-export encoding (initially UTF-8).
 */
clm_byte clm_string_get_default_encoding(void);

bool clm_string_utf8_next(
    const unsigned char* text,
    const size_t length,
    size_t* offset);

clm_int clm_string_measure(
    const char* text,
    const size_t byte_length,
    const clm_byte encoding,
    size_t* character_count);

clm_int clm_string_offset(
    const char* text,
    const size_t byte_length,
    const clm_byte encoding,
    const size_t character_index,
    size_t* byte_offset);

/** Decodes one validated UTF-8 scalar and advances offset past its bytes. 
 * @param data Input data or buffer; not modified.
 * @param size Size in bytes or elements, as specified above.
 * @param offset Writable output or in/out pointer.
 * @param scalar Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_utf8_decode_one(
    const unsigned char* data,
    size_t size,
    size_t* offset,
    uint32_t* scalar);

bool clm_string_is_whitespace(const uint32_t codepoint);

uint32_t clm_string_casefold_codepoint(const uint32_t codepoint);

clm_int clm_string_compare_text(
    const char* left,
    const size_t left_bytes,
    const char* right,
    const size_t right_bytes,
    const bool ignore_case);

clm_int clm_string_trim_bounds(
    const char* text,
    const size_t byte_length,
    const bool trim_left,
    const bool trim_right,
    size_t* begin,
    size_t* end,
    size_t* removed);

clm_int clm_string_get_block(
    const clm_id id,
    clm_bpa_block_node** block,
    const char** text,
    size_t* byte_length,
    size_t* character_count);

clm_int clm_string_replace_bytes(
    const clm_id id,
    const char* text,
    const size_t byte_length);

char* clm_string_temp_alloc(const size_t bytes);

void clm_string_temp_free(char* text, const size_t bytes);

/** Creates a string BPA block and initializes it with a UTF-8 string.
 * @return true on success; false for invalid text, duplicate ID, or allocation failure.
 * @param id CLM ID identifying the id.
 * @param text Input data or buffer; not modified.
 */
bool clm_string_new(const clm_id id, const char* text);

/** Deletes a string block without affecting blocks of other types.
 * @return true when deleted; false if missing or not a string block.
 * @param id CLM ID identifying the id.
 */
bool clm_string_delete(const clm_id id);

/** Replaces an existing string's value with valid UTF-8 text.
 * @param dest_id ID of the string block to update; it must already exist.
 * @param text NUL-terminated UTF-8 text to copy.
 * @return Unicode code-point count on success, or CLM_BPA_FAILURE.
 */
clm_int clm_string_set(const clm_id dest_id, const char* text);

/** Initializes a string from NUL-terminated UTF-8 text, creating the
 * destination with the current default encoding if it does not yet exist.
 * An existing destination must be a string block and retains its encoding.
 * @param dest_id ID to create or update.
 * @param source Valid NUL-terminated UTF-8 text.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE on invalid input or allocation failure.
 */
clm_int clm_string_from_from(const clm_id dest_id, const char* source);

/** Replaces a string block's value; returns its character count or failure. 
 * @param dest_id CLM ID identifying the dest.
 * @param text Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_set_from(const clm_id dest_id, const char* text);

/** Appends text and returns the number of characters appended, or CLM_BPA_FAILURE.
 * Character counts and indices are Unicode code points.
 * @param id CLM ID identifying the id.
 * @param suffix Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_append(const clm_id id, const char* suffix);

/** Appends text followed by a newline; returns appended character count or failure. 
 * @param id CLM ID identifying the id.
 * @param suffix Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_append_line(const clm_id id, const char* suffix);

/** Inserts text at a character index; returns inserted character count or failure. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param inserted Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_insert(
    const clm_id id,
    const clm_int index,
    const char* inserted);

/** Removes a character range; returns the number removed, or CLM_BPA_FAILURE. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param length Length in bytes or elements, as specified above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_remove(
    const clm_id id,
    const clm_int index,
    const clm_int length);

/** Returns whether a string block contains a valid search string. 
 * @param id CLM ID identifying the id.
 * @param search Input data or buffer; not modified.
 * @return Whether a string block contains a valid search string.
 */
bool clm_string_contains(const clm_id id, const char* search);

/** Returns string length in Unicode code points, or failure. 
 * @param id CLM ID identifying the id.
 * @return String length in Unicode code points, or failure.
 */
clm_int clm_string_length(const clm_id id);

clm_int clm_string_compare_case(
    const clm_id left_id,
    const clm_id right_id,
    const bool ignore_case);

/** Compares two string blocks lexicographically by Unicode code point.
 * The default comparison is case-sensitive.
 * @param left_id CLM ID identifying the left.
 * @param right_id CLM ID identifying the right.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_compare(
    const clm_id left_id,
    const clm_id right_id);

/** Compares two string blocks ignoring case using simple mappings for common
 * Latin, Greek, and Cyrillic letters; other code points compare exactly.
 * @param left_id CLM ID identifying the left.
 * @param right_id CLM ID identifying the right.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_compare_ignore_case(
    const clm_id left_id,
    const clm_id right_id);

/** Compares two valid NUL-terminated UTF-8 strings; case-sensitive by default. 
 * @param left Input data or buffer; not modified.
 * @param right Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_compare_from(
    const char* left,
    const char* right);

/** Compares two valid NUL-terminated UTF-8 strings ignoring case using the
 * same simple mappings as clm_string_compare_ignore_case.
 * @param left Input data or buffer; not modified.
 * @param right Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_compare_ignore_case_from(
    const char* left,
    const char* right);

clm_int clm_string_array_append_bytes(
    const clm_id array_id,
    const char* text,
    const size_t byte_length);

clm_int clm_string_split_text(
    const clm_id destination_array_id,
    const char* text,
    const size_t text_bytes,
    const char* delimiter,
    const size_t delimiter_bytes);

/** Splits a string block by a nonempty delimiter string into a new string array.
 * Empty fields, including leading and trailing fields, are retained.
 * @param destination_array_id CLM ID identifying the destination array.
 * @param source_string_id CLM ID identifying the source string.
 * @param delimiter_string_id CLM ID identifying the delimiter string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_split(
    const clm_id destination_array_id,
    const clm_id source_string_id,
    const clm_id delimiter_string_id);

/** Splits UTF-8 text by a nonempty UTF-8 delimiter; see clm_string_split. 
 * @param destination_array_id CLM ID identifying the destination array.
 * @param text Input data or buffer; not modified.
 * @param delimiter Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_split_from(
    const clm_id destination_array_id,
    const char* text,
    const char* delimiter);

clm_int clm_string_split_lines_text(
    const clm_id destination_array_id,
    const char* text,
    const size_t text_bytes);

/** Splits a string block at CR, LF, CRLF, NEL, line separator, and paragraph separator. 
 * @param destination_array_id CLM ID identifying the destination array.
 * @param source_string_id CLM ID identifying the source string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_split_lines(
    const clm_id destination_array_id,
    const clm_id source_string_id);

/** Splits UTF-8 text into lines; trailing empty lines are retained. 
 * @param destination_array_id CLM ID identifying the destination array.
 * @param text Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_split_lines_from(
    const clm_id destination_array_id,
    const char* text);

clm_int clm_string_trim_id(
    const clm_id id,
    const bool trim_left,
    const bool trim_right);

clm_int clm_string_trim_text_to_id(
    const clm_id destination_string_id,
    const char* text,
    const bool trim_left,
    const bool trim_right);

/** Removes leading Unicode White_Space characters from a string block. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_left_trim(const clm_id id);

/** Removes trailing Unicode White_Space characters from a string block. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_right_trim(const clm_id id);

/** Removes leading and trailing Unicode White_Space characters from a string block. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_trim(const clm_id id);

/** Copies raw UTF-8 text to a string block after removing leading whitespace. 
 * @param destination_string_id CLM ID identifying the destination string.
 * @param text Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_left_trim_from(
    const clm_id destination_string_id,
    const char* text);

/** Copies raw UTF-8 text to a string block after removing trailing whitespace. 
 * @param destination_string_id CLM ID identifying the destination string.
 * @param text Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_right_trim_from(
    const clm_id destination_string_id,
    const char* text);

/** Copies raw UTF-8 text to a string block after removing surrounding whitespace. 
 * @param destination_string_id CLM ID identifying the destination string.
 * @param text Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_trim_from(
    const clm_id destination_string_id,
    const char* text);

/** Replaces every non-overlapping occurrence; returns replacements made or failure.
 * An empty search string is invalid. The replacement may be empty.
 * @param id CLM ID identifying the id.
 * @param search Input data or buffer; not modified.
 * @param replacement Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_replace_from(
    const clm_id id,
    const char* search,
    const char* replacement);

/** Replaces every non-overlapping occurrence in a string.
 * Returns the replacement count, zero when there are no matches, or
 * CLM_BPA_FAILURE for invalid UTF-8, an empty search, or an invalid string ID.
 * @param id CLM ID identifying the id.
 * @param search Input data or buffer; not modified.
 * @param replacement Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_replace(
    const clm_id id,
    const char* search,
    const char* replacement);

/** Encodes one Unicode scalar as UTF-8. Caller provides 4 writable bytes. 
 * @param cp Input parameter described above.
 * @param out Writable output buffer.
 * @return Computed size, count, or value described in the summary above.
 */
size_t clm_string_utf8_encode_one(uint32_t cp, unsigned char* out);

/** Convert bytes to validated UTF-8 in an allocated temporary buffer.
 * Rejects embedded U+0000 because BPA currently stores NUL-terminated strings.
 * @param encoding Encoding used for input or output bytes.
 * @param input Input data or buffer; not modified.
 * @param size Size in bytes or elements, as specified above.
 * @param output Writable output or in/out pointer.
 * @param output_length Writable output or in/out pointer.
 * @return Operation status or value described in the summary above.
 * Call free() on the returned buffer. */
clm_int clm_string_decode_bytes(const clm_byte encoding,
    const void* input, size_t size, char** output, size_t* output_length);

/** Exports the Unicode text into bytes in its preferred external encoding.
 * No trailing NUL or BOM is included. Returns byte count, or -1 on failure.
 * NULL buffer with zero capacity queries required byte count (without writing).
 * Fails without writing when output capacity is insufficient.
 * @param id CLM ID identifying the id.
 * @param buffer Writable output buffer.
 * @param buffer_size Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_get_bytes(const clm_id id, void* buffer, const clm_uint buffer_size);

/** Decode external byte sequence into an existing string, or create a new string.
 * Buffer length is exact (not NUL-terminated). Internal storage remains UTF-8.
 * If replacing, preserve resource identity; update preferred export encoding.
 * Fails on invalid UTF-8, UTF-16, embedded U+0000, or allocation failure.
 * @param id CLM ID identifying the id.
 * @param encoding Encoding used for input or output bytes.
 * @param buffer Writable output buffer.
 * @param buffer_size Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_from_bytes(const clm_id id, const clm_byte encoding, const void* buffer, const clm_uint buffer_size);

/** Changes the preferred encoding for subsequent byte exports.
 * Text remains unchanged and stored as UTF-8. Does NOT transcode in place.
 * Fails if the current text cannot be represented in the new encoding.
 * @param id CLM ID identifying the id.
 * @param new_encoding Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_change_encoding(const clm_id id, const clm_byte new_encoding);


/** Creates an independent CLM string containing a Unicode code-point slice.
 * The range is [start, start + length), with strict bounds. A zero-length
 * range at the end is valid. Output is UTF-8 internally, and inherits the
 * source's preferred byte-export encoding. The destination ID must be unused.
 * @return CLM_BPA_SUCCESS on success; CLM_BPA_FAILURE otherwise.
 * @param source_id CLM ID identifying the source.
 * @param start Input parameter described above.
 * @param length Length in bytes or elements, as specified above.
 * @param destination_id CLM ID identifying the destination.
 */
clm_int clm_string_slice(const clm_id source_id, const clm_int start,
                                   const clm_int length, const clm_id destination_id);

/** Borrows a UTF-8 byte view of a Unicode code-point range.
 * The view is not NUL-terminated at its end and remains valid only while the
 * source string is live and unmodified. It does not retain the source block.
 * @param source_id ID of the source string.
 * @param start Zero-based first Unicode code point to include.
 * @param length Number of Unicode code points to borrow.
 * @param text Receives the first byte of the range; may point to the final NUL
 *        for an empty range.
 * @param byte_length Receives the view length in bytes, excluding any terminator.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/range.
 */
clm_int clm_string_slice_borrow(
    const clm_id source_id,
    const clm_int start,
    const clm_int length,
    const char** text,
    size_t* byte_length);

/** Copies a string block into a destination string, creating it if needed.
 * An existing destination must be a string block. The source must contain
 * valid UTF-8 and have a supported, representable export encoding; that
 * encoding preference is copied to the destination.
 * @param dest_id ID to create or update.
 * @param source_id ID of the source string block.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE on invalid input or allocation failure.
 */
clm_int clm_string_from(const clm_id dest_id, const clm_id source_id);

/** Decodes an externally encoded byte sequence and initializes a string from
 * the resulting UTF-8 text. New destinations use the global default encoding;
 * existing string destinations retain their encoding preference.
 * @param dest_id ID to create or update.
 * @param encoding Encoding of the input bytes.
 * @param source Exact input byte sequence; may be NULL only when source_size is zero.
 * @param source_size Number of input bytes, excluding any terminator.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input or allocation failure.
 */
clm_int clm_string_from_enc(
    const clm_id dest_id,
    const clm_byte encoding,
    const void* source,
    const clm_uint source_size);

/** Alias of clm_string_from_enc for callers using the from_from naming form. 
 * @param dest_id CLM ID identifying the dest.
 * @param encoding Encoding used for input or output bytes.
 * @param source Input data or buffer; not modified.
 * @param source_size Input parameter described above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_from_from_enc(
    const clm_id dest_id,
    const clm_byte encoding,
    const void* source,
    const clm_uint source_size);

clm_int clm_string_array_get_node(
    const clm_id id,
    clm_bpaa_array_node** array);

clm_int clm_string_array_validate_block(
    const clm_bpaa_array_node* array,
    const clm_id block_id);

clm_int clm_string_array_replace_bytes(
    clm_bpaa_array_node* array,
    const clm_int index,
    const char* text,
    const size_t byte_length);

clm_int clm_string_array_compare_text(
    const clm_id array_id,
    const clm_int index,
    const char* text,
    const size_t text_bytes,
    const bool ignore_case);

/** Compares an indexed array item with a string block; case-sensitive by default. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param string_id CLM ID identifying the string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_compare(
    const clm_id array_id,
    const clm_int index,
    const clm_id string_id);

/** Compares an indexed array item with a string block, ignoring case. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param string_id CLM ID identifying the string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_compare_ignore_case(
    const clm_id array_id,
    const clm_int index,
    const clm_id string_id);

/** Compares an indexed array item with raw UTF-8 text; case-sensitive by default. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param text Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_compare_from(
    const clm_id array_id,
    const clm_int index,
    const char* text);

/** Compares an indexed array item with raw UTF-8 text, ignoring case. 
 * @param array_id CLM ID identifying the array.
 * @param index Zero-based element or worker index.
 * @param text Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_compare_ignore_case_from(
    const clm_id array_id,
    const clm_int index,
    const char* text);

clm_int clm_string_array_trim_all(
    const clm_id id,
    const bool trim_left,
    const bool trim_right);

/** Removes leading Unicode whitespace from every array item; returns chars removed. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_left_trim(const clm_id id);

/** Removes trailing Unicode whitespace from every array item; returns chars removed. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_right_trim(const clm_id id);

/** Removes surrounding Unicode whitespace from every array item; returns chars removed. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_trim(const clm_id id);

clm_int clm_string_array_join_text(
    const clm_id destination_string_id,
    const clm_id array_id,
    const char* separator,
    const size_t separator_bytes);

/** Joins an array into a string block using a string block as the separator.
 * Returns the joined Unicode character count, or CLM_BPA_FAILURE.
 * @param destination_string_id CLM ID identifying the destination string.
 * @param array_id CLM ID identifying the array.
 * @param separator_string_id CLM ID identifying the separator string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_join(
    const clm_id destination_string_id,
    const clm_id array_id,
    const clm_id separator_string_id);

/** Joins an array into a string block using raw UTF-8 text as separator. 
 * @param destination_string_id CLM ID identifying the destination string.
 * @param array_id CLM ID identifying the array.
 * @param separator Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_join_from(
    const clm_id destination_string_id,
    const clm_id array_id,
    const char* separator);

/** Creates a string array; strings use the canonical UTF-8 internal encoding. 
 * @param id CLM ID identifying the id.
 * @param reserved Input parameter described above.
 * @return Whether the condition described above holds.
 */
bool clm_string_array_new(const clm_id id, const clm_int reserved);

/** Returns the indexed string's character count, or CLM_BPA_FAILURE. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @return The indexed string's character count, or CLM_BPA_FAILURE.
 */
clm_int clm_string_array_length_at(
    const clm_id id,
    const clm_int index);

/** Returns whether the indexed string contains a valid search string. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param search Input data or buffer; not modified.
 * @return Whether the indexed string contains a valid search string.
 */
bool clm_string_array_contains_at(
    const clm_id id,
    const clm_int index,
    const char* search);

/** Appends text to one array string; returns appended character count. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param suffix Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_append_at(
    const clm_id id,
    const clm_int index,
    const char* suffix);

/** Appends text and a newline to one array string. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param suffix Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_append_line_at(
    const clm_id id,
    const clm_int index,
    const char* suffix);

/** Inserts text at a character index in one array string. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param character_index Input parameter described above.
 * @param inserted Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_insert_at(
    const clm_id id,
    const clm_int index,
    const clm_int character_index,
    const char* inserted);

/** Removes a character range from one array string. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param character_index Input parameter described above.
 * @param length Length in bytes or elements, as specified above.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_remove_at(
    const clm_id id,
    const clm_int index,
    const clm_int character_index,
    const clm_int length);

/** Replaces every non-overlapping match in one array string; returns match count. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param search Input data or buffer; not modified.
 * @param replacement Input data or buffer; not modified.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_replace_at(
    const clm_id id,
    const clm_int index,
    const char* search,
    const char* replacement);

/** Deletes a string array without affecting arrays of other item types. 
 * @param id CLM ID identifying the id.
 * @return Whether the condition described above holds.
 */
bool clm_string_array_delete(const clm_id id);

/** Returns array item count, or CLM_BPA_FAILURE if missing/too large. 
 * @param id CLM ID identifying the id.
 * @return Array item count, or CLM_BPA_FAILURE if missing/too large.
 */
clm_int clm_string_array_count(const clm_id id);

/** Copies an array item into an existing string BPA block. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param destination_string_id CLM ID identifying the destination string.
 * @return Operation status or value described in the summary above.
 */
int clm_string_array_get(
    const clm_id id,
    const clm_int index,
    const clm_id destination_string_id);

/** Replaces an array item with an existing string BPA block's value. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param source_string_id CLM ID identifying the source string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_set(
    const clm_id id,
    const clm_int index,
    const clm_id source_string_id);

/** Inserts a string block at an array index. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @param source_string_id CLM ID identifying the source string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_insert(
    const clm_id id,
    const clm_int index,
    const clm_id source_string_id);

/** Appends a string block to an array. 
 * @param id CLM ID identifying the id.
 * @param source_string_id CLM ID identifying the source string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_append(const clm_id id, const clm_id source_string_id);

/** Removes an array item by zero-based index. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_remove(const clm_id id, const clm_int index);

/** Clears array items while preserving its reserved capacity. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_clear(const clm_id id);

/** Finds the first exact string match; returns its index or CLM_BPA_FAILURE. 
 * @param id CLM ID identifying the id.
 * @param source_string_id CLM ID identifying the source string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_find(
    const clm_id id,
    const clm_id source_string_id);

int clm_string_array_sort_compare(const void* left, const void* right);

/** Sorts strings lexicographically by their encoded bytes. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_sort(const clm_id id);

/** Randomly shuffles string items and resets the draw deck. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_shuffle(const clm_id id);

/** Moves the array cursor to the first item. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_move_first(const clm_id id);

/** Moves the array cursor to the previous item. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_move_previous(const clm_id id);

/** Moves the array cursor to the next item. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_move_next(const clm_id id);

/** Moves the array cursor to the last item. 
 * @param id CLM ID identifying the id.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_move_last(const clm_id id);

/** Moves the array cursor to an item. 
 * @param id CLM ID identifying the id.
 * @param index Zero-based element or worker index.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_move_to(const clm_id id, const clm_int index);

/** Copies the current array item into a string BPA block. 
 * @param id CLM ID identifying the id.
 * @param destination_string_id CLM ID identifying the destination string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_get_current(
    const clm_id id,
    const clm_id destination_string_id);

/** Draws an indexed item from the active deck into a string BPA block. 
 * @param id CLM ID identifying the id.
 * @param deck_index Input parameter described above.
 * @param destination_string_id CLM ID identifying the destination string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_draw_at(
    const clm_id id,
    const clm_int deck_index,
    const clm_id destination_string_id);

/** Draws the next item, shuffling again when the active deck is exhausted. 
 * @param id CLM ID identifying the id.
 * @param destination_string_id CLM ID identifying the destination string.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_array_draw(
    const clm_id id,
    const clm_id destination_string_id);

/* Define once in exactly one translation unit, next to CLM_BPA_IMPLEMENTATION. */
