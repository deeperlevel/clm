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

#include "string_format.h"

void clm_string_format_writer_init(
	clm_string_format_writer* writer) {
	if (writer != NULL) {
		writer->data = NULL;
		writer->length = 0;
		writer->capacity = 0;
	}
}

void clm_string_format_writer_dispose(
	clm_string_format_writer* writer) {
	if (writer != NULL) {
		free(writer->data);
		writer->data = NULL;
		writer->length = 0;
		writer->capacity = 0;
	}
}

clm_int clm_string_format_writer_reserve(
	clm_string_format_writer* writer,
	const size_t additional) {
	if (writer == NULL || additional > (size_t)-1 - writer->length - 1)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	const size_t required = writer->length + additional + 1;
	if (required <= writer->capacity)
		return CLM_FORMAT_SERIALIZE_SUCCESS;
	size_t capacity = writer->capacity == 0 ? 128 : writer->capacity;
	while (capacity < required) {
		if (capacity > (size_t)-1 / 2) {
			capacity = required;
			break;
		}
		capacity *= 2;
	}
	char* grown = (char*)realloc(writer->data, capacity);
	if (grown == NULL)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	writer->data = grown;
	writer->capacity = capacity;
	return CLM_FORMAT_SERIALIZE_SUCCESS;
}

clm_int clm_string_format_writer_append(
	clm_string_format_writer* writer,
	const char* text,
	const size_t length) {
	if ((length != 0 && text == NULL) ||
		clm_string_format_writer_reserve(writer, length) !=
			CLM_FORMAT_SERIALIZE_SUCCESS)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	if (length != 0)
		memcpy(writer->data + writer->length, text, length);
	writer->length += length;
	writer->data[writer->length] = '\0';
	return CLM_FORMAT_SERIALIZE_SUCCESS;
}

clm_int clm_string_format_writer_append_cstr(
	clm_string_format_writer* writer,
	const char* text) {
	return text == NULL ? CLM_FORMAT_SERIALIZE_FAILURE
		: clm_string_format_writer_append(writer, text, strlen(text));
}

clm_int clm_string_format_writer_append_char(
	clm_string_format_writer* writer,
	const char character) {
	return clm_string_format_writer_append(writer, &character, 1);
}

clm_int clm_string_format_writer_append_quoted(
	clm_string_format_writer* writer,
	const char* text,
	const size_t length) {
	static const char hex[] = "0123456789abcdef";
	if (clm_string_format_writer_append_char(writer, '"') !=
		CLM_FORMAT_SERIALIZE_SUCCESS)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	for (size_t index = 0; index < length; ++index) {
		const unsigned char character = (unsigned char)text[index];
		const char* escape = NULL;
		switch (character) {
		case '"': escape = "\\\""; break;
		case '\\': escape = "\\\\"; break;
		case '\b': escape = "\\b"; break;
		case '\f': escape = "\\f"; break;
		case '\n': escape = "\\n"; break;
		case '\r': escape = "\\r"; break;
		case '\t': escape = "\\t"; break;
		default: break;
		}
		if (escape != NULL) {
			if (clm_string_format_writer_append_cstr(writer, escape) !=
				CLM_FORMAT_SERIALIZE_SUCCESS)
				return CLM_FORMAT_SERIALIZE_FAILURE;
		} else if (character < 0x20) {
			const char encoded[] = {'\\', 'u', '0', '0',
				hex[character >> 4], hex[character & 0x0f]};
			if (clm_string_format_writer_append(writer, encoded,
											 sizeof(encoded)) !=
				CLM_FORMAT_SERIALIZE_SUCCESS)
				return CLM_FORMAT_SERIALIZE_FAILURE;
		} else if (clm_string_format_writer_append_char(writer,
										(char)character) !=
				CLM_FORMAT_SERIALIZE_SUCCESS) {
			return CLM_FORMAT_SERIALIZE_FAILURE;
		}
	}
	return clm_string_format_writer_append_char(writer, '"');
}

clm_int clm_string_format_writer_commit(
	clm_string_format_writer* writer,
	const clm_id destination_string_id) {
	if (writer == NULL || writer->length >= UINT_MAX ||
		clm_bpa_find_block(destination_string_id) == NULL)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	if (writer->data == NULL &&
		clm_string_format_writer_reserve(writer, 0) !=
			CLM_FORMAT_SERIALIZE_SUCCESS)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	if (writer->length == 0)
		writer->data[0] = '\0';
	return clm_string_set(destination_string_id, writer->data) >= 0
		? CLM_FORMAT_SERIALIZE_SUCCESS : CLM_FORMAT_SERIALIZE_FAILURE;
}

clm_int clm_string_format_temp_string_new(
	const clm_id avoid_id,
	clm_id* out_id) {
	if (out_id == NULL || clm_bpa_global_blocks_count > (size_t)-1 -
		clm_bpa_global_arrays_count - 1)
		return CLM_BPA_FAILURE;
	const size_t attempts = clm_bpa_global_blocks_count +
		clm_bpa_global_arrays_count + 1;
	for (size_t index = 0; index <= attempts; ++index) {
		const clm_id candidate = ~(clm_id)0 - (clm_id)index;
		if (candidate != avoid_id && !clm_bpa_is_existing(candidate) &&
			!clm_bpaa_is_existing(candidate) && clm_string_new(candidate, "")) {
			*out_id = candidate;
			return CLM_BPA_SUCCESS;
		}
	}
	return CLM_BPA_FAILURE;
}

bool clm_string_format_internal_in_set(
	const char value,
	const char* set) {
	return set != NULL && strchr(set, value) != NULL;
}

