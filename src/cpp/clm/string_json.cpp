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

/** JSON lexer profile; block comments are the documented CLM extension. */
static const clm_string_format_lex_rules clm_string_json_lex_rules = {
    "{}[],:", NULL, "/*", "*/", "\"", '\\', 0
};

static inline bool clm_string_json_internal_number(
    const char* text,
    const size_t length)
{
    size_t index = 0;
    if (index < length && text[index] == '-') ++index;
    if (index >= length) return false;
    if (text[index] == '0') {
        ++index;
        if (index < length && text[index] >= '0' && text[index] <= '9') return false;
    } else {
        if (text[index] < '1' || text[index] > '9') return false;
        while (index < length && text[index] >= '0' && text[index] <= '9') ++index;
    }
    if (index < length && text[index] == '.') {
        ++index;
        const size_t digits = index;
        while (index < length && text[index] >= '0' && text[index] <= '9') ++index;
        if (index == digits) return false;
    }
    if (index < length && (text[index] == 'e' || text[index] == 'E')) {
        ++index;
        if (index < length && (text[index] == '+' || text[index] == '-')) ++index;
        const size_t digits = index;
        while (index < length && text[index] >= '0' && text[index] <= '9') ++index;
        if (index == digits) return false;
    }
    return index == length;
}

static inline clm_int clm_string_json_internal_parse_value(
    clm_string_format_parser* parser,
    const clm_id parent_id,
    const bool parent_is_array,
    const clm_id key,
    const size_t depth);

