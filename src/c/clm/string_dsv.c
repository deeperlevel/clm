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

#include "string_dsv.h"

clm_int clm_string_dsv_internal_new_temp_string(
	const clm_id avoid_a,
	const clm_id avoid_b,
	clm_id* out_id) {
	if (out_id == NULL)
		return CLM_BPA_FAILURE;
	for (clm_id candidate = ~(clm_id)0; candidate != 0; --candidate) {
		if (candidate != avoid_a && candidate != avoid_b &&
			!clm_bpa_is_existing(candidate) && !clm_bpaa_is_existing(candidate) &&
			clm_string_new(candidate, "")) {
			*out_id = candidate;
			return CLM_BPA_SUCCESS;
		}
	}
	return CLM_BPA_FAILURE;
}

clm_int clm_string_dsv_internal_options(
	const clm_string_dsv_options* options,
	char separator[2],
	size_t* separator_length,
	bool* has_header) {
	if (separator == NULL || separator_length == NULL || has_header == NULL)
		return CLM_BPA_FAILURE;
	*has_header = options == NULL || options->has_header;
	if (options == NULL || options->separator_string_id == 0) {
		separator[0] = ',';
		*separator_length = 1;
		return CLM_BPA_SUCCESS;
	}
	clm_bpa_block_node* block;
	const char* text;
	size_t byte_length;
	size_t character_count;
	if (clm_string_get_block(options->separator_string_id, &block, &text,
			&byte_length, &character_count) != CLM_BPA_SUCCESS ||
		byte_length == 0 || byte_length > 2 || byte_length != character_count)
		return CLM_BPA_FAILURE;
	for (size_t index = 0; index < byte_length; ++index) {
		const unsigned char character = (unsigned char)text[index];
		if (character >= 0x80 || character == '\r' || character == '\n' ||
			character == '"' || character == '\'')
			return CLM_BPA_FAILURE;
		separator[index] = (char)character;
	}
	*separator_length = byte_length;
	return CLM_BPA_SUCCESS;
}

bool clm_string_dsv_internal_is_separator(
	const clm_string_dsv_internal_parser* parser,
	const size_t position,
	const char* text) {
	return position <= parser->length &&
		parser->separator_length <= parser->length - position &&
		memcmp(text + position, parser->separator,
			parser->separator_length) == 0;
}

clm_int clm_string_dsv_internal_parse_field(
	clm_string_dsv_internal_parser* parser,
	const clm_id field_string_id,
	bool* has_separator,
	bool* record_end) {
	clm_bpa_block_node* source_block;
	const char* text;
	size_t byte_length;
	size_t character_count;
	if (parser == NULL || has_separator == NULL || record_end == NULL ||
		clm_string_get_block(parser->source_string_id, &source_block, &text,
			&byte_length, &character_count) != CLM_BPA_SUCCESS ||
		byte_length != parser->length || parser->position > parser->length)
		return CLM_BPA_FAILURE;
	const size_t capacity = parser->length - parser->position + 1;
	char* decoded = (char*)malloc(capacity);
	if (decoded == NULL)
		return CLM_BPA_FAILURE;
	size_t used = 0;
	if (parser->position < parser->length &&
		(text[parser->position] == '"' ||
         (!parser->csv_quoting && text[parser->position] == '\''))) {
		const char quote = text[parser->position++];
		bool closed = false;
		while (parser->position < parser->length) {
			const char character = text[parser->position++];
			if (!parser->csv_quoting && (character == '\r' || character == '\n'))
				goto fail;
			if (!parser->csv_quoting && quote == '"' && character == '\\') {
				if (parser->position >= parser->length)
					goto fail;
				const char escaped = text[parser->position++];
				switch (escaped) {
				case '\\': decoded[used++] = '\\'; break;
				case '"': decoded[used++] = '"'; break;
				case 'n': decoded[used++] = '\n'; break;
				case 't': decoded[used++] = '\t'; break;
				default: goto fail;
				}
				continue;
			}
			if (character == quote) {
				if ((quote == '\'' || parser->csv_quoting) &&
					parser->position < parser->length &&
					text[parser->position] == quote) {
					decoded[used++] = quote;
					++parser->position;
					continue;
				}
				closed = true;
				break;
			}
			decoded[used++] = character;
		}
		if (!closed)
			goto fail;
	} else {
		while (parser->position < parser->length &&
			text[parser->position] != '\r' && text[parser->position] != '\n' &&
			!clm_string_dsv_internal_is_separator(parser, parser->position, text))
			decoded[used++] = text[parser->position++];
	}
	decoded[used] = '\0';
	if (clm_string_set(field_string_id, decoded) < 0)
		goto fail;
	free(decoded);
	*has_separator = false;
	*record_end = false;
	if (parser->position == parser->length) {
		*record_end = true;
		return CLM_BPA_SUCCESS;
	}
	if (clm_string_dsv_internal_is_separator(parser, parser->position, text)) {
		parser->position += parser->separator_length;
		*has_separator = true;
		return CLM_BPA_SUCCESS;
	}
	if (text[parser->position] == '\r') {
		++parser->position;
		if (parser->position < parser->length &&
			text[parser->position] == '\n')
			++parser->position;
		*record_end = true;
		return CLM_BPA_SUCCESS;
	}
	if (text[parser->position] == '\n') {
		++parser->position;
		*record_end = true;
		return CLM_BPA_SUCCESS;
	}
	return CLM_BPA_FAILURE;
fail:
	free(decoded);
	return CLM_BPA_FAILURE;
}