bool clm_string_format_internal_starts_with(
	const char* input,
	const size_t length,
	const size_t offset,
	const char* marker) {
	if (marker == NULL || marker[0] == '\0')
		return false;
	const size_t marker_length = strlen(marker);
	return offset <= length && marker_length <= length - offset &&
		memcmp(input + offset, marker, marker_length) == 0;
}

void clm_string_format_internal_advance(
	clm_string_format_lexer* lexer) {
	const char current = lexer->input[lexer->offset++];
	if (current == '\r') {
		if (lexer->offset < lexer->input_length &&
			lexer->input[lexer->offset] == '\n')
			++lexer->offset;
		++lexer->line;
		lexer->column = 1;
		lexer->line_start = lexer->offset;
		lexer->at_line_start = true;
	} else if (current == '\n') {
		++lexer->line;
		lexer->column = 1;
		lexer->line_start = lexer->offset;
		lexer->at_line_start = true;
	} else {
		++lexer->column;
		lexer->at_line_start = false;
	}
}

clm_int clm_string_format_parse_delimited(
	const clm_id destination_table_id,
	const char* input,
	const size_t input_length,
	const clm_string_format_delimited_parse_rules* rules) {
	(void)destination_table_id;
	(void)input;
	(void)input_length;
	(void)rules;
	return CLM_FORMAT_DESERIALIZE_FAILURE;
}

clm_int clm_string_format_serialize_delimited(
	const clm_id table_id,
	const clm_string_format_delimited_serialize_rules* rules,
	const clm_id destination_string_id) {
	(void)table_id;
	(void)rules;
	(void)destination_string_id;
	return CLM_FORMAT_SERIALIZE_FAILURE;
}

clm_int clm_string_format_lexer_init(
	clm_string_format_lexer* lexer,
	const char* input,
	const size_t input_length,
	const clm_string_format_lex_rules* rules) {
	if (lexer == NULL || input == NULL || rules == NULL ||
		((rules->block_comment_start == NULL) !=
		 (rules->block_comment_end == NULL)) ||
		(rules->block_comment_start != NULL &&
		 (rules->block_comment_start[0] == '\0' ||
		  rules->block_comment_end[0] == '\0')))
		return CLM_BPA_FAILURE;
	lexer->input = input;
	lexer->input_length = input_length;
	lexer->offset = 0;
	lexer->line = 1;
	lexer->column = 1;
	lexer->line_start = 0;
	lexer->rules = rules;
	lexer->at_line_start = true;
	return CLM_BPA_SUCCESS;
}

clm_int clm_string_format_lexer_next(
	clm_string_format_lexer* lexer,
	clm_string_format_token* token) {
	if (lexer == NULL || token == NULL || lexer->input == NULL ||
		lexer->rules == NULL || lexer->offset > lexer->input_length)
		return CLM_BPA_FAILURE;

	for (;;) {
		if (lexer->offset >= lexer->input_length) {
			token->kind = CLM_STRING_FORMAT_TOKEN_END;
			token->offset = lexer->offset;
			token->length = 0;
			token->line = lexer->line;
			token->column = lexer->column;
			return CLM_BPA_SUCCESS;
		}

		if (lexer->at_line_start &&
			(lexer->rules->flags & CLM_STRING_FORMAT_LEX_TRACK_INDENT) != 0) {
			const size_t start = lexer->offset;
			const size_t line = lexer->line;
			while (lexer->offset < lexer->input_length &&
				(lexer->input[lexer->offset] == ' ' ||
				 lexer->input[lexer->offset] == '\t'))
				clm_string_format_internal_advance(lexer);
			if (lexer->offset != start) {
				token->kind = CLM_STRING_FORMAT_TOKEN_INDENT;
				token->offset = start;
				token->length = lexer->offset - start;
				token->line = line;
				token->column = 1;
				return CLM_BPA_SUCCESS;
			}
			lexer->at_line_start = false;
		}

		const unsigned char current = (unsigned char)lexer->input[lexer->offset];
		if (current == '\r' || current == '\n') {
			const size_t start = lexer->offset;
			const size_t line = lexer->line;
			const size_t column = lexer->column;
			clm_string_format_internal_advance(lexer);
			if ((lexer->rules->flags & CLM_STRING_FORMAT_LEX_EMIT_NEWLINES) != 0) {
				token->kind = CLM_STRING_FORMAT_TOKEN_NEWLINE;
				token->offset = start;
				token->length = lexer->offset - start;
				token->line = line;
				token->column = column;
				return CLM_BPA_SUCCESS;
			}
			continue;
		}
		if (current == ' ' || current == '\t' || current == '\f') {
			clm_string_format_internal_advance(lexer);
			continue;
		}

		if (clm_string_format_internal_in_set(
				(char)current, lexer->rules->line_comment_markers)) {
			while (lexer->offset < lexer->input_length &&
				lexer->input[lexer->offset] != '\r' &&
				lexer->input[lexer->offset] != '\n')
				clm_string_format_internal_advance(lexer);
			continue;
		}
		if (clm_string_format_internal_starts_with(
				lexer->input, lexer->input_length, lexer->offset,
				lexer->rules->block_comment_start)) {
			const size_t marker_length = strlen(lexer->rules->block_comment_start);
			for (size_t index = 0; index < marker_length; ++index)
				clm_string_format_internal_advance(lexer);
			while (lexer->offset < lexer->input_length &&
				!clm_string_format_internal_starts_with(
					lexer->input, lexer->input_length, lexer->offset,
					lexer->rules->block_comment_end))
				clm_string_format_internal_advance(lexer);
			if (lexer->offset >= lexer->input_length)
				return CLM_BPA_FAILURE;
			const size_t end_length = strlen(lexer->rules->block_comment_end);
			for (size_t index = 0; index < end_length; ++index)
				clm_string_format_internal_advance(lexer);
			continue;
		}

		const size_t start = lexer->offset;
		const size_t line = lexer->line;
		const size_t column = lexer->column;
		if (clm_string_format_internal_in_set(
				(char)current, lexer->rules->quote_characters)) {
			const char quote = (char)current;
			clm_string_format_internal_advance(lexer);
			bool closed = false;
			while (lexer->offset < lexer->input_length) {
				const char character = lexer->input[lexer->offset];
				if (character == '\r' || character == '\n')
					return CLM_BPA_FAILURE;
				if (lexer->rules->escape_character != '\0' &&
					character == lexer->rules->escape_character) {
					clm_string_format_internal_advance(lexer);
					if (lexer->offset >= lexer->input_length ||
						lexer->input[lexer->offset] == '\r' ||
						lexer->input[lexer->offset] == '\n')
						return CLM_BPA_FAILURE;
					clm_string_format_internal_advance(lexer);
					continue;
				}
				clm_string_format_internal_advance(lexer);
				if (character == quote) {
					closed = true;
					break;
				}
			}
			if (!closed)
				return CLM_BPA_FAILURE;
			token->kind = CLM_STRING_FORMAT_TOKEN_QUOTED;
			token->offset = start;
			token->length = lexer->offset - start;
			token->line = line;
			token->column = column;
			return CLM_BPA_SUCCESS;
		}

		if (clm_string_format_internal_in_set(
				(char)current, lexer->rules->delimiters)) {
			clm_string_format_internal_advance(lexer);
			token->kind = CLM_STRING_FORMAT_TOKEN_DELIMITER;
			token->offset = start;
			token->length = 1;
			token->line = line;
			token->column = column;
			return CLM_BPA_SUCCESS;
		}

		while (lexer->offset < lexer->input_length) {
			const unsigned char character =
				(unsigned char)lexer->input[lexer->offset];
			if (character == '\r' || character == '\n' || character == ' ' ||
				character == '\t' || character == '\f' ||
				clm_string_format_internal_in_set((char)character,
					lexer->rules->delimiters) ||
				clm_string_format_internal_in_set((char)character,
					lexer->rules->quote_characters) ||
				clm_string_format_internal_in_set((char)character,
					lexer->rules->line_comment_markers) ||
				clm_string_format_internal_starts_with(
					lexer->input, lexer->input_length, lexer->offset,
					lexer->rules->block_comment_start))
				break;
			clm_string_format_internal_advance(lexer);
		}
		if (lexer->offset == start)
			return CLM_BPA_FAILURE;
		token->kind = CLM_STRING_FORMAT_TOKEN_WORD;
		token->offset = start;
		token->length = lexer->offset - start;
		token->line = line;
		token->column = column;
		return CLM_BPA_SUCCESS;
	}
}

