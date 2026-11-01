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

#include "string_toml.h"

static const clm_string_format_lex_rules clm_string_toml_lex_rules = {
    "[]=,", "#", NULL, NULL, "\"'", '\\',
    CLM_STRING_FORMAT_LEX_EMIT_NEWLINES
};

clm_int clm_string_toml_internal_parse_array(
    const char* text, const size_t offset, const size_t length,
    const clm_id parent_id, const bool parent_is_array, const clm_id key,
    const size_t depth) {
    if (depth > CLM_TREE_MAX_DEPTH || length < 2 || text[offset] != '[' ||
        text[offset + length - 1] != ']')
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    clm_id array_id;
    if (clm_string_format_internal_add_array(parent_id, key, parent_is_array,
            &array_id) != CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    const size_t end = offset + length - 1;
    size_t start = offset + 1;
    char quote = '\0';
    bool escaped = false;
    clm_int nesting = 0;
    for (size_t cursor = start; cursor <= end; ++cursor) {
        const char character = cursor < end ? text[cursor] : ',';
        if (escaped) { escaped = false; continue; }
        if (quote == '"' && character == '\\') { escaped = true; continue; }
        if (quote != '\0') {
            if (character == quote) quote = '\0';
            continue;
        }
        if (character == '"' || character == '\'') { quote = character; continue; }
        if (character == '[') { ++nesting; continue; }
        if (character == ']') { --nesting; continue; }
        if (character == ',' && nesting == 0) {
            size_t value_offset = start;
            size_t value_length = cursor - start;
            clm_string_format_internal_trim_span(text, &value_offset, &value_length);
            if (value_length != 0 &&
                clm_string_toml_internal_value(text, value_offset, value_length,
                    array_id, true, 0, depth + 1) !=
                CLM_FORMAT_DESERIALIZE_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
            start = cursor + 1;
        }
    }
    return quote == '\0' && nesting == 0
        ? CLM_FORMAT_DESERIALIZE_SUCCESS : CLM_FORMAT_DESERIALIZE_FAILURE;
}

clm_int clm_string_toml_internal_value(
    const char* text, size_t offset, size_t length,
    const clm_id parent_id, const bool parent_is_array, const clm_id key,
    const size_t depth) {
    if (depth > CLM_TREE_MAX_DEPTH || length == 0)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    clm_string_format_internal_trim_span(text, &offset, &length);
    if (length == 0)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    if (text[offset] == '[')
        return clm_string_toml_internal_parse_array(text, offset, length,
            parent_id, parent_is_array, key, depth);
    if (text[offset] == '"' || text[offset] == '\'') {
        const char quote = text[offset];
        if (length < 2 || text[offset + length - 1] != quote)
            return CLM_FORMAT_DESERIALIZE_FAILURE;
        char* value = NULL;
        size_t value_length = 0;
        if (quote == '"') {
            clm_string_format_parser parser = {0};
            clm_string_format_token token = {
                CLM_STRING_FORMAT_TOKEN_QUOTED, offset, length, 1, offset + 1
            };
            parser.lexer.input = text;
            parser.lexer.input_length = offset + length;
            if (clm_string_format_decode_quoted(&parser, &token, &value,
                    &value_length) != CLM_BPA_SUCCESS)
                return CLM_FORMAT_DESERIALIZE_FAILURE;
        } else {
            value_length = length - 2;
            value = (char*)malloc(value_length + 1);
            if (value == NULL) return CLM_FORMAT_DESERIALIZE_FAILURE;
            memcpy(value, text + offset + 1, value_length);
            value[value_length] = '\0';
        }
        const int result = clm_string_format_internal_set_leaf(parent_id,
            parent_is_array, key, CLM_BPA_TYPE_STRING, value, value_length + 1);
        free(value);
        return result == CLM_BPA_SUCCESS ? CLM_FORMAT_DESERIALIZE_SUCCESS
            : CLM_FORMAT_DESERIALIZE_FAILURE;
    }
    if ((length == 4 && memcmp(text + offset, "true", 4) == 0) ||
        (length == 5 && memcmp(text + offset, "false", 5) == 0))
        return clm_string_format_internal_set_boolean(parent_id, parent_is_array,
            key, length == 4) == CLM_BPA_SUCCESS
            ? CLM_FORMAT_DESERIALIZE_SUCCESS : CLM_FORMAT_DESERIALIZE_FAILURE;
    if (length == 4 && memcmp(text + offset, "null", 4) == 0)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    return clm_string_format_internal_parse_number(text + offset, length,
        parent_id, parent_is_array, key) == CLM_BPA_SUCCESS
        ? CLM_FORMAT_DESERIALIZE_SUCCESS : CLM_FORMAT_DESERIALIZE_FAILURE;
}

clm_int clm_string_toml_internal_parse(
    clm_string_format_parser* parser,
    const clm_id root_id) {
    const char* text = parser->lexer.input;
    const size_t length = parser->lexer.input_length;
    clm_id current = root_id;
    size_t line_start = 0;
    while (line_start < length) {
        size_t line_end = line_start;
        while (line_end < length && text[line_end] != '\n' && text[line_end] != '\r')
            ++line_end;
        size_t offset = line_start;
        size_t line_length = line_end - line_start;
        char quote = '\0';
        bool escaped = false;
        for (size_t index = 0; index < line_length; ++index) {
            const char character = text[offset + index];
            if (escaped) { escaped = false; continue; }
            if (quote == '"' && character == '\\') { escaped = true; continue; }
            if (quote != '\0') {
                if (character == quote) quote = '\0';
                continue;
            }
            if (character == '"' || character == '\'') quote = character;
            else if (character == '#') { line_length = index; break; }
        }
        clm_string_format_internal_trim_span(text, &offset, &line_length);
        if (line_length != 0) {
            if (text[offset] == '[' && text[offset + line_length - 1] == ']') {
                size_t name_offset = offset + 1;
                size_t name_length = line_length - 2;
                clm_string_format_internal_trim_span(text, &name_offset, &name_length);
                if (name_length == 0 || memchr(text + name_offset, '.', name_length) != NULL)
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
                clm_id key, child;
                if (clm_string_format_internal_resolve_key_with_parser(parser, text + name_offset,
                    name_length, &key) != CLM_BPA_SUCCESS ||
                clm_tree_contains(root_id, key) ||
                clm_string_format_internal_add_object(root_id, key, false,
                    &child) != CLM_BPA_SUCCESS)
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
                current = child;
            } else {
                quote = '\0'; escaped = false;
                size_t separator = line_length;
                for (size_t index = 0; index < line_length; ++index) {
                    const char character = text[offset + index];
                    if (escaped) { escaped = false; continue; }
                    if (quote == '"' && character == '\\') { escaped = true; continue; }
                    if (quote != '\0') {
                        if (character == quote) quote = '\0';
                        continue;
                    }
                    if (character == '"' || character == '\'') quote = character;
                    else if (character == '=') { separator = index; break; }
                }
                if (separator == line_length) return CLM_FORMAT_DESERIALIZE_FAILURE;
                size_t key_offset = offset, key_length = separator;
                clm_string_format_internal_trim_span(text, &key_offset, &key_length);
                size_t value_offset = offset + separator + 1;
                size_t value_length = line_length - separator - 1;
                clm_string_format_internal_trim_span(text, &value_offset, &value_length);
                clm_id key;
                if (key_length == 0 ||
                    clm_string_format_internal_resolve_key_with_parser(parser, text + key_offset,
                        key_length, &key) != CLM_BPA_SUCCESS ||
                    clm_tree_contains(current, key) ||
                    clm_string_toml_internal_value(text, value_offset,
                        value_length, current, false, key, 0) !=
                    CLM_FORMAT_DESERIALIZE_SUCCESS)
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
            }
        }
        line_start = line_end;
        if (line_start < length && text[line_start] == '\r') ++line_start;
        if (line_start < length && text[line_start] == '\n') ++line_start;
    }
    return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

clm_int clm_string_toml_internal_serialize_value(
    const clm_id value_id,
    clm_string_format_writer* writer,
    const size_t depth) {
    if (depth > CLM_TREE_MAX_DEPTH) return CLM_FORMAT_SERIALIZE_FAILURE;
    if (clm_tree_is_array(value_id)) {
        const clm_int count = clm_tree_array_count(value_id);
        if (count < 0 || clm_string_format_writer_append_char(writer, '[') !=
            CLM_FORMAT_SERIALIZE_SUCCESS) return CLM_FORMAT_SERIALIZE_FAILURE;
        for (clm_int index = 0; index < count; ++index) {
            clm_id element;
        if ((index != 0 && clm_string_format_writer_append_cstr(writer, ", ") !=
            CLM_FORMAT_SERIALIZE_SUCCESS) ||
                clm_tree_array_get_id(value_id, index, &element) != CLM_BPA_GET_SUCCESS ||
                clm_string_toml_internal_serialize_value(element, writer,
                    depth + 1) != CLM_FORMAT_SERIALIZE_SUCCESS)
                return CLM_FORMAT_SERIALIZE_FAILURE;
        }
        return clm_string_format_writer_append_char(writer, ']');
    }
    if (clm_tree_is_object(value_id)) return CLM_FORMAT_SERIALIZE_FAILURE;
    if (clm_tree_is_null(value_id)) return CLM_FORMAT_SERIALIZE_FAILURE;
    bool boolean_value;
    if (clm_tree_is_boolean(value_id, &boolean_value))
        return clm_string_format_writer_append_cstr(writer,
            boolean_value ? "true" : "false");
    const clm_bpa_block_node* block = clm_bpa_find_block(value_id);
    if (block == NULL) return CLM_FORMAT_SERIALIZE_FAILURE;
    if (block->type == CLM_BPA_TYPE_STRING) {
        const char* text = block->data != NULL ? (const char*)block->data : "";
        return clm_string_format_writer_append_quoted(writer, text, block->length);
    }
    return clm_string_format_writer_append_leaf_text(writer, value_id);
}

clm_int clm_string_toml_internal_serialize_object(
    const clm_id object_id,
    clm_string_format_writer* writer,
    clm_string_format_writer* path,
    const bool root,
    const size_t depth) {
    if (depth > CLM_TREE_MAX_DEPTH) return CLM_FORMAT_SERIALIZE_FAILURE;
    if (!root) {
        if (clm_string_format_writer_append_char(writer, '[') !=
            CLM_FORMAT_SERIALIZE_SUCCESS ||
        clm_string_format_writer_append(writer, path->data, path->length) !=
            CLM_FORMAT_SERIALIZE_SUCCESS ||
        clm_string_format_writer_append_cstr(writer, "]\n") !=
            CLM_FORMAT_SERIALIZE_SUCCESS) return CLM_FORMAT_SERIALIZE_FAILURE;
    }
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
        if (clm_tree_get_id(object_id, keys[index], &value_id) != CLM_BPA_GET_SUCCESS) {
            free(keys); return CLM_FORMAT_SERIALIZE_FAILURE;
        }
        if (clm_tree_is_object(value_id)) {
            const size_t old_length = path->length;
            if (old_length != 0 && clm_string_format_writer_append_char(path, '.') !=
                CLM_FORMAT_SERIALIZE_SUCCESS) { free(keys); return CLM_FORMAT_SERIALIZE_FAILURE; }
            if (clm_string_format_writer_append_key(path, keys[index]) !=
                CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_toml_internal_serialize_object(value_id, writer, path,
                false, depth + 1) != CLM_FORMAT_SERIALIZE_SUCCESS) {
                free(keys); return CLM_FORMAT_SERIALIZE_FAILURE;
            }
            path->length = old_length;
            if (path->data != NULL) path->data[old_length] = '\0';
            continue;
        }
        if (clm_tree_is_null(value_id)) {
            free(keys); return CLM_FORMAT_SERIALIZE_FAILURE;
        }
        if (clm_string_format_writer_append_key(writer, keys[index]) !=
            CLM_FORMAT_SERIALIZE_SUCCESS ||
        clm_string_format_writer_append_cstr(writer, " = ") !=
            CLM_FORMAT_SERIALIZE_SUCCESS ||
        clm_string_toml_internal_serialize_value(value_id, writer, depth + 1) !=
            CLM_FORMAT_SERIALIZE_SUCCESS ||
        clm_string_format_writer_append_char(writer, '\n') !=
            CLM_FORMAT_SERIALIZE_SUCCESS) {
            free(keys); return CLM_FORMAT_SERIALIZE_FAILURE;
        }
    }
    free(keys);
    return CLM_FORMAT_SERIALIZE_SUCCESS;
}

clm_int clm_string_toml_internal_serialize(
    const clm_id root_id, const clm_id destination_id) {
    clm_string_format_writer writer, path;
    clm_string_format_writer_init(&writer);
    clm_string_format_writer_init(&path);
    const int status = clm_string_toml_internal_serialize_object(
        root_id, &writer, &path, true, 0);
    const int result = status == CLM_FORMAT_SERIALIZE_SUCCESS
        ? clm_string_format_writer_commit(&writer, destination_id)
        : CLM_FORMAT_SERIALIZE_FAILURE;
    clm_string_format_writer_dispose(&path);
    clm_string_format_writer_dispose(&writer);
    return result;
}

clm_int clm_string_toml_internal_run(
    const char* source, const size_t length, const clm_id destination_id) {
    return source != NULL
        ? clm_string_format_parse_tree(source, length, &clm_string_toml_lex_rules,
            destination_id,
            clm_string_toml_internal_parse)
        : CLM_FORMAT_DESERIALIZE_FAILURE;
}

clm_int clm_string_toml_loads(
    const clm_id source_id, const clm_id destination_tree_id) {
    const clm_bpa_block_node* source = clm_bpa_find_block(source_id);
    size_t characters;
    if (source == NULL || source->type != CLM_BPA_TYPE_STRING ||
        source->length > INT_MAX || (source->length != 0 && source->data == NULL) ||
        clm_string_measure(source->data != NULL ? (const char*)source->data : "",
            source->length, CLM_STRING_ENCODING_UTF8, &characters) != CLM_BPA_SUCCESS)
        return CLM_FORMAT_DESERIALIZE_FAILURE;
    return clm_string_toml_internal_run(source->data != NULL
        ? (const char*)source->data : "", source->length, destination_tree_id);
}

clm_int clm_string_toml_load(
    const clm_id filename_id, const clm_id destination_tree_id) {
    return clm_string_format_internal_load_file(filename_id,
        destination_tree_id, &clm_string_toml_lex_rules,
        CLM_STRING_FORMAT_ROOT_OBJECT,
        clm_string_toml_internal_parse);
}

clm_int clm_string_toml_dumps(
    const clm_id object_id, const clm_id destination_id) {
    return clm_string_format_serialize_tree(object_id, destination_id,
        clm_string_toml_internal_serialize);
}

clm_int clm_string_toml_dump(
    const clm_id object_id, const clm_id filename_id) {
    return clm_string_format_internal_dump_file(filename_id,
        object_id, clm_string_toml_internal_serialize);
}

clm_int clm_string_toml_loads_auto_ids(
    const clm_id source_id, const clm_id destination_tree_id) {
    return clm_string_format_loads_tree_with_options(source_id,
        destination_tree_id, &clm_string_toml_lex_rules,
        CLM_STRING_FORMAT_ROOT_OBJECT, clm_string_toml_internal_parse, true);
}

clm_int clm_string_toml_load_auto_ids(
    const clm_id filename_id, const clm_id destination_tree_id) {
    return clm_string_format_internal_load_file_with_options(filename_id,
        destination_tree_id, &clm_string_toml_lex_rules,
        CLM_STRING_FORMAT_ROOT_OBJECT, clm_string_toml_internal_parse, true);
}
