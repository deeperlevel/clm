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

#include "string_ini.h"

static const clm_string_format_lex_rules clm_string_ini_lex_rules = {
    "[]=:", ";#", NULL, NULL, "\"'", '\\',
    CLM_STRING_FORMAT_LEX_EMIT_NEWLINES
};

clm_int clm_string_ini_internal_value(
    const char* text,
    const size_t offset,
    const size_t length,
    char** value,
    size_t* value_length) {
    if (length == 0) {
        *value = (char*)malloc(1);
        if (*value == NULL) return CLM_BPA_FAILURE;
        (*value)[0] = '\0';
        *value_length = 0;
        return CLM_BPA_SUCCESS;
    }
    if (text[offset] == '"' || text[offset] == '\'') {
        const char quote = text[offset];
        if (length < 2 || text[offset + length - 1] != quote)
            return CLM_BPA_FAILURE;
        if (quote == '"') {
            clm_string_format_parser parser = {0};
            clm_string_format_token token = {
                CLM_STRING_FORMAT_TOKEN_QUOTED, offset, length, 1, offset + 1
            };
            parser.lexer.input = text;
            parser.lexer.input_length = offset + length;
            return clm_string_format_decode_quoted(&parser, &token, value,
                                                   value_length);
        }
        const size_t inner_length = length - 2;
        char* copy = (char*)malloc(inner_length + 1);
        if (copy == NULL) return CLM_BPA_FAILURE;
        size_t used = 0;
        for (size_t index = 0; index < inner_length; ++index) {
            if (text[offset + 1 + index] == '\'' && index + 1 < inner_length &&
                text[offset + 2 + index] == '\'')
                ++index;
            copy[used++] = text[offset + 1 + index];
        }
        copy[used] = '\0';
        *value = copy;
        *value_length = used;
        return CLM_BPA_SUCCESS;
    }
    char* copy = (char*)malloc(length + 1);
    if (copy == NULL) return CLM_BPA_FAILURE;
    memcpy(copy, text + offset, length);
    copy[length] = '\0';
    *value = copy;
    *value_length = length;
    return CLM_BPA_SUCCESS;
}

