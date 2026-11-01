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

#include "clp.h"
#include "id.h"

#include <stdlib.h>

clm_int clm_clp_internal_track_name(clm_clp_internal_names* registered,const char* name) {
    if (registered->count == registered->capacity) {
        const size_t capacity = registered->capacity == 0 ? 4 :
            registered->capacity > (size_t)-1 / 2 ? (size_t)-1 :
            registered->capacity * 2;
        if (capacity == (size_t)-1 ||
            capacity > (size_t)-1 / sizeof(*registered->values)) {
            return CLM_CLP_OPERATION_FAILURE;
        }
        char** values = realloc(registered->values, capacity * sizeof(*registered->values));
        if (values == NULL) {
            return CLM_CLP_OPERATION_FAILURE;
        }
        registered->values = values;
        registered->capacity = capacity;
    }
    const size_t length = strlen(name);
    char* copy = malloc(length + 1);
    if (copy == NULL) {
        return CLM_CLP_OPERATION_FAILURE;
    }
    memcpy(copy, name, length + 1);
    registered->values[registered->count++] = copy;
    return CLM_CLP_OPERATION_SUCCESS;
}

void clm_clp_internal_release_names(clm_clp_internal_names* registered, const bool rollback) {
    for (size_t index = 0; index < registered->count; ++index) {
        if (rollback) {
            (void)clm_id_remove(registered->values[index]);
        }
        free(registered->values[index]);
    }
    free(registered->values);
    memset(registered, 0, sizeof(*registered));
}

clm_int clm_clp_internal_resolve_key(const char* name, clm_clp_internal_names* registered, clm_id* out_id) {
    if (name == NULL || name[0] == '\0' || out_id == NULL) {
        return CLM_CLP_OPERATION_INVALID_SYNTAX;
    }
    if (clm_id_try_get(name, out_id) == CLM_BPA_GET_SUCCESS) {
        return CLM_CLP_OPERATION_SUCCESS;
    }
    for (;;) {
        const clm_id candidate = clm_id_gen_next();
        size_t name_length;
        if (candidate == CLM_ID_NOT_FOUND) {
            return CLM_CLP_OPERATION_FAILURE;
        }
        if (clm_id_get_name(candidate, NULL, 0, &name_length) == CLM_BPA_GET_SUCCESS) {
            continue;
        }
        if (clm_clp_internal_track_name(registered, name) != CLM_CLP_OPERATION_SUCCESS) {
            return CLM_CLP_OPERATION_FAILURE;
        }
        if (clm_id_register(name, candidate) != CLM_BPA_SET_SUCCESS) {
            free(registered->values[--registered->count]);
            return CLM_CLP_OPERATION_FAILURE;
        }
        *out_id = candidate;
        return CLM_CLP_OPERATION_SUCCESS;
    }
}

clm_int clm_clp_internal_set_value(
    const clm_id dictionary_id,
    const char* name,
    const char* value,
    clm_clp_internal_names* registered
) {
    clm_id key;
    const clm_int key_status = clm_clp_internal_resolve_key(name, registered, &key);
    if (key_status != CLM_CLP_OPERATION_SUCCESS) {
        return key_status;
    }
    const size_t length = strlen(value);
    return clm_typed_dictionary_set_from(dictionary_id, key, value, length + 1) == CLM_BPA_SET_SUCCESS
        ? CLM_CLP_OPERATION_SUCCESS
        : CLM_CLP_OPERATION_FAILURE;
}

bool clm_clp_internal_is_option_token(const char* token) {
    if (token == NULL || token[0] == '\0') {
        return false;
    }
    if (strchr(token, '=') != NULL) {
        return true;
    }
    if (token[0] != '-') {
        return false;
    }
    if (token[1] == '-') {
        return token[2] != '\0';
    }
    if (token[1] == '\0') {
        return false;
    }
    return !((token[1] >= '0' && token[1] <= '9') || (token[1] == '.' && token[2] >= '0' && token[2] <= '9'));
}

