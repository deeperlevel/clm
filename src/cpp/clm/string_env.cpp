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
#include "string_format.hpp"
#include "file.hpp"

/** Parses line-oriented NAME=VALUE assignments into a root tree object. */
static inline clm_int clm_string_env_internal_parse(
    clm_string_format_parser* parser,
    const clm_id root_id)
{
    const char* text = parser->lexer.input;
    const size_t length = parser->lexer.input_length;
    size_t line_start = 0;
    while (line_start < length) {
        size_t line_end = line_start;
        while (line_end < length && text[line_end] != '\n' && text[line_end] != '\r')
            ++line_end;
        size_t offset = line_start;
        size_t line_length = line_end - line_start;
        clm_string_format_internal_trim_span(text, &offset, &line_length);
        if (line_length != 0 && text[offset] != '#') {
            if (line_length >= 7 && memcmp(text + offset, "export ", 7) == 0) {
            offset += 7;
            line_length -= 7;
            clm_string_format_internal_trim_span(text, &offset, &line_length);
            }
            size_t separator = 0;
            while (separator < line_length && text[offset + separator] != '=')
                ++separator;
            if (separator == 0 || separator == line_length)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            size_t key_offset = offset;
            size_t key_length = separator;
            clm_string_format_internal_trim_span(text, &key_offset, &key_length);
            if (key_length == 0 ||
                !((text[key_offset] >= 'A' && text[key_offset] <= 'Z') ||
                  (text[key_offset] >= 'a' && text[key_offset] <= 'z') ||
              text[key_offset] == '_'))
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            for (size_t index = 1; index < key_length; ++index) {
                const char character = text[key_offset + index];
                if (!((character >= 'A' && character <= 'Z') ||
                      (character >= 'a' && character <= 'z') ||
                      (character >= '0' && character <= '9') || character == '_'))
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
            }
            clm_id key;
            size_t value_offset = offset + separator + 1;
            size_t value_length = line_length - separator - 1;
            clm_string_format_internal_trim_span(text, &value_offset, &value_length);
            char* value = NULL;
            size_t decoded_length = 0;
            if (clm_string_format_internal_resolve_key_with_parser(parser, text + key_offset,
                    key_length, &key) != CLM_BPA_SUCCESS ||
                clm_tree_contains(root_id, key))
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            if (value_length != 0 && text[value_offset] == '"') {
                clm_string_format_parser quoted_parser = {0};
                clm_string_format_token token = {
                    CLM_STRING_FORMAT_TOKEN_QUOTED, value_offset, value_length, 1,
                    value_offset + 1
                };
                quoted_parser.lexer.input = text;
                quoted_parser.lexer.input_length = length;
                if (clm_string_format_decode_quoted(&quoted_parser, &token, &value,
                        &decoded_length) != CLM_BPA_SUCCESS)
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
            } else if (value_length != 0 && text[value_offset] == '\'') {
                if (value_length < 2 || text[value_offset + value_length - 1] != '\'')
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
                decoded_length = value_length - 2;
                value = (char*)malloc(decoded_length + 1);
                if (value == NULL) return CLM_FORMAT_DESERIALIZE_FAILURE;
                memcpy(value, text + value_offset + 1, decoded_length);
                value[decoded_length] = '\0';
            } else {
                decoded_length = value_length;
                value = (char*)malloc(decoded_length + 1);
                if (value == NULL) return CLM_FORMAT_DESERIALIZE_FAILURE;
                memcpy(value, text + value_offset, decoded_length);
                value[decoded_length] = '\0';
            }
            const int set_status = clm_tree_set_from(root_id, key,
                CLM_BPA_TYPE_STRING, value, decoded_length + 1);
            free(value);
            if (set_status != CLM_BPA_SET_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
        }
        line_start = line_end;
        if (line_start < length && text[line_start] == '\r') ++line_start;
        if (line_start < length && text[line_start] == '\n') ++line_start;
    }
    return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

/** Serializes one ENV assignment per string leaf; all values are quoted. */
static inline clm_int clm_string_env_internal_serialize(
    const clm_id root_id,
    const clm_id destination_id)
{
    clm_string_format_writer writer;
    const clm_int count = clm_tree_count(root_id);
    if (count < 0 || (size_t)count > (size_t)-1 / sizeof(clm_id))
        return CLM_FORMAT_SERIALIZE_FAILURE;
    clm_id* keys = count == 0 ? NULL : (clm_id*)malloc((size_t)count * sizeof(*keys));
    if ((count != 0 && keys == NULL) ||
        clm_dictionary_keys_to(root_id, keys, (size_t)count) != CLM_BPA_GET_SUCCESS) {
        free(keys);
        return CLM_FORMAT_SERIALIZE_FAILURE;
    }
    clm_string_format_writer_init(&writer);
    for (clm_int index = 0; index < count; ++index) {
        clm_byte type;
        clm_id value_id;
        clm_string_format_writer value_writer;
        clm_string_format_writer_init(&value_writer);
        if (clm_tree_get_type(root_id, keys[index], &type) != CLM_BPA_GET_SUCCESS ||
            type != CLM_BPA_TYPE_STRING ||
            clm_tree_get_id(root_id, keys[index], &value_id) != CLM_BPA_GET_SUCCESS ||
            clm_string_format_writer_append_key_raw(&writer, keys[index]) != CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_char(&writer, '=') != CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_leaf_text(&value_writer, value_id) != CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_quoted(&writer, value_writer.data,
                value_writer.length) != CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_char(&writer, '\n') != CLM_FORMAT_SERIALIZE_SUCCESS) {
            clm_string_format_writer_dispose(&value_writer);
            free(keys);
            clm_string_format_writer_dispose(&writer);
            return CLM_FORMAT_SERIALIZE_FAILURE;
        }
        clm_string_format_writer_dispose(&value_writer);
    }
    free(keys);
    const clm_int result = clm_string_format_writer_commit(&writer, destination_id);
    clm_string_format_writer_dispose(&writer);
    return result;
}

static const clm_string_format_lex_rules clm_string_env_lex_rules = {
    "=", "#", NULL, NULL, "\"'", '\\', CLM_STRING_FORMAT_LEX_EMIT_NEWLINES
};

/** Parses an existing CLM string block into a new owned ENV tree. */
static inline clm_int clm_string_env_loads(
    const clm_id source_id, const clm_id destination_tree_id)
{
    return clm_string_format_loads_tree(source_id, destination_tree_id,
        &clm_string_env_lex_rules, CLM_STRING_FORMAT_ROOT_OBJECT,
        clm_string_env_internal_parse);
}

/** Reads UTF-8 ENV text from a filename registered by CLM file IDs. */
static inline clm_int clm_string_env_load(
    const clm_id filename_id, const clm_id destination_tree_id)
{
    return clm_string_format_internal_load_file(filename_id,
        destination_tree_id, &clm_string_env_lex_rules,
        CLM_STRING_FORMAT_ROOT_OBJECT, clm_string_env_internal_parse);
}

/** Serializes an ENV tree into an existing CLM string block. */
static inline clm_int clm_string_env_dumps(
    const clm_id object_id, const clm_id destination_id)
{
    return clm_string_format_serialize_tree(object_id, destination_id,
        clm_string_env_internal_serialize);
}

/** Writes an ENV tree to a filename registered by CLM file IDs. */
static inline clm_int clm_string_env_dump(
    const clm_id object_id, const clm_id filename_id)
{
    return clm_string_format_internal_dump_file(filename_id,
        object_id, clm_string_env_internal_serialize);
}


/** Opt-in parsing: automatically map previously unknown field/section names.
 * New clm_id mappings survive success and are rolled back on parse failure. */
static inline clm_int clm_string_env_loads_auto_ids(
    const clm_id source_id, const clm_id destination_tree_id)
{
    return clm_string_format_loads_tree_with_options(source_id,
        destination_tree_id, &clm_string_env_lex_rules,
        CLM_STRING_FORMAT_ROOT_OBJECT,
        clm_string_env_internal_parse, true);
}

static inline clm_int clm_string_env_load_auto_ids(
    const clm_id filename_id, const clm_id destination_tree_id)
{
    return clm_string_format_internal_load_file_with_options(filename_id,
        destination_tree_id, &clm_string_env_lex_rules,
        CLM_STRING_FORMAT_ROOT_OBJECT,
        clm_string_env_internal_parse, true);
}