clm_int clm_string_ini_internal_parse(
    clm_string_format_parser* parser,
    const clm_id root_id) {
    const char* text = parser->lexer.input;
    const size_t length = parser->lexer.input_length;
    clm_id current_object = root_id;
    size_t line_start = 0;
    while (line_start < length) {
        size_t line_end = line_start;
        while (line_end < length && text[line_end] != '\n' && text[line_end] != '\r')
            ++line_end;
        size_t offset = line_start;
        size_t line_length = line_end - line_start;
        clm_string_format_internal_trim_span(text, &offset, &line_length);
        if (line_length != 0 && text[offset] != '#' && text[offset] != ';') {
            if (text[offset] == '[' && text[offset + line_length - 1] == ']') {
                size_t name_offset = offset + 1;
            size_t name_length = line_length - 2;
                clm_string_format_internal_trim_span(text, &name_offset, &name_length);
                clm_id section_id;
                clm_id child_id;
                if (name_length == 0 ||
                    clm_string_format_internal_resolve_key_with_parser(parser, text + name_offset,
                        name_length, &section_id) != CLM_BPA_SUCCESS ||
                    clm_tree_contains(root_id, section_id) ||
                    clm_string_format_internal_add_object(root_id, section_id,
                        false, &child_id) != CLM_BPA_SUCCESS)
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
                current_object = child_id;
            } else {
                char quote = '\0';
                bool escaped = false;
                size_t separator = line_length;
                for (size_t index = 0; index < line_length; ++index) {
                    const char character = text[offset + index];
                    if (escaped) { escaped = false; continue; }
                    if (quote == '"' && character == '\\') { escaped = true; continue; }
                    if (quote != '\0') {
                        if (character == quote) quote = '\0';
                        continue;
                    }
                    if (character == '"' || character == '\'') { quote = character; continue; }
                    if (character == '=' || character == ':') { separator = index; break; }
                }
                if (separator == line_length) return CLM_FORMAT_DESERIALIZE_FAILURE;
                size_t key_offset = offset;
                size_t key_length = separator;
                clm_string_format_internal_trim_span(text, &key_offset, &key_length);
                size_t value_offset = offset + separator + 1;
                size_t value_length = line_length - separator - 1;
                clm_string_format_internal_trim_span(text, &value_offset, &value_length);
                clm_id key;
                char* value = NULL;
                size_t decoded_length = 0;
                if (key_length == 0 ||
                    clm_string_format_internal_resolve_key_with_parser(parser, text + key_offset,
                        key_length, &key) != CLM_BPA_SUCCESS ||
                    clm_tree_contains(current_object, key) ||
                    clm_string_ini_internal_value(text, value_offset, value_length,
                        &value, &decoded_length) != CLM_BPA_SUCCESS)
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
            const int set_status = clm_tree_set_from(current_object, key,
                CLM_BPA_TYPE_STRING, value, decoded_length + 1);
                free(value);
                if (set_status != CLM_BPA_SET_SUCCESS)
                    return CLM_FORMAT_DESERIALIZE_FAILURE;
            }
        }
        line_start = line_end;
        if (line_start < length && text[line_start] == '\r') ++line_start;
        if (line_start < length && text[line_start] == '\n') ++line_start;
    }
    return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

clm_int clm_string_ini_internal_serialize_object(
    const clm_id object_id,
    clm_string_format_writer* writer,
    const bool section) {
    const clm_int count = clm_tree_count(object_id);
    if (count < 0 || (size_t)count > (size_t)-1 / sizeof(clm_id))
        return CLM_FORMAT_SERIALIZE_FAILURE;
    clm_id* keys = count == 0 ? NULL : (clm_id*)malloc((size_t)count * sizeof(*keys));
    if ((count != 0 && keys == NULL) ||
        clm_dictionary_keys_to(object_id, keys, (size_t)count) != CLM_BPA_GET_SUCCESS) {
        free(keys);
        return CLM_FORMAT_SERIALIZE_FAILURE;
    }
    for (clm_int index = 0; index < count; ++index) {
        clm_byte type;
        if (clm_tree_get_type(object_id, keys[index], &type) != CLM_BPA_GET_SUCCESS)
            goto fail;
        if (type == CLM_BPA_TYPE_STRUCT) {
            clm_id child_id;
            if (section || clm_tree_get_object(object_id, keys[index], &child_id) !=
                    CLM_BPA_GET_SUCCESS ||
                clm_string_format_writer_append_char(writer, '[') != CLM_FORMAT_SERIALIZE_SUCCESS ||
                clm_string_format_writer_append_key_raw(writer, keys[index]) != CLM_FORMAT_SERIALIZE_SUCCESS ||
                clm_string_format_writer_append_cstr(writer, "]\n") != CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_ini_internal_serialize_object(child_id, writer, true) !=
                CLM_FORMAT_SERIALIZE_SUCCESS)
                goto fail;
            continue;
        }
        if (type != CLM_BPA_TYPE_STRING ||
            clm_string_format_writer_append_key_raw(writer, keys[index]) != CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_cstr(writer, " = ") != CLM_FORMAT_SERIALIZE_SUCCESS)
            goto fail;
        clm_id value_id;
        clm_string_format_writer value_writer;
        clm_string_format_writer_init(&value_writer);
        if (clm_tree_get_id(object_id, keys[index], &value_id) != CLM_BPA_GET_SUCCESS ||
            clm_string_format_writer_append_leaf_text(&value_writer, value_id) !=
                CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_quoted(writer, value_writer.data,
                value_writer.length) != CLM_FORMAT_SERIALIZE_SUCCESS ||
            clm_string_format_writer_append_char(writer, '\n') != CLM_FORMAT_SERIALIZE_SUCCESS) {
            clm_string_format_writer_dispose(&value_writer);
            goto fail;
        }
        clm_string_format_writer_dispose(&value_writer);
    }
    free(keys);
    return CLM_FORMAT_SERIALIZE_SUCCESS;
fail:
    free(keys);
    return CLM_FORMAT_SERIALIZE_FAILURE;
}

clm_int clm_string_ini_internal_serialize(
    clm_id root_id,
    clm_id destination_id) {
    clm_string_format_writer writer;
    clm_string_format_writer_init(&writer);
    const clm_int status = clm_string_ini_internal_serialize_object(root_id,
        &writer, false);
    const clm_int result = status == CLM_FORMAT_SERIALIZE_SUCCESS
        ? clm_string_format_writer_commit(&writer, destination_id)
        : CLM_FORMAT_SERIALIZE_FAILURE;
    clm_string_format_writer_dispose(&writer);
    return result;
}

clm_int clm_string_ini_loads(
    const clm_id source_id,
    const clm_id destination_tree_id) {
    return clm_string_format_loads_tree(source_id, destination_tree_id,
        &clm_string_ini_lex_rules, CLM_STRING_FORMAT_ROOT_OBJECT,
        clm_string_ini_internal_parse);
}

clm_int clm_string_ini_load(
    const clm_id filename_id,
    const clm_id destination_tree_id) {
    return clm_string_format_internal_load_file(filename_id,
        destination_tree_id, &clm_string_ini_lex_rules,
        CLM_STRING_FORMAT_ROOT_OBJECT, clm_string_ini_internal_parse);
}

clm_int clm_string_ini_dumps(
    const clm_id object_id,
    const clm_id destination_id) {
    return clm_string_format_serialize_tree(object_id, destination_id,
        clm_string_ini_internal_serialize);
}

clm_int clm_string_ini_dump(
    const clm_id object_id,
    const clm_id filename_id) {
    return clm_string_format_internal_dump_file(filename_id,
        object_id, clm_string_ini_internal_serialize);
}

clm_int clm_string_ini_loads_auto_ids(
    const clm_id source_id, const clm_id destination_tree_id) {
    return clm_string_format_loads_tree_with_options(source_id,
        destination_tree_id, &clm_string_ini_lex_rules,
        CLM_STRING_FORMAT_ROOT_OBJECT,
        clm_string_ini_internal_parse, true);
}

clm_int clm_string_ini_load_auto_ids(
    const clm_id filename_id, const clm_id destination_tree_id) {
    return clm_string_format_internal_load_file_with_options(filename_id,
        destination_tree_id, &clm_string_ini_lex_rules,
        CLM_STRING_FORMAT_ROOT_OBJECT,
        clm_string_ini_internal_parse, true);
}