clm_int clm_string_dsv_internal_allocate_column_id(
	clm_id* out_id) {
	if (out_id == NULL)
		return CLM_BPA_FAILURE;
	for (;;) {
		const clm_id candidate = clm_id_gen_next();
		size_t name_length;
		if (candidate == CLM_ID_NOT_FOUND)
			return CLM_BPA_FAILURE;
		if (clm_id_get_name(candidate, NULL, 0, &name_length) !=
			CLM_BPA_GET_SUCCESS) {
			*out_id = candidate;
			return CLM_BPA_SUCCESS;
		}
	}
}

clm_int clm_string_dsv_internal_track_name(
	clm_string_dsv_internal_registered_names* registered,
	const char* name,
	const size_t length,
	char** copy_out) {
	if (registered == NULL || name == NULL || copy_out == NULL ||
		length == (size_t)-1)
		return CLM_BPA_FAILURE;
	if (registered->count == registered->capacity) {
		const size_t capacity = registered->capacity == 0 ? 4 :
			registered->capacity > (size_t)-1 / 2 ? (size_t)-1 :
			registered->capacity * 2;
		if (capacity == (size_t)-1 ||
			capacity > (size_t)-1 / sizeof(*registered->names))
			return CLM_BPA_FAILURE;
		char** names = (char**)realloc(registered->names,
			capacity * sizeof(*registered->names));
		if (names == NULL)
			return CLM_BPA_FAILURE;
		registered->names = names;
		registered->capacity = capacity;
	}
	char* copy = (char*)malloc(length + 1);
	if (copy == NULL)
		return CLM_BPA_FAILURE;
	memcpy(copy, name, length);
	copy[length] = '\0';
	*copy_out = copy;
	return CLM_BPA_SUCCESS;
}