clm_int clm_string_format_parser_init(
	clm_string_format_parser* parser,
	const char* input,
	const size_t input_length,
	const clm_string_format_lex_rules* rules) {
	if (parser == NULL ||
		clm_string_format_lexer_init(&parser->lexer, input, input_length,
									 rules) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	memset(&parser->lookahead, 0, sizeof(parser->lookahead));
	memset(&parser->error, 0, sizeof(parser->error));
	parser->has_lookahead = false;
    parser->new_keys = NULL;
	return CLM_BPA_SUCCESS;
}

clm_int clm_string_format_parser_peek(
	clm_string_format_parser* parser,
	clm_string_format_token* token) {
	if (parser == NULL || token == NULL)
		return CLM_BPA_FAILURE;
	if (!parser->has_lookahead) {
		if (clm_string_format_lexer_next(&parser->lexer,
										 &parser->lookahead) != CLM_BPA_SUCCESS) {
			parser->error.code = CLM_BPA_FAILURE;
			parser->error.offset = parser->lexer.offset;
			parser->error.line = parser->lexer.line;
			parser->error.column = parser->lexer.column;
			return CLM_BPA_FAILURE;
		}
		parser->has_lookahead = true;
	}
	*token = parser->lookahead;
	return CLM_BPA_GET_SUCCESS;
}

clm_int clm_string_format_parser_take(
	clm_string_format_parser* parser,
	clm_string_format_token* token) {
	if (clm_string_format_parser_peek(parser, token) != CLM_BPA_GET_SUCCESS)
		return CLM_BPA_FAILURE;
	parser->has_lookahead = false;
	return CLM_BPA_GET_SUCCESS;
}

clm_int clm_string_format_parser_expect(
	clm_string_format_parser* parser,
	const clm_string_format_token_kind expected_kind,
	const char* expected_text,
	clm_string_format_token* token) {
	clm_string_format_token found;
	if (clm_string_format_parser_peek(parser, &found) != CLM_BPA_GET_SUCCESS ||
		found.kind != expected_kind ||
		(expected_text != NULL &&
		 (strlen(expected_text) != found.length ||
		  memcmp(parser->lexer.input + found.offset, expected_text,
				 found.length) != 0))) {
		if (parser != NULL) {
			parser->error.code = CLM_BPA_FAILURE;
			if (parser->has_lookahead) {
				parser->error.offset = parser->lookahead.offset;
				parser->error.line = parser->lookahead.line;
				parser->error.column = parser->lookahead.column;
			}
		}
		return CLM_BPA_FAILURE;
	}
	return clm_string_format_parser_take(parser, token);
}

bool clm_string_format_token_equals(
	const clm_string_format_parser* parser,
	const clm_string_format_token* token,
	const char* literal) {
	return parser != NULL && token != NULL && literal != NULL &&
		strlen(literal) == token->length &&
		memcmp(parser->lexer.input + token->offset, literal, token->length) == 0;
}

clm_int clm_string_format_token_copy(
	const clm_string_format_parser* parser,
	const clm_string_format_token* token,
	char** output) {
	if (parser == NULL || token == NULL || output == NULL ||
		token->offset > parser->lexer.input_length ||
		token->length > parser->lexer.input_length - token->offset ||
		token->length == (size_t)-1)
		return CLM_BPA_FAILURE;
	char* copy = (char*)malloc(token->length + 1);
	if (copy == NULL)
		return CLM_BPA_FAILURE;
	memcpy(copy, parser->lexer.input + token->offset, token->length);
	copy[token->length] = '\0';
	*output = copy;
	return CLM_BPA_SUCCESS;
}

int clm_string_format_internal_hex(const char value) {
	if (value >= '0' && value <= '9') return value - '0';
	if (value >= 'a' && value <= 'f') return value - 'a' + 10;
	if (value >= 'A' && value <= 'F') return value - 'A' + 10;
	return -1;
}

clm_int clm_string_format_decode_quoted(
	const clm_string_format_parser* parser,
	const clm_string_format_token* token,
	char** output,
	size_t* output_length) {
	clm_string_format_writer writer;
	if (parser == NULL || token == NULL || output == NULL || output_length == NULL ||
		token->kind != CLM_STRING_FORMAT_TOKEN_QUOTED || token->length < 2 ||
		parser->lexer.input[token->offset] != '"' ||
		parser->lexer.input[token->offset + token->length - 1] != '"')
		return CLM_BPA_FAILURE;
	clm_string_format_writer_init(&writer);
	const char* input = parser->lexer.input + token->offset;
	const size_t end = token->length - 1;
	for (size_t index = 1; index < end;) {
		const unsigned char character = (unsigned char)input[index++];
		if (character < 0x20)
			goto fail;
		if (character != '\\') {
			if (clm_string_format_writer_append(&writer,
				(const char*)&input[index - 1], 1) != CLM_FORMAT_SERIALIZE_SUCCESS)
				goto fail;
			continue;
		}
		if (index >= end)
			goto fail;
		const char escape = input[index++];
		char decoded;
		switch (escape) {
		case '"': decoded = '"'; break;
		case '\\': decoded = '\\'; break;
		case '/': decoded = '/'; break;
		case 'b': decoded = '\b'; break;
		case 'f': decoded = '\f'; break;
		case 'n': decoded = '\n'; break;
		case 'r': decoded = '\r'; break;
		case 't': decoded = '\t'; break;
		case 'u': {
			if (end - index < 4) goto fail;
			uint32_t scalar = 0;
			for (size_t digit = 0; digit < 4; ++digit) {
				const int hex = clm_string_format_internal_hex(input[index++]);
				if (hex < 0) goto fail;
				scalar = (scalar << 4) | (uint32_t)hex;
			}
			if (scalar >= 0xd800 && scalar <= 0xdbff) {
				if (end - index < 6 || input[index] != '\\' ||
					input[index + 1] != 'u') goto fail;
				index += 2;
				uint32_t low = 0;
				for (size_t digit = 0; digit < 4; ++digit) {
					const int hex = clm_string_format_internal_hex(input[index++]);
					if (hex < 0) goto fail;
					low = (low << 4) | (uint32_t)hex;
				}
				if (low < 0xdc00 || low > 0xdfff) goto fail;
				scalar = 0x10000 + ((scalar - 0xd800) << 10) + (low - 0xdc00);
			} else if (scalar >= 0xdc00 && scalar <= 0xdfff) {
				goto fail;
			}
			if (scalar == 0) goto fail;
			unsigned char encoded[4];
			const size_t encoded_length = clm_string_utf8_encode_one(scalar, encoded);
			if (clm_string_format_writer_append(&writer, (const char*)encoded,
													 encoded_length) !=
				CLM_FORMAT_SERIALIZE_SUCCESS) goto fail;
			continue;
		}
		default: goto fail;
		}
		if (clm_string_format_writer_append_char(&writer, decoded) !=
			CLM_FORMAT_SERIALIZE_SUCCESS)
			goto fail;
	}
	if (writer.data == NULL) {
		if (clm_string_format_writer_reserve(&writer, 0) !=
			CLM_FORMAT_SERIALIZE_SUCCESS)
			goto fail;
		writer.data[0] = '\0';
	}
	*output = writer.data;
	*output_length = writer.length;
	return CLM_BPA_SUCCESS;
fail:
	clm_string_format_writer_dispose(&writer);
	return CLM_BPA_FAILURE;
}

clm_int clm_string_format_internal_new_node_id(clm_id* out_id) {
	if (out_id == NULL || clm_bpa_global_arrays_count == (size_t)-1 ||
		clm_bpa_global_blocks_count > (size_t)-1 -
			clm_bpa_global_arrays_count - 1)
		return CLM_BPA_FAILURE;
	const size_t attempts = clm_bpa_global_blocks_count +
		clm_bpa_global_arrays_count + 1;
	for (size_t index = 0; index <= attempts; ++index) {
		const clm_id candidate = ~(clm_id)0 - (clm_id)index;
		if (!clm_bpa_is_existing(candidate) && !clm_bpaa_is_existing(candidate)) {
			*out_id = candidate;
			return CLM_BPA_SUCCESS;
		}
	}
	return CLM_BPA_FAILURE;
}

void clm_string_format_internal_trim_span(
	const char* text,
	size_t* offset,
	size_t* length) {
	while (*length != 0 && (text[*offset] == ' ' || text[*offset] == '\t')) {
		++*offset;
		--*length;
	}
	while (*length != 0 &&
		(text[*offset + *length - 1] == ' ' ||
		 text[*offset + *length - 1] == '\t'))
		--*length;
}

clm_int clm_string_format_internal_resolve_key(
	const char* name,
	const size_t length,
	clm_id* out_id) {
	if (name == NULL || out_id == NULL || length == 0 || length >= UINT_MAX)
		return CLM_BPA_FAILURE;
	char* terminated = (char*)malloc(length + 1);
	if (terminated == NULL)
		return CLM_BPA_FAILURE;
	memcpy(terminated, name, length);
	terminated[length] = '\0';
	const clm_int status = clm_id_try_get(terminated, out_id);
	free(terminated);
	return status == CLM_BPA_GET_SUCCESS ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

clm_int clm_string_format_internal_resolve_key_with_parser(
    clm_string_format_parser* parser,
    const char* name,
    const size_t length,
    clm_id* out_id) {
    if (clm_string_format_internal_resolve_key(name, length, out_id) ==
        CLM_BPA_SUCCESS)
        return CLM_BPA_SUCCESS;
    if (parser == NULL || parser->new_keys == NULL || name == NULL ||
        out_id == NULL || length == 0 || length >= UINT_MAX ||
        memchr(name, '\0', length) != NULL)
        return CLM_BPA_FAILURE;
    clm_string_format_key_registry* registry = parser->new_keys;
    if (registry->count == registry->capacity) {
        const size_t new_capacity = registry->capacity == 0 ? 8 :
            registry->capacity > (size_t)-1 / 2 ? 0 : registry->capacity * 2;
        if (new_capacity == 0 || new_capacity > (size_t)-1 / sizeof(char*))
            return CLM_BPA_FAILURE;
        char** names = (char**)realloc(registry->names,
            new_capacity * sizeof(*names));
        if (names == NULL) return CLM_BPA_FAILURE;
        registry->names = names;
        registry->capacity = new_capacity;
    }
    char* copy = (char*)malloc(length + 1);
    if (copy == NULL) return CLM_BPA_FAILURE;
    memcpy(copy, name, length);
    copy[length] = '\0';
    clm_id candidate;
    for (;;) {
        candidate = clm_id_gen_next();
        if (candidate == CLM_ID_NOT_FOUND) {
            free(copy); return CLM_BPA_FAILURE;
        }
        size_t unused;
        if (clm_id_get_name(candidate, NULL, 0, &unused) != CLM_BPA_GET_SUCCESS)
            break;
    }
    if (clm_id_register(copy, candidate) != CLM_BPA_SET_SUCCESS) {
        free(copy); return CLM_BPA_FAILURE;
    }
    registry->names[registry->count++] = copy;
    *out_id = candidate;
    return CLM_BPA_SUCCESS;
}

void clm_string_format_internal_release_keys(
    clm_string_format_key_registry* registry,
    const bool rollback) {
    if (registry == NULL) return;
    for (size_t index = 0; index < registry->count; ++index) {
        if (rollback) (void)clm_id_remove(registry->names[index]);
        free(registry->names[index]);
    }
    free(registry->names);
    memset(registry, 0, sizeof(*registry));
}

clm_int clm_string_format_internal_add_object(
	const clm_id parent_id,
	const clm_id key,
	const bool parent_is_array,
	clm_id* child_id) {
	if (child_id == NULL ||
		clm_string_format_internal_new_node_id(child_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	const clm_int result = parent_is_array
		? clm_tree_array_append_object(parent_id, *child_id)
		: clm_tree_add_object(parent_id, key, *child_id);
	return result == CLM_BPA_SET_SUCCESS ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

clm_int clm_string_format_internal_add_array(
	const clm_id parent_id,
	const clm_id key,
	const bool parent_is_array,
	clm_id* child_id) {
	if (child_id == NULL ||
		clm_string_format_internal_new_node_id(child_id) != CLM_BPA_SUCCESS)
		return CLM_BPA_FAILURE;
	const clm_int result = parent_is_array
		? clm_tree_array_append_array(parent_id, *child_id)
		: clm_tree_add_array(parent_id, key, *child_id);
	return result == CLM_BPA_SET_SUCCESS ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

clm_int clm_string_format_internal_set_leaf(
	const clm_id parent_id,
	const bool parent_is_array,
	const clm_id key,
	const clm_byte type,
	const void* value,
	const size_t size) {
	const clm_int result = parent_is_array
		? clm_tree_array_append_from(parent_id, type, value, size)
		: clm_tree_set_from(parent_id, key, type, value, size);
	return result == CLM_BPA_SET_SUCCESS ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

clm_int clm_string_format_internal_set_boolean(
	const clm_id parent_id,
	const bool parent_is_array,
	const clm_id key,
	const bool value) {
	const clm_int result = parent_is_array
		? clm_tree_array_append_boolean(parent_id, value)
		: clm_tree_set_boolean(parent_id, key, value);
	return result == CLM_BPA_SET_SUCCESS ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

clm_int clm_string_format_internal_set_null(
	const clm_id parent_id,
	const bool parent_is_array,
	const clm_id key) {
	const clm_int result = parent_is_array
		? clm_tree_array_append_null(parent_id)
		: clm_tree_set_null(parent_id, key);
	return result == CLM_BPA_SET_SUCCESS ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

clm_int clm_string_format_internal_parse_number(
	const char* text,
	const size_t length,
	const clm_id parent_id,
	const bool parent_is_array,
	const clm_id key) {
	if (text == NULL || length == 0 || length >= 128)
		return CLM_BPA_FAILURE;
	char number[128];
	memcpy(number, text, length);
	number[length] = '\0';
	bool floating = false;
	for (size_t index = 0; index < length; ++index)
		if (number[index] == '.' || number[index] == 'e' || number[index] == 'E')
			floating = true;
	if (floating) {
		char* end = NULL;
		errno = 0;
		const double value = strtod(number, &end);
		if (end != number + length || errno == ERANGE || !isfinite(value))
			return CLM_BPA_FAILURE;
		const clm_double stored = value;
		return clm_string_format_internal_set_leaf(parent_id, parent_is_array,
			key, CLM_BPA_TYPE_DOUBLE, &stored, sizeof(stored));
	}
	if (number[0] == '-') {
		char* end = NULL;
		errno = 0;
		const long long parsed = strtoll(number, &end, 10);
		if (end != number + length || errno == ERANGE ||
			parsed < (long long)LONG_MIN || parsed > (long long)LONG_MAX)
			return CLM_BPA_FAILURE;
		if (parsed >= INT_MIN && parsed <= INT_MAX) {
			const clm_int value = (clm_int)parsed;
			return clm_string_format_internal_set_leaf(parent_id, parent_is_array,
				key, CLM_BPA_TYPE_INT, &value, sizeof(value));
		}
		const clm_long value = (clm_long)parsed;
		return clm_string_format_internal_set_leaf(parent_id, parent_is_array,
			key, CLM_BPA_TYPE_LONG, &value, sizeof(value));
	}
	char* end = NULL;
	errno = 0;
	const unsigned long long parsed = strtoull(number, &end, 10);
	if (end != number + length || errno == ERANGE ||
		parsed > (unsigned long long)ULONG_MAX)
		return CLM_BPA_FAILURE;
	if (parsed <= (unsigned long long)INT_MAX) {
		const clm_int value = (clm_int)parsed;
		return clm_string_format_internal_set_leaf(parent_id, parent_is_array,
			key, CLM_BPA_TYPE_INT, &value, sizeof(value));
	}
	if (parsed <= (unsigned long long)LONG_MAX) {
		const clm_long value = (clm_long)parsed;
		return clm_string_format_internal_set_leaf(parent_id, parent_is_array,
			key, CLM_BPA_TYPE_LONG, &value, sizeof(value));
	}
	const clm_ulong value = (clm_ulong)parsed;
	return clm_string_format_internal_set_leaf(parent_id, parent_is_array,
		key, CLM_BPA_TYPE_UNSIGNED_LONG, &value, sizeof(value));
}

clm_int clm_string_format_writer_append_key(
	clm_string_format_writer* writer,
	const clm_id key) {
	size_t length;
	if (clm_id_get_name(key, NULL, 0, &length) != CLM_BPA_GET_SUCCESS ||
		length == (size_t)-1)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	char* name = (char*)malloc(length + 1);
	if (name == NULL)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	const int lookup = clm_id_get_name(key, name, length + 1, &length);
	const int result = lookup == CLM_BPA_GET_SUCCESS
		? clm_string_format_writer_append_quoted(writer, name, length)
		: CLM_FORMAT_SERIALIZE_FAILURE;
	free(name);
	return result;
}

clm_int clm_string_format_writer_append_key_raw(
	clm_string_format_writer* writer,
	const clm_id key) {
	size_t length;
	if (clm_id_get_name(key, NULL, 0, &length) != CLM_BPA_GET_SUCCESS ||
		length == (size_t)-1)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	char* name = (char*)malloc(length + 1);
	if (name == NULL)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	const int lookup = clm_id_get_name(key, name, length + 1, &length);
	const int result = lookup == CLM_BPA_GET_SUCCESS
		? clm_string_format_writer_append(writer, name, length)
		: CLM_FORMAT_SERIALIZE_FAILURE;
	free(name);
	return result;
}

clm_int clm_string_format_writer_append_leaf_text(
	clm_string_format_writer* writer,
	const clm_id value_id) {
	bool boolean_value;
	if (clm_tree_is_null(value_id))
		return clm_string_format_writer_append_cstr(writer, "null");
	if (clm_tree_is_boolean(value_id, &boolean_value))
		return clm_string_format_writer_append_cstr(writer,
			boolean_value ? "true" : "false");
	const clm_bpa_block_node* block = clm_bpa_find_block(value_id);
	if (block == NULL)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	if (block->type == CLM_BPA_TYPE_STRING)
		return clm_string_format_writer_append(writer,
			block->data != NULL ? (const char*)block->data : "",
			block->length);
	if (block->data == NULL)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	char number[128];
	int length = -1;
	switch (block->type) {
	case CLM_BPA_TYPE_SIGNED_BYTE: {
		clm_sbyte value; memcpy(&value, block->data, sizeof(value));
		length = snprintf(number, sizeof(number), "%d", (int)value); break;
	}
	case CLM_BPA_TYPE_BYTE: {
		clm_byte value; memcpy(&value, block->data, sizeof(value));
		length = snprintf(number, sizeof(number), "%u", (unsigned)value); break;
	}
	case CLM_BPA_TYPE_SHORT: {
		clm_short value; memcpy(&value, block->data, sizeof(value));
		length = snprintf(number, sizeof(number), "%d", (int)value); break;
	}
	case CLM_BPA_TYPE_UNSIGNED_SHORT: {
		clm_ushort value; memcpy(&value, block->data, sizeof(value));
		length = snprintf(number, sizeof(number), "%u", (unsigned)value); break;
	}
	case CLM_BPA_TYPE_INT: {
		clm_int value; memcpy(&value, block->data, sizeof(value));
		length = snprintf(number, sizeof(number), "%d", value); break;
	}
	case CLM_BPA_TYPE_UNSIGNED_INT: {
		clm_uint value; memcpy(&value, block->data, sizeof(value));
		length = snprintf(number, sizeof(number), "%u", value); break;
	}
	case CLM_BPA_TYPE_LONG: {
		clm_long value; memcpy(&value, block->data, sizeof(value));
		length = snprintf(number, sizeof(number), "%lld", (long long)value); break;
	}
	case CLM_BPA_TYPE_UNSIGNED_LONG: {
		clm_ulong value; memcpy(&value, block->data, sizeof(value));
		length = snprintf(number, sizeof(number), "%llu", (unsigned long long)value); break;
	}
	case CLM_BPA_TYPE_FLOAT: {
		clm_float value; memcpy(&value, block->data, sizeof(value));
		if (!isfinite(value)) return CLM_FORMAT_SERIALIZE_FAILURE;
		length = snprintf(number, sizeof(number), "%.9g", (double)value); break;
	}
	case CLM_BPA_TYPE_DOUBLE: {
		clm_double value; memcpy(&value, block->data, sizeof(value));
		if (!isfinite(value)) return CLM_FORMAT_SERIALIZE_FAILURE;
		length = snprintf(number, sizeof(number), "%.17g", value); break;
	}
	default:
		return CLM_FORMAT_SERIALIZE_FAILURE;
	}
	if (length < 0 || (size_t)length >= sizeof(number))
		return CLM_FORMAT_SERIALIZE_FAILURE;
	return clm_string_format_writer_append(writer, number, (size_t)length);
}

clm_int clm_string_format_parse_node_with_options(
	const char* input,
	const size_t input_length,
	const clm_string_format_lex_rules* rules,
	const clm_id destination_root_id,
	const clm_string_format_root_kind root_kind,
	const clm_string_format_tree_parser_fn parse_format,
    const bool auto_register_keys) {
	clm_string_format_parser parser;
    clm_string_format_key_registry registry = {0};
	size_t character_count;
	if (parse_format == NULL ||
		input == NULL ||
		clm_string_measure(input, input_length, CLM_STRING_ENCODING_UTF8,
						   &character_count) != CLM_BPA_SUCCESS ||
		clm_string_format_parser_init(&parser, input, input_length, rules) !=
			CLM_BPA_SUCCESS)
		return CLM_FORMAT_DESERIALIZE_FAILURE;
    parser.new_keys = auto_register_keys ? &registry : NULL;
	const int root_status = root_kind == CLM_STRING_FORMAT_ROOT_OBJECT
		? clm_tree_new(destination_root_id)
		: root_kind == CLM_STRING_FORMAT_ROOT_ARRAY
			? clm_tree_new_array(destination_root_id) : CLM_BPA_FAILURE;
	if (root_status != CLM_BPA_SUCCESS)
		return CLM_FORMAT_DESERIALIZE_FAILURE;
	if (parse_format(&parser, destination_root_id) !=
		CLM_FORMAT_DESERIALIZE_SUCCESS) {
		(void)clm_tree_delete(destination_root_id);
        clm_string_format_internal_release_keys(&registry, true);
		return CLM_FORMAT_DESERIALIZE_FAILURE;
	}
    clm_string_format_internal_release_keys(&registry, false);
	return CLM_FORMAT_DESERIALIZE_SUCCESS;
}

clm_int clm_string_format_parse_node(
    const char* input,
    const size_t input_length,
    const clm_string_format_lex_rules* rules,
    const clm_id destination_root_id,
    const clm_string_format_root_kind root_kind,
    const clm_string_format_tree_parser_fn parse_format) {
    return clm_string_format_parse_node_with_options(input, input_length,
        rules, destination_root_id, root_kind, parse_format, false);
}

clm_int clm_string_format_parse_tree(
	const char* input,
	const size_t input_length,
	const clm_string_format_lex_rules* rules,
	const clm_id destination_root_id,
	const clm_string_format_tree_parser_fn parse_format) {
	return clm_string_format_parse_node(input, input_length, rules,
		destination_root_id, CLM_STRING_FORMAT_ROOT_OBJECT, parse_format);
}

clm_int clm_string_format_loads_tree_with_options(
	const clm_id source_string_id,
	const clm_id destination_root_id,
	const clm_string_format_lex_rules* rules,
	const clm_string_format_root_kind root_kind,
	const clm_string_format_tree_parser_fn parse_format,
    const bool auto_register_keys) {
	const clm_bpa_block_node* source = clm_bpa_find_block(source_string_id);
	size_t characters;
	if (source == NULL || source->type != CLM_BPA_TYPE_STRING ||
		source->length > INT_MAX || (source->length != 0 && source->data == NULL) ||
		clm_string_measure(source->data != NULL ? (const char*)source->data : "",
						   source->length, CLM_STRING_ENCODING_UTF8,
						   &characters) != CLM_BPA_SUCCESS)
		return CLM_FORMAT_DESERIALIZE_FAILURE;
	return clm_string_format_parse_node_with_options(
		source->data != NULL ? (const char*)source->data : "",
		source->length, rules, destination_root_id, root_kind, parse_format, auto_register_keys);
}

clm_int clm_string_format_loads_tree(
    const clm_id source_string_id,
    const clm_id destination_root_id,
    const clm_string_format_lex_rules* rules,
    const clm_string_format_root_kind root_kind,
    const clm_string_format_tree_parser_fn parse_format) {
    return clm_string_format_loads_tree_with_options(source_string_id,
        destination_root_id, rules, root_kind, parse_format, false);
}

clm_int clm_string_format_serialize_tree(
	const clm_id root_object_id,
	const clm_id destination_string_id,
	const clm_string_format_tree_serializer_fn serialize_format) {
	const clm_bpa_block_node* destination =
		clm_bpa_find_block(destination_string_id);
	clm_id temporary_id;
	if (serialize_format == NULL ||
		(!clm_tree_is_object(root_object_id) && !clm_tree_is_array(root_object_id)) ||
		destination == NULL || destination->type != CLM_BPA_TYPE_STRING ||
		clm_string_format_temp_string_new(destination_string_id,
										  &temporary_id) != CLM_BPA_SUCCESS)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	clm_int result = CLM_FORMAT_SERIALIZE_FAILURE;
	if (serialize_format(root_object_id, temporary_id) ==
		CLM_FORMAT_SERIALIZE_SUCCESS) {
		const clm_bpa_block_node* temporary = clm_bpa_find_block(temporary_id);
		if (temporary != NULL && temporary->type == CLM_BPA_TYPE_STRING &&
			temporary->data != NULL &&
			clm_string_set(destination_string_id,
				(const char*)temporary->data) >= 0)
			result = CLM_FORMAT_SERIALIZE_SUCCESS;
	}
	(void)clm_bpa_delete(temporary_id);
	return result;
}

clm_int clm_string_format_internal_load_file_with_options(
	const clm_id path_id,
	const clm_id destination_root_id,
	const clm_string_format_lex_rules* rules,
	const clm_string_format_root_kind root_kind,
	const clm_string_format_tree_parser_fn parse_format,
    const bool auto_register_keys) {
	clm_id input_id;
	size_t input_length;
	if (clm_string_format_temp_string_new(destination_root_id, &input_id) !=
			CLM_BPA_SUCCESS)
		return CLM_FORMAT_DESERIALIZE_FAILURE;
	const clm_int read_status = clm_file_read_all_text(path_id,
		CLM_FILE_TEXT_ENCODING_UTF8, input_id, &input_length);
	clm_int result = CLM_FORMAT_DESERIALIZE_FAILURE;
	const clm_bpa_block_node* input = clm_bpa_find_block(input_id);
	if (read_status == CLM_FILE_OPERATION_SUCCESS && input != NULL &&
		input->type == CLM_BPA_TYPE_STRING && input->length == input_length)
		result = clm_string_format_parse_node_with_options(
			input->data != NULL ? (const char*)input->data : "",
			input_length, rules, destination_root_id, root_kind, parse_format,
            auto_register_keys);
	(void)clm_bpa_delete(input_id);
	return result;
}

clm_int clm_string_format_internal_load_file(
    const clm_id path_id,
    const clm_id destination_root_id,
    const clm_string_format_lex_rules* rules,
    const clm_string_format_root_kind root_kind,
    const clm_string_format_tree_parser_fn parse_format) {
    return clm_string_format_internal_load_file_with_options(path_id,
        destination_root_id, rules, root_kind, parse_format, false);
}

clm_int clm_string_format_internal_dump_file(
	const clm_id path_id,
	const clm_id root_id,
	const clm_string_format_tree_serializer_fn serialize_format) {
	clm_id output_id;
	if (clm_string_format_temp_string_new(root_id, &output_id) != CLM_BPA_SUCCESS)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	clm_int result = CLM_FORMAT_SERIALIZE_FAILURE;
	if (clm_string_format_serialize_tree(root_id, output_id, serialize_format) ==
		CLM_FORMAT_SERIALIZE_SUCCESS) {
		const clm_bpa_block_node* output = clm_bpa_find_block(output_id);
		if (output != NULL && output->type == CLM_BPA_TYPE_STRING)
			result = clm_file_write_all_text(path_id,
				CLM_FILE_TEXT_ENCODING_UTF8, output_id, output->length) ==
				CLM_FILE_OPERATION_SUCCESS
				? CLM_FORMAT_SERIALIZE_SUCCESS : CLM_FORMAT_SERIALIZE_FAILURE;
	}
	(void)clm_bpa_delete(output_id);
	return result;
}

clm_int clm_string_format_serialize_tree_to(
	const clm_id root_object_id,
	char* destination,
	const size_t capacity,
	const clm_string_format_tree_serializer_fn serialize_format) {
	clm_id temporary_id;
	if (destination == NULL || capacity == 0 || serialize_format == NULL ||
		(!clm_tree_is_object(root_object_id) && !clm_tree_is_array(root_object_id)) ||
		clm_string_format_temp_string_new(root_object_id, &temporary_id) !=
			CLM_BPA_SUCCESS)
		return CLM_FORMAT_SERIALIZE_FAILURE;
	clm_int result = CLM_FORMAT_SERIALIZE_FAILURE;
	if (serialize_format(root_object_id, temporary_id) ==
		CLM_FORMAT_SERIALIZE_SUCCESS) {
		const clm_bpa_block_node* temporary = clm_bpa_find_block(temporary_id);
		if (temporary != NULL && temporary->type == CLM_BPA_TYPE_STRING &&
			temporary->data != NULL && temporary->length < capacity) {
			memcpy(destination, temporary->data, (size_t)temporary->length + 1);
			result = CLM_FORMAT_SERIALIZE_SUCCESS;
		}
	}
	(void)clm_bpa_delete(temporary_id);
	return result;
}