clm_int clm_clp_internal_parse_arguments(
    const clm_int count,
    const char* const arguments[],
    const clm_id output_dictionary_id) {
    if (count < 0 || (count != 0 && arguments == NULL)) {
        return CLM_CLP_OPERATION_INVALID_ARGUMENT;
    }
    if (clm_bpa_is_existing(output_dictionary_id) || clm_bpaa_is_existing(output_dictionary_id)) {
        return CLM_CLP_OPERATION_DUPLICATE_ID;
    }
    if (clm_typed_dictionary_new(output_dictionary_id, CLM_BPA_TYPE_STRING, 0) != CLM_BPA_SUCCESS) {
        return CLM_CLP_OPERATION_FAILURE;
    }
    clm_clp_internal_names registered = {0};
    clm_int status = CLM_CLP_OPERATION_SUCCESS;
    for (clm_int index = 0; index < count; ++index) {
        const char* token = arguments[index];
        if (token == NULL || token[0] == '\0') {
            status = CLM_CLP_OPERATION_INVALID_SYNTAX;
            break;
        }
        const char* name = token;
        while (*name == '-') {
            ++name;
        }
        if (name == token && strchr(token, '=') == NULL) {
            status = CLM_CLP_OPERATION_INVALID_SYNTAX;
            break;
        }
        const char* equals = strchr(name, '=');
        const char* value = NULL;
        char* name_copy = NULL;
        if (equals != NULL) {
            const size_t name_length = (size_t)(equals - name);
            if (name_length == 0) {
                status = CLM_CLP_OPERATION_INVALID_SYNTAX;
                break;
            }
            name_copy = (char*)malloc(name_length + 1);
            if (name_copy == NULL) {
                status = CLM_CLP_OPERATION_FAILURE;
                break;
            }
            memcpy(name_copy, name, name_length);
            name_copy[name_length] = '\0';
            name = name_copy;
            value = equals + 1;
        } else if (index + 1 < count && !clm_clp_internal_is_option_token(arguments[index + 1])) {
            value = arguments[++index];
        } else {
            value = "true";
        }
        status = clm_clp_internal_set_value(output_dictionary_id, name, value, &registered);
        free(name_copy);
        if (status != CLM_CLP_OPERATION_SUCCESS) {
            break;
        }
    }
    if (status != CLM_CLP_OPERATION_SUCCESS) {
        (void)clm_typed_dictionary_delete(output_dictionary_id);
        clm_clp_internal_release_names(&registered, true);
        return status;
    }
    clm_clp_internal_release_names(&registered, false);
    return CLM_CLP_OPERATION_SUCCESS;
}

bool clm_clp_internal_space(const unsigned char value) {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\v' || value == '\f';
}

clm_int clm_clp_internal_buffer_append(clm_clp_internal_buffer* buffer, const char value) {
    if (buffer->length == buffer->capacity) {
        const size_t capacity = buffer->capacity == 0 ? 32 : buffer->capacity * 2;
        if (capacity < buffer->capacity) {
            return CLM_CLP_OPERATION_FAILURE;
        }
        char* data = realloc(buffer->data, capacity);
        if (data == NULL) {
            return CLM_CLP_OPERATION_FAILURE;
        }
        buffer->data = data;
        buffer->capacity = capacity;
    }
    buffer->data[buffer->length++] = value;
    return CLM_CLP_OPERATION_SUCCESS;
}

clm_int clm_clp_internal_push_token(
    clm_clp_internal_tokens* tokens,
    clm_clp_internal_buffer* buffer) {
    if (tokens->count == tokens->capacity) {
        const size_t capacity = tokens->capacity == 0 ? 4 : tokens->capacity * 2;
        if (capacity < tokens->capacity || capacity > (size_t)-1 / sizeof(*tokens->values)) {
            return CLM_CLP_OPERATION_FAILURE;
        }
        char** values = (char**)realloc(tokens->values, capacity * sizeof(*tokens->values));
        if (values == NULL) {
            return CLM_CLP_OPERATION_FAILURE;
        }
        tokens->values = values;
        tokens->capacity = capacity;
    }
    char* value = malloc(buffer->length + 1);
    if (value == NULL) {
        return CLM_CLP_OPERATION_FAILURE;
    }
    if (buffer->length != 0) {
        memcpy(value, buffer->data, buffer->length);
    }
    value[buffer->length] = '\0';
    tokens->values[tokens->count++] = value;
    buffer->length = 0;
    return CLM_CLP_OPERATION_SUCCESS;
}