static inline clm_int clm_string_json_internal_parse_object(
    clm_string_format_parser* parser,
    const clm_id object_id,
    const size_t depth)
{
    clm_string_format_token token;
    if (clm_string_format_parser_expect(parser,
        CLM_STRING_FORMAT_TOKEN_DELIMITER, "{", &token) != CLM_BPA_GET_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    if (clm_string_format_parser_peek(parser, &token) != CLM_BPA_GET_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    if (clm_string_format_token_equals(parser, &token, "}")) {
        (void)clm_string_format_parser_take(parser, &token);
        return CLM_FORMAT_DESERIALIZE_SUCCESS;
    }
    for (;;) {
        char* name = NULL;
        size_t name_length = 0;
        clm_id key;
        if (clm_string_format_parser_take(parser, &token) != CLM_BPA_GET_SUCCESS ||
            clm_string_format_decode_quoted(parser, &token, &name,
                &name_length) != CLM_BPA_SUCCESS ||
            clm_string_format_internal_resolve_key_with_parser(parser, name, name_length, &key) !=
                CLM_BPA_SUCCESS || clm_tree_contains(object_id, key)) {
            free(name);
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        }
        free(name);
        if (clm_string_format_parser_expect(parser,
            CLM_STRING_FORMAT_TOKEN_DELIMITER, ":", &token) != CLM_BPA_GET_SUCCESS ||
            clm_string_json_internal_parse_value(parser, object_id, false, key,
                depth + 1) != CLM_FORMAT_DESERIALIZE_SUCCESS ||
            clm_string_format_parser_peek(parser, &token) != CLM_BPA_GET_SUCCESS)
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        if (clm_string_format_token_equals(parser, &token, "}")) {
            (void)clm_string_format_parser_take(parser, &token);
            return CLM_FORMAT_DESERIALIZE_SUCCESS;
        }
        if (clm_string_format_parser_expect(parser,
            CLM_STRING_FORMAT_TOKEN_DELIMITER, ",", &token) != CLM_BPA_GET_SUCCESS ||
            clm_string_format_parser_peek(parser, &token) != CLM_BPA_GET_SUCCESS ||
            clm_string_format_token_equals(parser, &token, "}"))
            return CLM_FORMAT_DESERIALIZE_FAILURE;
    }
}

static inline clm_int clm_string_json_internal_parse_array(
    clm_string_format_parser* parser,
    const clm_id array_id,
    const size_t depth)
{
    clm_string_format_token token;
    if (clm_string_format_parser_expect(parser,
        CLM_STRING_FORMAT_TOKEN_DELIMITER, "[", &token) != CLM_BPA_GET_SUCCESS ||
        clm_string_format_parser_peek(parser, &token) != CLM_BPA_GET_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    if (clm_string_format_token_equals(parser, &token, "]")) {
        (void)clm_string_format_parser_take(parser, &token);
        return CLM_FORMAT_DESERIALIZE_SUCCESS;
    }
    for (;;) {
        if (clm_string_json_internal_parse_value(parser, array_id, true, 0,
            depth + 1) != CLM_FORMAT_DESERIALIZE_SUCCESS ||
            clm_string_format_parser_peek(parser, &token) != CLM_BPA_GET_SUCCESS)
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        if (clm_string_format_token_equals(parser, &token, "]")) {
            (void)clm_string_format_parser_take(parser, &token);
            return CLM_FORMAT_DESERIALIZE_SUCCESS;
        }
        if (clm_string_format_parser_expect(parser,
            CLM_STRING_FORMAT_TOKEN_DELIMITER, ",", &token) != CLM_BPA_GET_SUCCESS ||
            clm_string_format_parser_peek(parser, &token) != CLM_BPA_GET_SUCCESS ||
            clm_string_format_token_equals(parser, &token, "]"))
            return CLM_FORMAT_DESERIALIZE_FAILURE;
    }
}

static inline clm_int clm_string_json_internal_parse_value(
    clm_string_format_parser* parser,
    const clm_id parent_id,
    const bool parent_is_array,
    const clm_id key,
    const size_t depth)
{
    clm_string_format_token token;
    if (depth > CLM_TREE_MAX_DEPTH ||
        clm_string_format_parser_peek(parser, &token) != CLM_BPA_GET_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    if (clm_string_format_token_equals(parser, &token, "{") ||
        clm_string_format_token_equals(parser, &token, "[")) {
        const bool is_array = clm_string_format_token_equals(parser, &token, "[");
        clm_id child_id;
        const clm_int create_status = is_array
            ? clm_string_format_internal_add_array(parent_id, key, parent_is_array,
                &child_id)
            : clm_string_format_internal_add_object(parent_id, key, parent_is_array,
                &child_id);
        if (create_status != CLM_BPA_SUCCESS)
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        return is_array
            ? clm_string_json_internal_parse_array(parser, child_id, depth)
            : clm_string_json_internal_parse_object(parser, child_id, depth);
    }
    if (token.kind == CLM_STRING_FORMAT_TOKEN_QUOTED) {
        char* value = NULL;
        size_t value_length = 0;
        if (clm_string_format_parser_take(parser, &token) != CLM_BPA_GET_SUCCESS ||
            clm_string_format_decode_quoted(parser, &token, &value,
                &value_length) != CLM_BPA_SUCCESS)
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        const int result = clm_string_format_internal_set_leaf(parent_id,
            parent_is_array, key, CLM_BPA_TYPE_STRING, value, value_length + 1);
        free(value);
        return result == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    }
    if (token.kind != CLM_STRING_FORMAT_TOKEN_WORD ||
        clm_string_format_parser_take(parser, &token) != CLM_BPA_GET_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    const char* value = parser->lexer.input + token.offset;
    if (clm_string_format_token_equals(parser, &token, "true"))
        return clm_string_format_internal_set_boolean(parent_id, parent_is_array,
            key, true) == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    if (clm_string_format_token_equals(parser, &token, "false"))
        return clm_string_format_internal_set_boolean(parent_id, parent_is_array,
            key, false) == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    if (clm_string_format_token_equals(parser, &token, "null"))
        return clm_string_format_internal_set_null(parent_id, parent_is_array,
            key) == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    if (!clm_string_json_internal_number(value, token.length) ||
        clm_string_format_internal_parse_number(value, token.length,
            parent_id, parent_is_array, key) != CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

static inline clm_int clm_string_json_internal_parse(
    clm_string_format_parser* parser,
    const clm_id root_id)
{
    if (clm_tree_is_array(root_id)) {
        if (clm_string_json_internal_parse_array(parser, root_id, 0) !=
            CLM_FORMAT_DESERIALIZE_SUCCESS)
            return CLM_FORMAT_DESERIALIZE_FAILURE;
    } else if (clm_string_json_internal_parse_object(parser, root_id, 0) !=
        CLM_FORMAT_DESERIALIZE_SUCCESS) {
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    }
    clm_string_format_token token;
    return clm_string_format_parser_peek(parser, &token) == CLM_BPA_GET_SUCCESS &&
        token.kind == CLM_STRING_FORMAT_TOKEN_END
        ? CLM_FORMAT_DESERIALIZE_SUCCESS : CLM_FORMAT_DESERIALIZE_FAILURE;
}

static inline clm_int clm_string_json_internal_detect_root(
    const char* input,
    const size_t length,
    clm_string_format_root_kind* kind)
{
    clm_string_format_lexer lexer;
    clm_string_format_token token;
    if (kind == NULL || clm_string_format_lexer_init(&lexer, input, length,
        &clm_string_json_lex_rules) != CLM_BPA_SUCCESS ||
    clm_string_format_lexer_next(&lexer, &token) != CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    if (clm_string_format_token_equals(&(clm_string_format_parser){.lexer = lexer},
        &token, "{")) *kind = CLM_STRING_FORMAT_ROOT_OBJECT;
    else if (clm_string_format_token_equals(&(clm_string_format_parser){.lexer = lexer},
        &token, "[")) *kind = CLM_STRING_FORMAT_ROOT_ARRAY;
    else return CLM_FORMAT_DESERIALIZE_FAILURE;
    return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

static inline clm_int clm_string_json_internal_parse_text(
    const char* input,
    const size_t length,
    const clm_id destination_id)
{
    clm_string_format_root_kind kind;
    if (clm_string_json_internal_detect_root(input, length, &kind) !=
        CLM_FORMAT_DESERIALIZE_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    return clm_string_format_parse_node(input, length, &clm_string_json_lex_rules,
        destination_id, kind, clm_string_json_internal_parse);
}

static inline clm_int clm_string_json_internal_serialize_value(
    const clm_id node_id,
    clm_string_format_writer* writer,
    const size_t depth);

static inline clm_int clm_string_json_internal_serialize_object(
    const clm_id object_id,
    clm_string_format_writer* writer,
    const size_t depth)
{
    if (depth > CLM_TREE_MAX_DEPTH ||
        clm_string_format_writer_append_char(writer, '{') != CLM_FORMAT_SERIALIZE_SUCCESS)
        return CLM_FORMAT_SERIALIZE_FAILURE;
    const clm_int count = clm_tree_count(object_id);
    if (count < 0 || (size_t)count > (size_t)-1 / sizeof(clm_id))
        return CLM_FORMAT_SERIALIZE_FAILURE;
    clm_id* keys = count == 0 ? NULL : (clm_id*)malloc((size_t)count * sizeof(*keys));
    if ((count != 0 && keys == NULL) ||
        clm_dictionary_keys_to(object_id, keys, (size_t)count) != CLM_BPA_GET_SUCCESS) {
        free(keys); return CLM_FORMAT_SERIALIZE_FAILURE;
    }
    for (clm_int index = 0; index < count; ++index) {
        clm_id value_id;
        if ((index != 0 && clm_string_format_writer_append_char(writer, ',') !=
                CLM_FORMAT_SERIALIZE_SUCCESS) ||
            clm_string_format_writer_append_key(writer, keys[index]) !=
                CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_char(writer, ':') !=
                CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_tree_get_id(object_id, keys[index], &value_id) != CLM_BPA_GET_SUCCESS ||
            clm_string_json_internal_serialize_value(value_id, writer,
                depth + 1) != CLM_FORMAT_SERIALIZE_SUCCESS) {
            free(keys); return CLM_FORMAT_SERIALIZE_FAILURE;
        }
    }
    free(keys);
    return clm_string_format_writer_append_char(writer, '}');
}

static inline clm_int clm_string_json_internal_serialize_array(
    const clm_id array_id,
    clm_string_format_writer* writer,
    const size_t depth)
{
    if (depth > CLM_TREE_MAX_DEPTH ||
        clm_string_format_writer_append_char(writer, '[') != CLM_FORMAT_SERIALIZE_SUCCESS)
        return CLM_FORMAT_SERIALIZE_FAILURE;
    const clm_int count = clm_tree_array_count(array_id);
    if (count < 0) return CLM_FORMAT_SERIALIZE_FAILURE;
    for (clm_int index = 0; index < count; ++index) {
        clm_id value_id;
        if ((index != 0 && clm_string_format_writer_append_char(writer, ',') !=
                CLM_FORMAT_SERIALIZE_SUCCESS) ||
            clm_tree_array_get_id(array_id, index, &value_id) != CLM_BPA_GET_SUCCESS ||
            clm_string_json_internal_serialize_value(value_id, writer,
                depth + 1) != CLM_FORMAT_SERIALIZE_SUCCESS)
            return CLM_FORMAT_SERIALIZE_FAILURE;
    }
    return clm_string_format_writer_append_char(writer, ']');
}

static inline clm_int clm_string_json_internal_serialize_value(
    const clm_id node_id,
    clm_string_format_writer* writer,
    const size_t depth)
{
    if (clm_tree_is_object(node_id))
        return clm_string_json_internal_serialize_object(node_id, writer, depth);
    if (clm_tree_is_array(node_id))
        return clm_string_json_internal_serialize_array(node_id, writer, depth);
    clm_string_format_writer text;
    clm_string_format_writer_init(&text);
    const int status = clm_string_format_writer_append_leaf_text(&text, node_id);
    /* Preserve leaf types: strings are quoted, numbers/bools/null are JSON literals. */
    const clm_bpa_block_node* leaf = clm_bpa_find_block(node_id);
    const int result = status != CLM_FORMAT_SERIALIZE_SUCCESS
        ? CLM_FORMAT_SERIALIZE_FAILURE
        : leaf != NULL && leaf->type == CLM_BPA_TYPE_STRING
            ? clm_string_format_writer_append_quoted(writer, text.data, text.length)
            : clm_string_format_writer_append(writer, text.data, text.length);
    clm_string_format_writer_dispose(&text);
    return result;
}

static inline clm_int clm_string_json_internal_serialize(
    const clm_id root_id,
    const clm_id destination_id)
{
    clm_string_format_writer writer;
    clm_string_format_writer_init(&writer);
    const int status = clm_string_json_internal_serialize_value(root_id,
        &writer, 0);
    const int result = status == CLM_FORMAT_SERIALIZE_SUCCESS
        ? clm_string_format_writer_commit(&writer, destination_id)
        : CLM_FORMAT_SERIALIZE_FAILURE;
    clm_string_format_writer_dispose(&writer);
    return result;
}

/** Parses an existing CLM string block into a new owned JSON tree. */
static inline clm_int clm_string_json_loads(
    const clm_id source_id, const clm_id destination_tree_id)
{
    const clm_bpa_block_node* source = clm_bpa_find_block(source_id);
    size_t character_count;
    clm_string_format_root_kind kind;
    if (source == NULL || source->type != CLM_BPA_TYPE_STRING ||
        source->length > INT_MAX || (source->length != 0 && source->data == NULL) ||
        clm_string_measure(source->data != NULL ? (const char*)source->data : "",
            source->length, CLM_STRING_ENCODING_UTF8, &character_count) != CLM_BPA_SUCCESS ||
        clm_string_json_internal_detect_root(source->data != NULL
            ? (const char*)source->data : "", source->length, &kind) !=
            CLM_FORMAT_DESERIALIZE_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    return clm_string_format_parse_node(source->data != NULL
        ? (const char*)source->data : "", source->length,
        &clm_string_json_lex_rules, destination_tree_id, kind,
        clm_string_json_internal_parse);
}

/** Reads UTF-8 JSON text from a filename registered by CLM file IDs. */
static inline clm_int clm_string_json_load(
    const clm_id filename_id, const clm_id destination_tree_id)
{
    clm_id input_id;
    size_t input_length;
    if (clm_string_format_temp_string_new(destination_tree_id, &input_id) !=
        CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    clm_int status = clm_file_read_all_text(filename_id,
        CLM_FILE_TEXT_ENCODING_UTF8, input_id, &input_length);
    const clm_bpa_block_node* input = clm_bpa_find_block(input_id);
    clm_string_format_root_kind kind;
    if (status == CLM_FILE_OPERATION_SUCCESS && input != NULL &&
        input->type == CLM_BPA_TYPE_STRING &&
        clm_string_json_internal_detect_root(input->data != NULL
            ? (const char*)input->data : "", input_length, &kind) ==
            CLM_FORMAT_DESERIALIZE_SUCCESS)
        status = clm_string_format_parse_node(input->data != NULL
            ? (const char*)input->data : "", input_length,
            &clm_string_json_lex_rules, destination_tree_id, kind,
            clm_string_json_internal_parse);
    else status = CLM_FORMAT_DESERIALIZE_FAILURE;
    (void)clm_bpa_delete(input_id);
    return status == CLM_FORMAT_DESERIALIZE_SUCCESS ? status
        : CLM_FORMAT_DESERIALIZE_FAILURE;
}

/** Serializes a JSON object or array into an existing CLM string block. */
static inline clm_int clm_string_json_dumps(
    const clm_id object_id, const clm_id destination_id)
{
    return clm_string_format_serialize_tree(object_id, destination_id,
        clm_string_json_internal_serialize);
}

/** Writes a JSON tree to a filename registered by CLM file IDs. */
static inline clm_int clm_string_json_dump(
    const clm_id object_id, const clm_id filename_id)
{
    return clm_string_format_internal_dump_file(filename_id,
        object_id, clm_string_json_internal_serialize);
}


/** Opt-in JSON import with automatic clm_id name registration (rollback on error). */
static inline clm_int clm_string_json_loads_auto_ids(
    const clm_id source_id, const clm_id destination_tree_id)
{
    const clm_bpa_block_node* source = clm_bpa_find_block(source_id);
    size_t characters;
    clm_string_format_root_kind kind;
    if (source == NULL || source->type != CLM_BPA_TYPE_STRING ||
        source->length > INT_MAX || (source->length != 0 && source->data == NULL) ||
        clm_string_measure(source->data != NULL ? (const char*)source->data : "",
            source->length, CLM_STRING_ENCODING_UTF8, &characters) != CLM_BPA_SUCCESS ||
        clm_string_json_internal_detect_root(source->data != NULL
            ? (const char*)source->data : "", source->length, &kind) !=
            CLM_FORMAT_DESERIALIZE_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    return clm_string_format_parse_node_with_options(source->data != NULL
        ? (const char*)source->data : "", source->length,
        &clm_string_json_lex_rules, destination_tree_id, kind,
        clm_string_json_internal_parse, true);
}

static inline clm_int clm_string_json_load_auto_ids(
    const clm_id filename_id, const clm_id destination_tree_id)
{
    clm_id input_id;
    size_t length;
    if (clm_string_format_temp_string_new(destination_tree_id, &input_id) !=
        CLM_BPA_SUCCESS) return CLM_FORMAT_DESERIALIZE_FAILURE;
    const int read_status = clm_file_read_all_text(filename_id,
        CLM_FILE_TEXT_ENCODING_UTF8, input_id, &length);
    const int result = read_status == CLM_FILE_OPERATION_SUCCESS
        ? clm_string_json_loads_auto_ids(input_id, destination_tree_id)
        : CLM_FORMAT_DESERIALIZE_FAILURE;
    (void)clm_bpa_delete(input_id);
    return result;
}