clm_int clm_string_dsv_internal_add_column_name(
    const clm_id table_id,
    const char* name,
    clm_string_dsv_internal_registered_names* registered) {
    clm_id column_id;
    size_t byte_length;
    if (name == NULL || (byte_length = strlen(name)) == 0)
        return CLM_BPA_FAILURE;
    if (clm_id_try_get(name, &column_id) == CLM_BPA_GET_SUCCESS)
        return clm_table_add_column(table_id, name) == CLM_BPA_SET_SUCCESS
            ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
    if (clm_string_dsv_internal_allocate_column_id(&column_id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    char* tracked_name;
    if (clm_string_dsv_internal_track_name(registered, name, byte_length,
            &tracked_name) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (clm_id_register(name, column_id) != CLM_BPA_SET_SUCCESS) {
        free(tracked_name);
        return CLM_BPA_FAILURE;
    }
    registered->names[registered->count++] = tracked_name;
    return clm_table_add_column(table_id, name) == CLM_BPA_SET_SUCCESS
        ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

clm_int clm_string_dsv_internal_add_header_column(
    const clm_id table_id,
    const clm_id name_string_id,
    clm_string_dsv_internal_registered_names* registered) {
    clm_bpa_block_node* name_block;
    const char* name;
    size_t byte_length;
    size_t character_count;
    if (clm_string_get_block(name_string_id, &name_block, &name, &byte_length,
            &character_count) != CLM_BPA_SUCCESS || byte_length == 0)
        return CLM_BPA_FAILURE;
    return clm_string_dsv_internal_add_column_name(table_id, name, registered);
}

void clm_string_dsv_internal_release_names(
	clm_string_dsv_internal_registered_names* registered,
	const bool rollback) {
	if (registered == NULL)
		return;
	for (size_t index = 0; index < registered->count; ++index) {
		if (rollback)
			(void)clm_id_remove(registered->names[index]);
		free(registered->names[index]);
	}
	free(registered->names);
	memset(registered, 0, sizeof(*registered));
}

clm_int clm_string_dsv_internal_parse(
	const clm_id source_string_id,
	const clm_id destination_table_id,
	const clm_string_dsv_options* options) {
	clm_bpa_block_node* source_block;
	const char* source_text;
	size_t source_length;
	size_t source_characters;
	clm_string_dsv_internal_parser parser = {0};
	clm_string_dsv_internal_registered_names registered = {0};
	clm_id field_string_id = 0;
	bool has_header;
	if (clm_string_get_block(source_string_id, &source_block, &source_text,
			&source_length, &source_characters) != CLM_BPA_SUCCESS ||
		clm_string_dsv_internal_options(options, parser.separator,
			&parser.separator_length, &has_header) != CLM_BPA_SUCCESS ||
		clm_table_new(destination_table_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	if (clm_string_dsv_internal_new_temp_string(source_string_id,
		destination_table_id, &field_string_id) != CLM_BPA_SUCCESS) {
		(void)clm_table_delete(destination_table_id);
		return CLM_BPA_FAILURE;
	}
	parser.source_string_id = source_string_id;
	parser.length = source_length;
	parser.csv_quoting = options != NULL && options->csv_quoting;
	size_t expected_columns = 0;
	bool first_record = true;
	clm_int status = CLM_BPA_SUCCESS;
	while (parser.position < parser.length) {
		const bool header_record = has_header && first_record;
		const clm_int row = !header_record
			? clm_table_append_row(destination_table_id) : CLM_BPA_FAILURE;
		if (!header_record && row < 0) {
			status = CLM_BPA_FAILURE;
			break;
		}
		size_t field_count = 0;
		bool more_fields;
		bool record_end;
		do {
			if (clm_string_dsv_internal_parse_field(&parser, field_string_id,
					&more_fields, &record_end) != CLM_BPA_SUCCESS ||
				field_count >= INT_MAX) {
				status = CLM_BPA_FAILURE;
				break;
			}
			if (header_record) {
				if (clm_string_dsv_internal_add_header_column(destination_table_id,
						field_string_id, &registered) != CLM_BPA_SUCCESS) {
					status = CLM_BPA_FAILURE;
					break;
				}
			} else {
                if (!has_header && first_record) {
                    char generated_name[64];
                    const int count = snprintf(generated_name,
                        sizeof(generated_name), "column_%zu", field_count + 1);
                    if (count < 0 || (size_t)count >= sizeof(generated_name) ||
                        clm_string_dsv_internal_add_column_name(destination_table_id,
                            generated_name, &registered) != CLM_BPA_SUCCESS) {
                        status = CLM_BPA_FAILURE;
                        break;
                    }
                }
				clm_id column_id;
				if (((!first_record || has_header) && field_count >= expected_columns) ||
					clm_table_column_id_at(destination_table_id,
						(clm_int)field_count, &column_id) != CLM_BPA_GET_SUCCESS ||
					clm_table_set(destination_table_id, row, column_id,
						field_string_id) != CLM_BPA_SET_SUCCESS) {
					status = CLM_BPA_FAILURE;
					break;
				}
			}
			++field_count;
		} while (more_fields);
		if (status != CLM_BPA_SUCCESS)
			break;
		if (header_record) {
			expected_columns = field_count;
			if (expected_columns == 0) {
				status = CLM_BPA_FAILURE;
				break;
			}
        } else if (!has_header && first_record) {
            expected_columns = field_count;
        } else if (field_count != expected_columns) {
            status = CLM_BPA_FAILURE;
            break;
        }
		if (!record_end) {
			status = CLM_BPA_FAILURE;
			break;
		}
		first_record = false;
	}
	(void)clm_string_delete(field_string_id);
	if (status != CLM_BPA_SUCCESS) {
		(void)clm_table_delete(destination_table_id);
		clm_string_dsv_internal_release_names(&registered, true);
		return CLM_BPA_FAILURE;
	}
	clm_string_dsv_internal_release_names(&registered, false);
	return CLM_BPA_SUCCESS;
}

clm_int clm_string_dsv_loads(
	const clm_id source_string_id,
	const clm_id destination_table_id) {
	return clm_string_dsv_internal_parse(source_string_id,
		destination_table_id, NULL);
}

clm_int clm_string_dsv_loads_with_options(
	const clm_id source_string_id,
	const clm_id destination_table_id,
	const clm_string_dsv_options* options) {
	return clm_string_dsv_internal_parse(source_string_id,
		destination_table_id, options);
}

clm_int clm_string_dsv_load_with_options(
	const clm_id filename_string_id,
	const clm_id destination_table_id,
	const clm_string_dsv_options* options) {
	clm_id input_string_id;
	size_t input_length;
	if (clm_string_dsv_internal_new_temp_string(filename_string_id,
		destination_table_id, &input_string_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	const clm_int read_status = clm_file_read_all_text(filename_string_id,
		CLM_FILE_TEXT_ENCODING_UTF8, input_string_id, &input_length);
	const clm_int status = read_status == CLM_FILE_OPERATION_SUCCESS
		? clm_string_dsv_internal_parse(input_string_id, destination_table_id,
			options) : CLM_BPA_FAILURE;
	(void)clm_string_delete(input_string_id);
	return status;
}

clm_int clm_string_dsv_load(
	const clm_id filename_string_id,
	const clm_id destination_table_id) {
	return clm_string_dsv_load_with_options(filename_string_id,
		destination_table_id, NULL);
}

clm_int clm_string_dsv_internal_writer_append(
	clm_string_dsv_internal_writer* writer,
	const char* text,
	const size_t length) {
	if (writer == NULL || (length != 0 && text == NULL) ||
		length > (size_t)INT_MAX - writer->length)
		return CLM_BPA_FAILURE;
	const size_t required = writer->length + length + 1;
	if (required > writer->capacity) {
		size_t capacity = writer->capacity == 0 ? 128 : writer->capacity;
		while (capacity < required) {
			if (capacity > (size_t)INT_MAX / 2) {
				capacity = required;
				break;
			}
			capacity *= 2;
		}
		char* data = (char*)realloc(writer->data, capacity);
		if (data == NULL)
			return CLM_BPA_FAILURE;
		writer->data = data;
		writer->capacity = capacity;
	}
	if (length != 0)
		memcpy(writer->data + writer->length, text, length);
	writer->length += length;
	writer->data[writer->length] = '\0';
	return CLM_BPA_SUCCESS;
}

clm_int clm_string_dsv_internal_writer_append_quoted(
	clm_string_dsv_internal_writer* writer,
	const clm_id value_string_id,
    const bool csv_quoting) {
	clm_bpa_block_node* block;
	const char* text;
	size_t byte_length;
	size_t character_count;
	if (clm_string_get_block(value_string_id, &block, &text, &byte_length,
			&character_count) != CLM_BPA_SUCCESS ||
		clm_string_dsv_internal_writer_append(writer, "\"", 1) !=
			CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	for (size_t index = 0; index < byte_length; ++index) {
		const char character = text[index];
		const char* escape = NULL;
		size_t escape_length = 0;
		switch (character) {
        case '\\': if (!csv_quoting) {escape = "\\\\"; escape_length = 2;} break;
        case '"': escape = csv_quoting ? "\"\"" : "\\\""; escape_length = 2; break;
        case '\n': if (!csv_quoting) {escape = "\\n"; escape_length = 2;} break;
        case '\t': if (!csv_quoting) {escape = "\\t"; escape_length = 2;} break;
        case '\r': if (!csv_quoting) return CLM_BPA_FAILURE; break;
		default: break;
		}
		if (escape != NULL) {
			if (clm_string_dsv_internal_writer_append(writer, escape,
				escape_length) != CLM_BPA_SUCCESS)
				return CLM_BPA_FAILURE;
		} else if (clm_string_dsv_internal_writer_append(writer, &character, 1) !=
			CLM_BPA_SUCCESS) {
			return CLM_BPA_FAILURE;
		}
	}
	return clm_string_dsv_internal_writer_append(writer, "\"", 1);
}

clm_int clm_string_dsv_internal_load_column_name(
	const clm_id table_id,
	const clm_int column_index,
	const clm_id destination_string_id) {
	clm_id column_id;
	if (clm_table_column_id_at(table_id, column_index, &column_id) !=
		CLM_BPA_GET_SUCCESS)
		return CLM_BPA_FAILURE;
	size_t capacity = 32;
	while (capacity <= (size_t)INT_MAX) {
		char* name = (char*)malloc(capacity);
		if (name == NULL)
			return CLM_BPA_FAILURE;
		const clm_int length = clm_table_column_name_to(table_id, column_index,
			name, capacity);
		if (length >= 0) {
			const clm_int result = clm_string_set(destination_string_id, name) >= 0
				? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
			free(name);
			return result;
		}
		free(name);
		if (capacity > (size_t)INT_MAX / 2)
			capacity = (size_t)INT_MAX;
		else
			capacity *= 2;
	}
	return CLM_BPA_FAILURE;
}

clm_int clm_string_dsv_dumps_with_options(
	const clm_id table_id,
	const clm_id destination_string_id,
	const clm_string_dsv_options* options) {
	clm_string_dsv_internal_writer writer = {0};
	clm_bpa_block_node* destination_block;
	const char* destination_text;
	size_t destination_length;
	size_t destination_characters;
	char separator[2];
	size_t separator_length;
	bool has_header;
	const clm_int column_count = clm_table_column_count(table_id);
	const clm_int row_count = clm_table_row_count(table_id);
	clm_id field_string_id;
	clm_int status = CLM_BPA_FAILURE;
	if (column_count < 0 || row_count < 0 ||
		clm_string_get_block(destination_string_id, &destination_block,
			&destination_text, &destination_length, &destination_characters) !=
			CLM_BPA_SUCCESS ||
		clm_string_dsv_internal_options(options, separator, &separator_length,
			&has_header) != CLM_BPA_SUCCESS ||
		clm_string_dsv_internal_new_temp_string(table_id, destination_string_id,
			&field_string_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	if (has_header && column_count > 0) {
		for (clm_int column = 0; column < column_count; ++column) {
			if ((column != 0 && clm_string_dsv_internal_writer_append(&writer,
				separator, separator_length) != CLM_BPA_SUCCESS) ||
			clm_string_dsv_internal_load_column_name(table_id, column,
				field_string_id) != CLM_BPA_SUCCESS ||
			clm_string_dsv_internal_writer_append_quoted(&writer,
				field_string_id, options != NULL && options->csv_quoting) != CLM_BPA_SUCCESS)
				goto done;
		}
		if (clm_string_dsv_internal_writer_append(&writer, "\n", 1) !=
			CLM_BPA_SUCCESS)
			goto done;
	}
	for (clm_int row = 0; row < row_count; ++row) {
		for (clm_int column = 0; column < column_count; ++column) {
			clm_id column_id;
			if ((column != 0 && clm_string_dsv_internal_writer_append(&writer,
				separator, separator_length) != CLM_BPA_SUCCESS) ||
			clm_table_column_id_at(table_id, column, &column_id) !=
				CLM_BPA_GET_SUCCESS)
				goto done;
		if (clm_table_cell_contains(table_id, row, column_id)) {
			if (clm_table_get(table_id, row, column_id, field_string_id) !=
				CLM_BPA_GET_SUCCESS)
				goto done;
		} else if (clm_string_set(field_string_id, "") < 0) {
			goto done;
		}
			if (clm_string_dsv_internal_writer_append_quoted(&writer,
				field_string_id, options != NULL && options->csv_quoting) != CLM_BPA_SUCCESS)
				goto done;
		}
		if (clm_string_dsv_internal_writer_append(&writer, "\n", 1) !=
			CLM_BPA_SUCCESS)
			goto done;
	}
	if (clm_string_set(destination_string_id,
		writer.data != NULL ? writer.data : "") >= 0)
		status = CLM_BPA_SUCCESS;
done:
	(void)clm_string_delete(field_string_id);
	free(writer.data);
	return status;
}

clm_int clm_string_dsv_dumps(
	const clm_id table_id,
	const clm_id destination_string_id) {
	return clm_string_dsv_dumps_with_options(table_id, destination_string_id,
		NULL);
}

clm_int clm_string_dsv_dump_with_options(
	const clm_id table_id,
	const clm_id filename_string_id,
	const clm_string_dsv_options* options) {
	clm_id output_string_id;
	if (clm_string_dsv_internal_new_temp_string(table_id, filename_string_id,
			&output_string_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	clm_int status = CLM_BPA_FAILURE;
	if (clm_string_dsv_dumps_with_options(table_id, output_string_id, options) ==
		CLM_BPA_SUCCESS) {
		clm_bpa_block_node* output_block;
		const char* output_text;
		size_t output_length;
		size_t output_characters;
		if (clm_string_get_block(output_string_id, &output_block, &output_text,
				&output_length, &output_characters) == CLM_BPA_SUCCESS &&
			clm_file_write_all_text(filename_string_id,
				CLM_FILE_TEXT_ENCODING_UTF8, output_string_id,
				output_block->length) == CLM_FILE_OPERATION_SUCCESS)
			status = CLM_BPA_SUCCESS;
	}
	(void)clm_string_delete(output_string_id);
	return status;
}

clm_int clm_string_dsv_dump(
	const clm_id table_id,
	const clm_id filename_string_id) {
	return clm_string_dsv_dump_with_options(table_id, filename_string_id, NULL);
}