void clm_clp_internal_free_tokens(clm_clp_internal_tokens* tokens) {
    for (size_t index = 0; index < tokens->count; ++index) {
        free(tokens->values[index]);
    }
    free(tokens->values);
    memset(tokens, 0, sizeof(*tokens));
}

clm_int clm_clp_internal_tokenize(const char* command_line, clm_clp_internal_tokens* tokens) {
    clm_clp_internal_buffer buffer = {0};
    char quote = '\0';
    bool escaped = false;
    bool token_started = false;
    clm_int status = CLM_CLP_OPERATION_SUCCESS;
    for (size_t index = 0; ; ++index) {
        const unsigned char current = (unsigned char)command_line[index];
        if (current == '\0' || (quote == '\0' && !escaped &&
            clm_clp_internal_space(current))) {
            if (escaped || quote != '\0') {
                status = CLM_CLP_OPERATION_INVALID_SYNTAX;
                break;
            }
            if (token_started) {
                status = clm_clp_internal_push_token(tokens, &buffer);
                if (status != CLM_CLP_OPERATION_SUCCESS) {
                    break;
                }
                token_started = false;
            }
            if (current == '\0') {
                break;
            }
            continue;
        }
        if (escaped) {
            status = clm_clp_internal_buffer_append(&buffer, (char)current);
            escaped = false;
            token_started = true;
        } else if (current == '\\') {
            escaped = true;
            token_started = true;
        } else if (quote != '\0') {
            if (current == (unsigned char)quote) {
                quote = '\0';
            }
            else {
                status = clm_clp_internal_buffer_append(&buffer, (char)current);
            }
        } else if (current == '\'' || current == '"') {
            quote = (char)current;
            token_started = true;
        } else {
            status = clm_clp_internal_buffer_append(&buffer, (char)current);
            token_started = true;
        }
        if (status != CLM_CLP_OPERATION_SUCCESS) {
            break;
        }
    }
    free(buffer.data);
    if (status != CLM_CLP_OPERATION_SUCCESS) {
        clm_clp_internal_free_tokens(tokens);
    }
    return status;
}

clm_int clm_clp_parse_argv(
    const clm_int argc,
    const char* const argv[],
    const clm_id output_dictionary_id) {
    if (argc < 0 || (argc > 0 && argv == NULL)) {
        return CLM_CLP_OPERATION_INVALID_ARGUMENT;
    }
    const clm_int count = argc > 0 ? argc - 1 : 0;
    const char* const* arguments = argc > 1 ? argv + 1 : NULL;
    return clm_clp_internal_parse_arguments(count, arguments, output_dictionary_id);
}

clm_int clm_clp_parse_tokens(
    const clm_int count,
    const char* const tokens[],
    const clm_id output_dictionary_id) {
    return clm_clp_internal_parse_arguments(count, tokens, output_dictionary_id);
}

clm_int clm_clp_parse_string(
    const char* command_line,
    const clm_id output_dictionary_id) {
    if (command_line == NULL) {
        return CLM_CLP_OPERATION_INVALID_ARGUMENT;
    }
    clm_clp_internal_tokens tokens = {0};
    const clm_int token_status = clm_clp_internal_tokenize(command_line, &tokens);
    if (token_status != CLM_CLP_OPERATION_SUCCESS) {
        return token_status;
    }
    if (tokens.count > INT_MAX) {
        clm_clp_internal_free_tokens(&tokens);
        return CLM_CLP_OPERATION_INVALID_ARGUMENT;
    }
    const clm_int status = clm_clp_internal_parse_arguments(
        // ReSharper disable once CppRedundantCastExpression
        (clm_int)tokens.count, (const char* const*)tokens.values, output_dictionary_id);
    clm_clp_internal_free_tokens(&tokens);
    return status;
}