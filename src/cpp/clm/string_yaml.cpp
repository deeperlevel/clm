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

static const clm_string_format_lex_rules clm_string_yaml_lex_rules = {
    ":-[]{}", "#", NULL, NULL, "\"'", '\\',
    CLM_STRING_FORMAT_LEX_EMIT_NEWLINES | CLM_STRING_FORMAT_LEX_TRACK_INDENT
};

typedef struct {
    size_t offset;
    size_t length;
    size_t indentation;
    size_t line;
} clm_string_yaml_line;

typedef struct {
    const char* text;
    clm_string_yaml_line* lines;
    size_t count;
    size_t position;
    clm_string_format_parser* context;
} clm_string_yaml_document;

static inline bool clm_string_yaml_internal_sequence_line(
    const clm_string_yaml_document* document,
    const size_t index)
{
    const clm_string_yaml_line* line = &document->lines[index];
    return line->length != 0 && document->text[line->offset] == '-' &&
        (line->length == 1 || document->text[line->offset + 1] == ' ' ||
         document->text[line->offset + 1] == '\t');
}

static inline clm_int clm_string_yaml_internal_prepare_lines(
    const char* text,
    const size_t length,
    clm_string_yaml_document* document)
{
    if (text == NULL || document == NULL ||
        length > ((size_t)-1 / sizeof(clm_string_yaml_line)) - 1)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    const size_t capacity = length + 1;
    clm_string_yaml_line* lines = (clm_string_yaml_line*)calloc(
        capacity, sizeof(*lines));
    if (lines == NULL)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    size_t line_start = 0;
    size_t line_number = 1;
    size_t count = 0;
    while (line_start <= length) {
        size_t line_end = line_start;
        while (line_end < length && text[line_end] != '\r' && text[line_end] != '\n')
            ++line_end;
        size_t indentation = 0;
        while (line_start + indentation < line_end &&
            text[line_start + indentation] == ' ')
            ++indentation;
        if (line_start + indentation < line_end &&
            text[line_start + indentation] == '\t') {
            free(lines);
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        }
        size_t content_end = line_end;
        char quote = '\0';
        bool escaped = false;
        for (size_t cursor = line_start + indentation; cursor < line_end; ++cursor) {
            const char character = text[cursor];
            if (escaped) { escaped = false; continue; }
            if (quote == '"' && character == '\\') { escaped = true; continue; }
            if (quote == '\'' && character == '\'' && cursor + 1 < line_end &&
                text[cursor + 1] == '\'') { ++cursor; continue; }
            if (quote != '\0') {
                if (character == quote) quote = '\0';
                continue;
            }
            if (character == '"' || character == '\'') { quote = character; continue; }
            if (character == '#' &&
                (cursor == line_start + indentation || text[cursor - 1] == ' ')) {
                content_end = cursor;
                break;
            }
        }
        while (content_end > line_start + indentation &&
            (text[content_end - 1] == ' ' || text[content_end - 1] == '\t'))
            --content_end;
        const size_t content_length = content_end - line_start - indentation;
        if (content_length != 0) {
            const char* content = text + line_start + indentation;
            if (!((content_length == 3 && memcmp(content, "---", 3) == 0) ||
                (content_length == 3 && memcmp(content, "...", 3) == 0))) {
                lines[count].offset = line_start + indentation;
                lines[count].length = content_length;
                lines[count].indentation = indentation;
                lines[count].line = line_number;
                ++count;
            }
        }
        if (line_end == length) break;
        line_start = line_end + 1;
        if (text[line_end] == '\r' && line_start < length && text[line_start] == '\n')
            ++line_start;
        ++line_number;
    }
    document->text = text;
    document->lines = lines;
    document->count = count;
    document->position = 0;
    return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

static inline clm_int clm_string_yaml_internal_scalar(
    const clm_string_yaml_document* document,
    const size_t offset,
    const size_t length,
    const clm_id parent_id,
    const bool parent_is_array,
    const clm_id key)
{
    size_t trimmed_offset = offset;
    size_t trimmed_length = length;
    clm_string_format_internal_trim_span(document->text, &trimmed_offset,
        &trimmed_length);
    if (trimmed_length == 0)
        return clm_string_format_internal_set_null(parent_id, parent_is_array,
            key) == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    const char* text = document->text;
    if ((text[trimmed_offset] == '[' && text[trimmed_offset + trimmed_length - 1] == ']') ||
        (text[trimmed_offset] == '{' && text[trimmed_offset + trimmed_length - 1] == '}'))
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    if (text[trimmed_offset] == '"' || text[trimmed_offset] == '\'') {
        char* value = NULL;
        size_t value_length = 0;
        if (text[trimmed_offset] == '"') {
            clm_string_format_parser parser = {0};
            clm_string_format_token token = {
                CLM_STRING_FORMAT_TOKEN_QUOTED, trimmed_offset, trimmed_length,
                1, trimmed_offset + 1
            };
            parser.lexer.input = text;
            parser.lexer.input_length = trimmed_offset + trimmed_length;
            if (clm_string_format_decode_quoted(&parser, &token, &value,
                    &value_length) != CLM_BPA_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
        } else {
            if (trimmed_length < 2 || text[trimmed_offset + trimmed_length - 1] != '\'')
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            value_length = trimmed_length - 2;
            value = (char*)malloc(value_length + 1);
            if (value == NULL) return CLM_FORMAT_DESERIALIZE_FAILURE;
            size_t used = 0;
            for (size_t index = 1; index < trimmed_length - 1; ++index) {
                if (text[trimmed_offset + index] == '\'' &&
                    index + 1 < trimmed_length - 1 &&
                    text[trimmed_offset + index + 1] == '\'') ++index;
                value[used++] = text[trimmed_offset + index];
            }
            value[used] = '\0';
            value_length = used;
        }
        const int result = clm_string_format_internal_set_leaf(parent_id,
            parent_is_array, key, CLM_BPA_TYPE_STRING, value, value_length + 1);
        free(value);
        return result == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    }
    if ((trimmed_length == 4 && memcmp(text + trimmed_offset, "true", 4) == 0) ||
        (trimmed_length == 5 && memcmp(text + trimmed_offset, "false", 5) == 0))
        return clm_string_format_internal_set_boolean(parent_id, parent_is_array,
            key, trimmed_length == 4) == CLM_BPA_SUCCESS
            ? CLM_FORMAT_DESERIALIZE_SUCCESS : CLM_FORMAT_DESERIALIZE_FAILURE;
    if (trimmed_length == 4 && memcmp(text + trimmed_offset, "null", 4) == 0)
        return clm_string_format_internal_set_null(parent_id, parent_is_array,
            key) == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    if (clm_string_format_internal_parse_number(text + trimmed_offset,
        trimmed_length, parent_id, parent_is_array, key) == CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_SUCCESS;
    char* value = (char*)malloc(trimmed_length + 1);
    if (value == NULL) return CLM_FORMAT_DESERIALIZE_FAILURE;
    memcpy(value, text + trimmed_offset, trimmed_length);
    value[trimmed_length] = '\0';
    const int result = clm_string_format_internal_set_leaf(parent_id,
        parent_is_array, key, CLM_BPA_TYPE_STRING, value, trimmed_length + 1);
    free(value);
    return result == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
        : CLM_FORMAT_DESERIALIZE_FAILURE;
}

static inline clm_int clm_string_yaml_internal_parse_block(
    clm_string_yaml_document* document,
    const clm_id container_id,
    const bool is_array,
    const size_t indentation,
    const size_t depth);

static inline size_t clm_string_yaml_internal_colon(
    const char* text,
    const size_t offset,
    const size_t length)
{
    char quote = '\0';
    bool escaped = false;
    for (size_t index = 0; index < length; ++index) {
        const char character = text[offset + index];
        if (escaped) { escaped = false; continue; }
        if (quote == '"' && character == '\\') { escaped = true; continue; }
        if (quote != '\0') {
            if (character == quote) quote = '\0';
            continue;
        }
        if (character == '"' || character == '\'') { quote = character; continue; }
        if (character == ':') return index;
    }
    return length;
}

static inline clm_int clm_string_yaml_internal_key(
    clm_string_yaml_document* document,
    size_t offset,
    size_t length,
    clm_id* key)
{
    clm_string_format_internal_trim_span(document->text, &offset, &length);
    if (length == 0) return CLM_FORMAT_DESERIALIZE_FAILURE;
    const char* text = document->text;
    char* name = NULL;
    size_t name_length = 0;
    if (text[offset] == '"' || text[offset] == '\'') {
        if (length < 2 || text[offset + length - 1] != text[offset])
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        if (text[offset] == '"') {
            clm_string_format_parser parser = {0};
            clm_string_format_token token = {
                CLM_STRING_FORMAT_TOKEN_QUOTED, offset, length, 1, offset + 1
            };
            parser.lexer.input = text;
            parser.lexer.input_length = offset + length;
            if (clm_string_format_decode_quoted(&parser, &token, &name,
                    &name_length) != CLM_BPA_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
        } else {
            name_length = length - 2;
            name = (char*)malloc(name_length + 1);
            if (name == NULL) return CLM_FORMAT_DESERIALIZE_FAILURE;
            memcpy(name, text + offset + 1, name_length);
            name[name_length] = '\0';
        }
        const int result = clm_string_format_internal_resolve_key_with_parser(document->context, name,
            name_length, key);
        free(name);
        return result == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    }
    return clm_string_format_internal_resolve_key_with_parser(document->context, text + offset, length, key) ==
        CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
        : CLM_FORMAT_DESERIALIZE_FAILURE;
}

static inline clm_int clm_string_yaml_internal_map_entry(
    clm_string_yaml_document* document,
    const clm_id object_id,
    const size_t key_offset,
    const size_t key_length,
    const size_t value_offset,
    const size_t value_length,
    const size_t logical_indentation,
    const size_t depth)
{
    clm_id key;
    if (clm_string_yaml_internal_key(document, key_offset, key_length, &key) !=
        CLM_FORMAT_DESERIALIZE_SUCCESS || clm_tree_contains(object_id, key))
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    size_t trimmed_offset = value_offset;
    size_t trimmed_length = value_length;
    clm_string_format_internal_trim_span(document->text, &trimmed_offset,
        &trimmed_length);
    if (trimmed_length != 0) {
        const int result = clm_string_yaml_internal_scalar(document,
            trimmed_offset, trimmed_length, object_id, false, key);
        ++document->position;
        return result;
    }
    if (document->position + 1 >= document->count ||
        document->lines[document->position + 1].indentation <= logical_indentation) {
        ++document->position;
        return clm_string_format_internal_set_null(object_id, false, key) ==
            CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    }
    ++document->position;
    const clm_string_yaml_line* child_line =
        &document->lines[document->position];
    const bool child_is_array = clm_string_yaml_internal_sequence_line(
        document, document->position);
    clm_id child_id;
    if ((child_is_array
            ? clm_string_format_internal_add_array(object_id, key, false, &child_id)
            : clm_string_format_internal_add_object(object_id, key, false, &child_id)) !=
        CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    return clm_string_yaml_internal_parse_block(document, child_id,
        child_is_array, child_line->indentation, depth + 1);
}

static inline clm_int clm_string_yaml_internal_parse_block(
    clm_string_yaml_document* document,
    const clm_id container_id,
    const bool is_array,
    const size_t indentation,
    const size_t depth)
{
    if (document == NULL || depth > CLM_TREE_MAX_DEPTH)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    while (document->position < document->count) {
        const clm_string_yaml_line* line = &document->lines[document->position];
        if (line->indentation < indentation)
            return CLM_FORMAT_DESERIALIZE_SUCCESS;
        if (line->indentation > indentation ||
            clm_string_yaml_internal_sequence_line(document, document->position) != is_array)
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        const size_t offset = line->offset;
        const size_t length = line->length;
        if (!is_array) {
            const size_t colon = clm_string_yaml_internal_colon(document->text,
                offset, length);
            if (colon == length)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            if (clm_string_yaml_internal_map_entry(document, container_id,
                offset, colon, offset + colon + 1, length - colon - 1,
                indentation, depth) != CLM_FORMAT_DESERIALIZE_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            continue;
        }
        size_t item_offset = offset + 1;
        size_t item_length = length - 1;
        clm_string_format_internal_trim_span(document->text, &item_offset,
            &item_length);
        if (item_length == 0) {
            ++document->position;
            if (document->position >= document->count ||
                document->lines[document->position].indentation <= indentation) {
                if (clm_tree_array_append_null(container_id) != CLM_BPA_SET_SUCCESS)
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
                continue;
            }
            const clm_string_yaml_line* child_line =
                &document->lines[document->position];
            const bool child_is_array = clm_string_yaml_internal_sequence_line(
                document, document->position);
            clm_id child_id;
            if ((child_is_array
                    ? clm_string_format_internal_add_array(container_id, 0, true, &child_id)
                    : clm_string_format_internal_add_object(container_id, 0, true, &child_id)) !=
                CLM_BPA_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            if (clm_string_yaml_internal_parse_block(document, child_id,
                child_is_array, child_line->indentation, depth + 1) !=
                CLM_FORMAT_DESERIALIZE_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            continue;
        }
        const size_t colon = clm_string_yaml_internal_colon(document->text,
            item_offset, item_length);
        if (colon != item_length) {
            clm_id child_id;
            if (clm_string_format_internal_add_object(container_id, 0, true,
                &child_id) != CLM_BPA_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            const size_t logical_indent = indentation + 2;
            if (clm_string_yaml_internal_map_entry(document, child_id,
                item_offset, colon, item_offset + colon + 1,
                item_length - colon - 1, logical_indent, depth + 1) !=
                CLM_FORMAT_DESERIALIZE_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            if (document->position < document->count &&
                document->lines[document->position].indentation > indentation) {
                const size_t continuation_indent =
                    document->lines[document->position].indentation;
                if (clm_string_yaml_internal_parse_block(document, child_id,
                    false, continuation_indent, depth + 1) !=
                    CLM_FORMAT_DESERIALIZE_SUCCESS)
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
            }
            continue;
        }
        if (clm_string_yaml_internal_scalar(document, item_offset, item_length,
            container_id, true, 0) != CLM_FORMAT_DESERIALIZE_SUCCESS)
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        ++document->position;
    }
    return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

static inline clm_int clm_string_yaml_internal_prepare(
    const char* input,
    const size_t input_length,
    clm_string_yaml_document* document)
{
    if (clm_string_yaml_internal_prepare_lines(input, input_length, document) !=
        CLM_FORMAT_DESERIALIZE_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    if (document->count != 0 && document->lines[0].indentation != 0) {
        free(document->lines);
        memset(document, 0, sizeof(*document));
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    }
    return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

static inline clm_int clm_string_yaml_internal_detect_root(
    const char* input,
    const size_t input_length,
    clm_string_format_root_kind* kind)
{
    clm_string_yaml_document document = {0};
    if (kind == NULL || clm_string_yaml_internal_prepare(input, input_length,
        &document) != CLM_FORMAT_DESERIALIZE_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    *kind = document.count != 0 &&
        clm_string_yaml_internal_sequence_line(&document, 0)
        ? CLM_STRING_FORMAT_ROOT_ARRAY : CLM_STRING_FORMAT_ROOT_OBJECT;
    free(document.lines);
    return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

static inline clm_int clm_string_yaml_internal_parse(
    clm_string_format_parser* parser,
    const clm_id root_id)
{
    clm_string_yaml_document document = {0};
    if (clm_string_yaml_internal_prepare(parser->lexer.input,
        parser->lexer.input_length, &document) != CLM_FORMAT_DESERIALIZE_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    document.context = parser;
    if (document.count == 0) {
        free(document.lines);
        return CLM_FORMAT_DESERIALIZE_SUCCESS;
    }
    const bool root_is_array = clm_tree_is_array(root_id);
    const bool detected_array = clm_string_yaml_internal_sequence_line(&document, 0);
    const int result = root_is_array == detected_array
        ? clm_string_yaml_internal_parse_block(&document, root_id,
            root_is_array, 0, 0)
        : CLM_FORMAT_DESERIALIZE_FAILURE;
    const bool consumed = document.position == document.count;
    free(document.lines);
    return result == CLM_FORMAT_DESERIALIZE_SUCCESS && consumed
        ? CLM_FORMAT_DESERIALIZE_SUCCESS : CLM_FORMAT_DESERIALIZE_FAILURE;
}

static inline clm_int clm_string_yaml_internal_emit_indent(
    clm_string_format_writer* writer,
    const size_t indentation)
{
    for (size_t index = 0; index < indentation; ++index)
        if (clm_string_format_writer_append_char(writer, ' ') !=
            CLM_FORMAT_SERIALIZE_SUCCESS)
            return CLM_FORMAT_SERIALIZE_FAILURE;
    return CLM_FORMAT_SERIALIZE_SUCCESS;
}

static inline clm_int clm_string_yaml_internal_emit_node(
    const clm_id node_id,
    clm_string_format_writer* writer,
    const size_t indentation,
    const size_t depth);

static inline clm_int clm_string_yaml_internal_emit_scalar(
    const clm_id value_id,
    clm_string_format_writer* writer)
{
    clm_string_format_writer value;
    clm_string_format_writer_init(&value);
    const int status = clm_string_format_writer_append_leaf_text(&value, value_id);
    const int result = status == CLM_FORMAT_SERIALIZE_SUCCESS
        ? clm_string_format_writer_append_quoted(writer, value.data, value.length)
        : CLM_FORMAT_SERIALIZE_FAILURE;
    clm_string_format_writer_dispose(&value);
    return result;
}

static inline clm_int clm_string_yaml_internal_emit_node(
    const clm_id node_id,
    clm_string_format_writer* writer,
    const size_t indentation,
    const size_t depth)
{
    if (depth > CLM_TREE_MAX_DEPTH) return CLM_FORMAT_SERIALIZE_FAILURE;
    if (clm_tree_is_object(node_id)) {
        const clm_int count = clm_tree_count(node_id);
        if (count < 0 || (size_t)count > (size_t)-1 / sizeof(clm_id))
            return CLM_FORMAT_SERIALIZE_FAILURE;
        if (count == 0)
            return clm_string_format_writer_append_cstr(writer, "{}\n");
        clm_id* keys = (clm_id*)malloc((size_t)count * sizeof(*keys));
        if (keys == NULL || clm_dictionary_keys_to(node_id, keys, (size_t)count) !=
            CLM_BPA_GET_SUCCESS) { free(keys); return CLM_FORMAT_SERIALIZE_FAILURE; }
        for (clm_int index = 0; index < count; ++index) {
            clm_id value_id;
            if (clm_string_yaml_internal_emit_indent(writer, indentation) !=
                CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_key(writer, keys[index]) !=
                CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_char(writer, ':') !=
                CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_tree_get_id(node_id, keys[index], &value_id) != CLM_BPA_GET_SUCCESS) {
                free(keys); return CLM_FORMAT_SERIALIZE_FAILURE;
            }
            if (clm_tree_is_object(value_id) || clm_tree_is_array(value_id)) {
                const clm_int child_count = clm_tree_is_array(value_id)
                    ? clm_tree_array_count(value_id) : clm_tree_count(value_id);
                if (child_count == 0) {
                    const char* empty = clm_tree_is_array(value_id) ? " []\n" : " {}\n";
                    if (clm_string_format_writer_append_cstr(writer, empty) !=
                        CLM_FORMAT_SERIALIZE_SUCCESS) { free(keys); return CLM_FORMAT_SERIALIZE_FAILURE; }
                } else if (clm_string_format_writer_append_char(writer, '\n') !=
                    CLM_FORMAT_SERIALIZE_SUCCESS ||
                    clm_string_yaml_internal_emit_node(value_id, writer,
                        indentation + 2, depth + 1) != CLM_FORMAT_SERIALIZE_SUCCESS) {
                    free(keys); return CLM_FORMAT_SERIALIZE_FAILURE;
                }
            } else if (clm_string_format_writer_append_char(writer, ' ') !=
                    CLM_FORMAT_SERIALIZE_SUCCESS ||
                clm_string_yaml_internal_emit_scalar(value_id, writer) !=
                    CLM_FORMAT_SERIALIZE_SUCCESS ||
                clm_string_format_writer_append_char(writer, '\n') !=
                    CLM_FORMAT_SERIALIZE_SUCCESS) {
                free(keys); return CLM_FORMAT_SERIALIZE_FAILURE;
            }
        }
        free(keys);
        return CLM_FORMAT_SERIALIZE_SUCCESS;
    }
    if (clm_tree_is_array(node_id)) {
        const clm_int count = clm_tree_array_count(node_id);
        if (count < 0) return CLM_FORMAT_SERIALIZE_FAILURE;
        if (count == 0)
            return clm_string_format_writer_append_cstr(writer, "[]\n");
        for (clm_int index = 0; index < count; ++index) {
            clm_id value_id;
            if (clm_string_yaml_internal_emit_indent(writer, indentation) !=
                CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_cstr(writer, "-") !=
                CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_tree_array_get_id(node_id, index, &value_id) != CLM_BPA_GET_SUCCESS)
                return CLM_FORMAT_SERIALIZE_FAILURE;
            if (clm_tree_is_object(value_id) || clm_tree_is_array(value_id)) {
                const clm_int child_count = clm_tree_is_array(value_id)
                    ? clm_tree_array_count(value_id) : clm_tree_count(value_id);
                if (child_count == 0) {
                    const char* empty = clm_tree_is_array(value_id) ? " []\n" : " {}\n";
                    if (clm_string_format_writer_append_cstr(writer, empty) !=
                        CLM_FORMAT_SERIALIZE_SUCCESS) return CLM_FORMAT_SERIALIZE_FAILURE;
                } else if (clm_string_format_writer_append_char(writer, '\n') !=
                    CLM_FORMAT_SERIALIZE_SUCCESS ||
                    clm_string_yaml_internal_emit_node(value_id, writer,
                        indentation + 2, depth + 1) != CLM_FORMAT_SERIALIZE_SUCCESS) {
                    return CLM_FORMAT_SERIALIZE_FAILURE;
                }
            } else if (clm_string_format_writer_append_char(writer, ' ') !=
                    CLM_FORMAT_SERIALIZE_SUCCESS ||
                clm_string_yaml_internal_emit_scalar(value_id, writer) !=
                    CLM_FORMAT_SERIALIZE_SUCCESS ||
                clm_string_format_writer_append_char(writer, '\n') !=
                    CLM_FORMAT_SERIALIZE_SUCCESS) {
                return CLM_FORMAT_SERIALIZE_FAILURE;
            }
        }
        return CLM_FORMAT_SERIALIZE_SUCCESS;
    }
    return CLM_FORMAT_SERIALIZE_FAILURE;
}

static inline clm_int clm_string_yaml_internal_serialize(
    const clm_id root_id, const clm_id destination_id)
{
    clm_string_format_writer writer;
    clm_string_format_writer_init(&writer);
    const int status = clm_string_yaml_internal_emit_node(root_id, &writer, 0, 0);
    const int result = status == CLM_FORMAT_SERIALIZE_SUCCESS
        ? clm_string_format_writer_commit(&writer, destination_id)
        : CLM_FORMAT_SERIALIZE_FAILURE;
    clm_string_format_writer_dispose(&writer);
    return result;
}

static inline clm_int clm_string_yaml_internal_parse_text(
    const char* source, const size_t length, const clm_id destination_id)
{
    clm_string_yaml_document document = {0};
    if (clm_string_yaml_internal_prepare(source, length, &document) !=
        CLM_FORMAT_DESERIALIZE_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    const clm_string_format_root_kind kind = document.count != 0 &&
        clm_string_yaml_internal_sequence_line(&document, 0)
        ? CLM_STRING_FORMAT_ROOT_ARRAY : CLM_STRING_FORMAT_ROOT_OBJECT;
    free(document.lines);
    return clm_string_format_parse_node(source, length,
        &clm_string_yaml_lex_rules, destination_id,
        kind, clm_string_yaml_internal_parse);
}

/** Parses an existing CLM string block into a new owned YAML tree. */
static inline clm_int clm_string_yaml_loads(
    const clm_id source_id, const clm_id destination_tree_id)
{
    const clm_bpa_block_node* source = clm_bpa_find_block(source_id);
    size_t characters;
    if (source == NULL || source->type != CLM_BPA_TYPE_STRING ||
        source->length > INT_MAX || (source->length != 0 && source->data == NULL) ||
        clm_string_measure(source->data != NULL ? (const char*)source->data : "",
            source->length, CLM_STRING_ENCODING_UTF8, &characters) != CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    return clm_string_yaml_internal_parse_text(source->data != NULL
        ? (const char*)source->data : "", source->length, destination_tree_id);
}

static inline clm_int clm_string_yaml_internal_load_file(
    const clm_id filename_id,
    const clm_id destination_tree_id)
{
    clm_id input_id;
    size_t length;
    if (clm_string_format_temp_string_new(destination_tree_id, &input_id) !=
        CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    const int read_status = clm_file_read_all_text(filename_id,
        clm_string_get_default_encoding(), input_id, &length);
    const clm_bpa_block_node* input = clm_bpa_find_block(input_id);
    clm_int status = read_status == CLM_FILE_OPERATION_SUCCESS && input != NULL
        ? clm_string_yaml_internal_parse_text(input->data != NULL
            ? (const char*)input->data : "", length, destination_tree_id)
        : CLM_FORMAT_DESERIALIZE_FAILURE;
    (void)clm_bpa_delete(input_id);
    return status;
}

/** Reads YAML text from a filename registered by CLM file IDs. */
static inline clm_int clm_string_yaml_load(
    const clm_id filename_id, const clm_id destination_tree_id)
{
    return clm_string_yaml_internal_load_file(filename_id, destination_tree_id);
}

/** Serializes a YAML object or array into an existing CLM string block. */
static inline clm_int clm_string_yaml_dumps(
    const clm_id object_id, const clm_id destination_id)
{
    return clm_string_format_serialize_tree(object_id, destination_id,
        clm_string_yaml_internal_serialize);
}

/** Writes a YAML tree to a filename registered by CLM file IDs. */
static inline clm_int clm_string_yaml_dump(
    const clm_id object_id, const clm_id filename_id)
{
    return clm_string_format_internal_dump_file(filename_id,
        object_id, clm_string_yaml_internal_serialize);
}


/** Opt-in YAML import with automatic clm_id mapping (rollback on error). */
static inline clm_int clm_string_yaml_loads_auto_ids(
    const clm_id source_id, const clm_id destination_tree_id)
{
    const clm_bpa_block_node* source = clm_bpa_find_block(source_id);
    size_t characters;
    if (source == NULL || source->type != CLM_BPA_TYPE_STRING ||
        source->length > INT_MAX || (source->length != 0 && source->data == NULL) ||
        clm_string_measure(source->data != NULL ? (const char*)source->data : "",
            source->length, CLM_STRING_ENCODING_UTF8, &characters) != CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    const char* text = source->data != NULL ? (const char*)source->data : "";
    clm_string_yaml_document document = {0};
    if (clm_string_yaml_internal_prepare(text, source->length, &document) !=
        CLM_FORMAT_DESERIALIZE_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    const clm_string_format_root_kind kind = document.count != 0 &&
        clm_string_yaml_internal_sequence_line(&document, 0)
        ? CLM_STRING_FORMAT_ROOT_ARRAY : CLM_STRING_FORMAT_ROOT_OBJECT;
    free(document.lines);
    return clm_string_format_parse_node_with_options(text, source->length,
        &clm_string_yaml_lex_rules, destination_tree_id, kind,
        clm_string_yaml_internal_parse, true);
}

static inline clm_int clm_string_yaml_load_auto_ids(
    const clm_id filename_id, const clm_id destination_tree_id)
{
    clm_id input_id;
    size_t length;
    if (clm_string_format_temp_string_new(destination_tree_id, &input_id) !=
        CLM_BPA_SUCCESS) return CLM_FORMAT_DESERIALIZE_FAILURE;
    const int read_status = clm_file_read_all_text(filename_id,
        clm_string_get_default_encoding(), input_id, &length);
    const int result = read_status == CLM_FILE_OPERATION_SUCCESS
        ? clm_string_yaml_loads_auto_ids(input_id, destination_tree_id)
        : CLM_FORMAT_DESERIALIZE_FAILURE;
    (void)clm_bpa_delete(input_id);
    return result;
}

