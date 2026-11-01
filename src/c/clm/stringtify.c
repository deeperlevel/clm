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

#include "stringtify.h"

#include "bpa.h"
#include "dictionary.h"
#include "id.h"
#include "storage.h"
#include "string_format.h"
#include "string_json.h"
#include "string_yaml.h"
#include "table.h"
#include "tree.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct clm_stringtify_writer {
	char *data;
	size_t length;
	size_t capacity;
	clm_stringtify_status status;
};

typedef struct clm_stringtify_context {
	const clm_stringtify_options *options;
	clm_id active_ids[CLM_TREE_MAX_DEPTH + 1];
} clm_stringtify_context;

typedef clm_stringtify_status (*clm_stringtify_render_fn)(
	clm_stringtify_context *context,
	clm_stringtify_writer *writer,
	clm_id id);

static clm_stringtify_status clm_stringtify_reserve(
	clm_stringtify_writer *writer, size_t additional)
{
	if (writer == NULL || writer->status != CLM_STRINGTIFY_SUCCESS)
		return writer != NULL ? writer->status : CLM_STRINGTIFY_INVALID_ARGUMENT;
	if (additional > SIZE_MAX - writer->length - 1) {
		writer->status = CLM_STRINGTIFY_NO_MEMORY;
		return writer->status;
	}
	const size_t required = writer->length + additional + 1;
	if (required <= writer->capacity)
		return CLM_STRINGTIFY_SUCCESS;
	size_t capacity = writer->capacity != 0 ? writer->capacity : 128;
	while (capacity < required) {
		if (capacity > SIZE_MAX / 2) {
			capacity = required;
			break;
		}
		capacity *= 2;
	}
	char *data = (char *)realloc(writer->data, capacity);
	if (data == NULL) {
		writer->status = CLM_STRINGTIFY_NO_MEMORY;
		return writer->status;
	}
	writer->data = data;
	writer->capacity = capacity;
	if (writer->length == 0)
		writer->data[0] = '\0';
	return CLM_STRINGTIFY_SUCCESS;
}

clm_stringtify_status clm_stringtify_writer_append(
	clm_stringtify_writer *writer, const char *text, size_t length)
{
	if (writer == NULL || (length != 0 && text == NULL))
		return CLM_STRINGTIFY_INVALID_ARGUMENT;
	const clm_stringtify_status status = clm_stringtify_reserve(writer, length);
	if (status != CLM_STRINGTIFY_SUCCESS)
		return status;
	if (length != 0)
		memcpy(writer->data + writer->length, text, length);
	writer->length += length;
	writer->data[writer->length] = '\0';
	return CLM_STRINGTIFY_SUCCESS;
}

clm_stringtify_status clm_stringtify_writer_append_cstr(
	clm_stringtify_writer *writer, const char *text)
{
	if (text == NULL)
		return CLM_STRINGTIFY_INVALID_ARGUMENT;
	return clm_stringtify_writer_append(writer, text, strlen(text));
}

static clm_stringtify_status clm_stringtify_append_format(
	clm_stringtify_writer *writer, const char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	va_list copy;
	va_copy(copy, arguments);
	const int length = vsnprintf(NULL, 0, format, copy);
	va_end(copy);
	if (length < 0) {
		va_end(arguments);
		return CLM_STRINGTIFY_FAILURE;
	}
	clm_stringtify_status status = clm_stringtify_reserve(writer, (size_t)length);
	if (status == CLM_STRINGTIFY_SUCCESS) {
		const int written = vsnprintf(writer->data + writer->length,
			writer->capacity - writer->length, format, arguments);
		if (written != length) {
			writer->status = CLM_STRINGTIFY_FAILURE;
			status = writer->status;
		} else {
			writer->length += (size_t)length;
		}
	}
	va_end(arguments);
	return status;
}

static clm_stringtify_status clm_stringtify_append_quoted(
	clm_stringtify_writer *writer, const char *text, size_t length)
{
	static const char hex[] = "0123456789abcdef";
	clm_stringtify_status status = clm_stringtify_writer_append_cstr(writer, "\"");
	if (status != CLM_STRINGTIFY_SUCCESS)
		return status;
	for (size_t index = 0; index < length; ++index) {
		const unsigned char value = (unsigned char)text[index];
		const char *escape = NULL;
		switch (value) {
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
			status = clm_stringtify_writer_append_cstr(writer, escape);
		} else if (value < 0x20) {
			const char encoded[] = {
				'\\', 'u', '0', '0', hex[value >> 4], hex[value & 0x0f]};
			status = clm_stringtify_writer_append(writer, encoded, sizeof(encoded));
		} else {
			status = clm_stringtify_writer_append(writer, (const char *)&text[index], 1);
		}
		if (status != CLM_STRINGTIFY_SUCCESS)
			return status;
	}
	return clm_stringtify_writer_append_cstr(writer, "\"");
}

static clm_stringtify_status clm_stringtify_indent(
	clm_stringtify_writer *writer, const clm_stringtify_options *options,
	size_t depth)
{
	if (options->style == CLM_STRINGTIFY_STYLE_COMPACT)
		return CLM_STRINGTIFY_SUCCESS;
	const size_t width = options->indent_width != 0 ? options->indent_width : 2;
	if (depth > SIZE_MAX / width)
		return CLM_STRINGTIFY_NO_MEMORY;
	for (size_t count = 0; count < depth * width; ++count) {
		const clm_stringtify_status status =
			clm_stringtify_writer_append_cstr(writer, " ");
		if (status != CLM_STRINGTIFY_SUCCESS)
			return status;
	}
	return CLM_STRINGTIFY_SUCCESS;
}

static clm_stringtify_status clm_stringtify_append_key(
	clm_stringtify_writer *writer, clm_id key)
{
	size_t length = 0;
	if (clm_id_get_name(key, NULL, 0, &length) == CLM_BPA_GET_SUCCESS &&
		length != SIZE_MAX) {
		char *name = (char *)malloc(length + 1);
		if (name == NULL)
			return CLM_STRINGTIFY_NO_MEMORY;
		const int status = clm_id_get_name(key, name, length + 1, &length);
		clm_stringtify_status result = status == CLM_BPA_GET_SUCCESS
			? clm_stringtify_append_quoted(writer, name, length)
			: CLM_STRINGTIFY_FAILURE;
		free(name);
		return result;
	}
	char numeric[32];
	const int written = snprintf(numeric, sizeof(numeric), "%llu",
		(unsigned long long)key);
	if (written < 0 || (size_t)written >= sizeof(numeric))
		return CLM_STRINGTIFY_FAILURE;
	return clm_stringtify_append_quoted(writer, numeric, (size_t)written);
}

static const char *clm_stringtify_type_name(clm_byte type)
{
	switch (type) {
	case CLM_BPA_TYPE_STRUCT: return "struct";
	case CLM_BPA_TYPE_SIGNED_BYTE: return "i8";
	case CLM_BPA_TYPE_BYTE: return "u8";
	case CLM_BPA_TYPE_SHORT: return "i16";
	case CLM_BPA_TYPE_UNSIGNED_SHORT: return "u16";
	case CLM_BPA_TYPE_INT: return "i32";
	case CLM_BPA_TYPE_UNSIGNED_INT: return "u32";
	case CLM_BPA_TYPE_LONG: return "i64";
	case CLM_BPA_TYPE_UNSIGNED_LONG: return "u64";
	case CLM_BPA_TYPE_FLOAT: return "f32";
	case CLM_BPA_TYPE_DOUBLE: return "f64";
	case CLM_BPA_TYPE_STRING: return "string";
	case CLM_BPA_TYPE_UUID: return "uuid";
	case CLM_BPA_TYPE_DATETIME: return "datetime";
	default: return "unknown";
	}
}

static clm_stringtify_status clm_stringtify_append_hex(
	clm_stringtify_writer *writer, const void *data, size_t size)
{
	static const char hex[] = "0123456789abcdef";
	const unsigned char *bytes = (const unsigned char *)data;
	clm_stringtify_status status = clm_stringtify_writer_append_cstr(writer, "\"0x");
	for (size_t index = 0; status == CLM_STRINGTIFY_SUCCESS && index < size; ++index) {
		char encoded[2] = {hex[bytes[index] >> 4], hex[bytes[index] & 0x0f]};
		status = clm_stringtify_writer_append(writer, encoded, sizeof(encoded));
	}
	if (status == CLM_STRINGTIFY_SUCCESS)
		status = clm_stringtify_writer_append_cstr(writer, "\"");
	return status;
}

static size_t clm_stringtify_visible_width(const char *text)
{
	size_t width = 0;
	for (size_t index = 0; text[index] != '\0';) {
		const unsigned char current = (unsigned char)text[index];
		if (current == 0x1b && text[index + 1] == '[') {
			index += 2;
			while (text[index] != '\0') {
				const unsigned char code = (unsigned char)text[index++];
				if (code >= 0x40 && code <= 0x7e)
					break;
			}
			continue;
		}
		if ((current & 0xc0) != 0x80)
			++width;
		++index;
	}
	return width;
}

static clm_stringtify_status clm_stringtify_render_leaf_default(
	clm_stringtify_context *context, clm_stringtify_writer *writer,
	clm_id node_id, clm_id field_id, clm_id value_id,
	clm_byte type, const void *data, size_t size)
{
	(void)node_id;
	(void)field_id;
	if (context->options->style == CLM_STRINGTIFY_STYLE_DETAILED) {
		clm_stringtify_status status = clm_stringtify_append_format(writer,
			"(%s) ", clm_stringtify_type_name(type));
		if (status != CLM_STRINGTIFY_SUCCESS)
			return status;
	}
	if (value_id != 0 && clm_tree_is_null(value_id))
		return clm_stringtify_writer_append_cstr(writer, "null");
	bool boolean_value = false;
	if (value_id != 0 && clm_tree_is_boolean(value_id, &boolean_value))
		return clm_stringtify_writer_append_cstr(writer,
			boolean_value ? "true" : "false");
	if (type == CLM_BPA_TYPE_STRING) {
		if (data == NULL && size != 0)
			return CLM_STRINGTIFY_FAILURE;
		return clm_stringtify_append_quoted(writer,
			data != NULL ? (const char *)data : "", size);
	}
	if (data == NULL || size == 0)
		return CLM_STRINGTIFY_FAILURE;
	char number[128];
	int length = -1;
	switch (type) {
	case CLM_BPA_TYPE_SIGNED_BYTE: {
		clm_sbyte value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		length = snprintf(number, sizeof(number), "%d", (int)value);
		break;
	}
	case CLM_BPA_TYPE_BYTE: {
		clm_byte value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		length = snprintf(number, sizeof(number), "%u", (unsigned)value);
		break;
	}
	case CLM_BPA_TYPE_SHORT: {
		clm_short value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		length = snprintf(number, sizeof(number), "%d", (int)value);
		break;
	}
	case CLM_BPA_TYPE_UNSIGNED_SHORT: {
		clm_ushort value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		length = snprintf(number, sizeof(number), "%u", (unsigned)value);
		break;
	}
	case CLM_BPA_TYPE_INT: {
		clm_int value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		length = snprintf(number, sizeof(number), "%d", value);
		break;
	}
	case CLM_BPA_TYPE_UNSIGNED_INT: {
		clm_uint value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		length = snprintf(number, sizeof(number), "%u", value);
		break;
	}
	case CLM_BPA_TYPE_LONG: {
		clm_long value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		length = snprintf(number, sizeof(number), "%lld", (long long)value);
		break;
	}
	case CLM_BPA_TYPE_UNSIGNED_LONG: {
		clm_ulong value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		length = snprintf(number, sizeof(number), "%llu",
			(unsigned long long)value);
		break;
	}
	case CLM_BPA_TYPE_FLOAT: {
		clm_float value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		if (!isfinite(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		length = snprintf(number, sizeof(number), "%.9g", (double)value);
		break;
	}
	case CLM_BPA_TYPE_DOUBLE: {
		clm_double value;
		if (size != sizeof(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		memcpy(&value, data, sizeof(value));
		if (!isfinite(value)) return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
		length = snprintf(number, sizeof(number), "%.17g", value);
		break;
	}
	case CLM_BPA_TYPE_STRUCT:
	case CLM_BPA_TYPE_UUID:
	case CLM_BPA_TYPE_DATETIME:
		return clm_stringtify_append_hex(writer, data, size);
	default:
		return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
	}
	if (length < 0 || (size_t)length >= sizeof(number))
		return CLM_STRINGTIFY_FAILURE;
	return clm_stringtify_writer_append(writer, number, (size_t)length);
}

static clm_stringtify_status clm_stringtify_render_leaf(
	clm_stringtify_context *context, clm_stringtify_writer *writer,
	clm_id node_id, clm_id field_id, clm_id value_id,
	clm_byte type, const void *data, size_t size)
{
	if (context->options->adapter != NULL) {
		const clm_stringtify_value value = {value_id, type, data, size};
		const size_t start = writer->length;
		const clm_stringtify_status status = context->options->adapter(
			node_id, field_id, &value, context->options, writer,
			context->options->adapter_user);
		if (status == CLM_STRINGTIFY_SUCCESS)
			return writer->status;
		if (status != CLM_STRINGTIFY_USE_DEFAULT)
			return status;
		if (writer->status != CLM_STRINGTIFY_SUCCESS)
			return writer->status;
		writer->length = start;
		if (writer->data != NULL)
			writer->data[start] = '\0';
	}
	return clm_stringtify_render_leaf_default(context, writer, node_id,
		field_id, value_id, type, data, size);
}

static clm_stringtify_status clm_stringtify_render_value(
	clm_stringtify_context *context, clm_stringtify_writer *writer,
	clm_id node_id, clm_id field_id, clm_id value_id, size_t depth);

static clm_stringtify_status clm_stringtify_render_block_root(
	clm_stringtify_context *context, clm_stringtify_writer *writer, clm_id id);

static clm_stringtify_status clm_stringtify_render_dictionary(
	clm_stringtify_context *context, clm_stringtify_writer *writer,
	clm_id id, size_t depth)
{
	if (depth >= CLM_TREE_MAX_DEPTH)
		return clm_stringtify_writer_append_cstr(writer, "\"...\"");
	for (size_t ancestor = 0; ancestor < depth; ++ancestor)
		if (context->active_ids[ancestor] == id)
			return CLM_STRINGTIFY_FAILURE;
	context->active_ids[depth] = id;

	const clm_int count = clm_dictionary_count(id);
	if (count < 0 || (size_t)count > SIZE_MAX / sizeof(clm_id))
		return CLM_STRINGTIFY_NOT_FOUND;
	clm_id *keys = count == 0 ? NULL :
		(clm_id *)malloc((size_t)count * sizeof(*keys));
	if (count != 0 && keys == NULL)
		return CLM_STRINGTIFY_NO_MEMORY;
	if (clm_dictionary_keys_to(id, keys, (size_t)count) != CLM_BPA_GET_SUCCESS) {
		free(keys);
		return CLM_STRINGTIFY_FAILURE;
	}

	clm_stringtify_status status = clm_stringtify_writer_append_cstr(writer, "{");
	for (clm_int index = 0; status == CLM_STRINGTIFY_SUCCESS && index < count;
		 ++index) {
		clm_id value_id = 0;
		if (clm_dictionary_get_id(id, keys[index], &value_id) != CLM_BPA_GET_SUCCESS) {
			status = CLM_STRINGTIFY_FAILURE;
			break;
		}
		if (index != 0)
			status = clm_stringtify_writer_append_cstr(writer, ",");
		if (status == CLM_STRINGTIFY_SUCCESS &&
			context->options->style != CLM_STRINGTIFY_STYLE_COMPACT) {
			status = clm_stringtify_writer_append_cstr(writer, "\n");
			if (status == CLM_STRINGTIFY_SUCCESS)
				status = clm_stringtify_indent(writer, context->options, depth + 1);
		}
		if (status == CLM_STRINGTIFY_SUCCESS)
			status = clm_stringtify_append_key(writer, keys[index]);
		if (status == CLM_STRINGTIFY_SUCCESS)
			status = clm_stringtify_writer_append_cstr(writer,
				context->options->style == CLM_STRINGTIFY_STYLE_COMPACT ? ":" : ": ");
		if (status == CLM_STRINGTIFY_SUCCESS)
			status = clm_stringtify_render_value(context, writer, id,
				keys[index], value_id, depth + 1);
	}
	free(keys);
	if (status == CLM_STRINGTIFY_SUCCESS && count != 0 &&
		context->options->style != CLM_STRINGTIFY_STYLE_COMPACT) {
		status = clm_stringtify_writer_append_cstr(writer, "\n");
		if (status == CLM_STRINGTIFY_SUCCESS)
			status = clm_stringtify_indent(writer, context->options, depth);
	}
	if (status == CLM_STRINGTIFY_SUCCESS)
		status = clm_stringtify_writer_append_cstr(writer, "}");
	return status;
}

static clm_stringtify_status clm_stringtify_render_tree(
	clm_stringtify_context *context, clm_stringtify_writer *writer,
	clm_id id, size_t depth)
{
	bool boolean_value = false;
	if (clm_tree_is_null(id) || clm_tree_is_boolean(id, &boolean_value)) {
		const clm_bpa_block_node *block = clm_bpa_find_block(id);
		if (block == NULL)
			return CLM_STRINGTIFY_NOT_FOUND;
		return clm_stringtify_render_leaf(context, writer, id, 0, id,
			block->type, block->data, block->block_size);
	}
	if (clm_tree_is_object(id))
		return clm_stringtify_render_dictionary(context, writer, id, depth);
	if (!clm_tree_is_array(id))
		return clm_stringtify_render_block_root(context, writer, id);
	if (depth >= CLM_TREE_MAX_DEPTH)
		return clm_stringtify_writer_append_cstr(writer, "\"...\"");
	for (size_t ancestor = 0; ancestor < depth; ++ancestor)
		if (context->active_ids[ancestor] == id)
			return CLM_STRINGTIFY_FAILURE;
	context->active_ids[depth] = id;

	const clm_int count = clm_tree_array_count(id);
	if (count < 0)
		return CLM_STRINGTIFY_NOT_FOUND;
	clm_stringtify_status status = clm_stringtify_writer_append_cstr(writer, "[");
	for (clm_int index = 0; status == CLM_STRINGTIFY_SUCCESS && index < count;
		 ++index) {
		clm_id value_id = 0;
		if (clm_tree_array_get_id(id, index, &value_id) != CLM_BPA_GET_SUCCESS) {
			status = CLM_STRINGTIFY_FAILURE;
			break;
		}
		if (index != 0)
			status = clm_stringtify_writer_append_cstr(writer, ",");
		if (status == CLM_STRINGTIFY_SUCCESS &&
			context->options->style != CLM_STRINGTIFY_STYLE_COMPACT) {
			status = clm_stringtify_writer_append_cstr(writer, "\n");
			if (status == CLM_STRINGTIFY_SUCCESS)
				status = clm_stringtify_indent(writer, context->options, depth + 1);
		}
		if (status == CLM_STRINGTIFY_SUCCESS)
			status = clm_stringtify_render_value(context, writer, id,
				(clm_id)index, value_id, depth + 1);
	}
	if (status == CLM_STRINGTIFY_SUCCESS && count != 0 &&
		context->options->style != CLM_STRINGTIFY_STYLE_COMPACT) {
		status = clm_stringtify_writer_append_cstr(writer, "\n");
		if (status == CLM_STRINGTIFY_SUCCESS)
			status = clm_stringtify_indent(writer, context->options, depth);
	}
	if (status == CLM_STRINGTIFY_SUCCESS)
		status = clm_stringtify_writer_append_cstr(writer, "]");
	return status;
}

static clm_stringtify_status clm_stringtify_render_value(
	clm_stringtify_context *context, clm_stringtify_writer *writer,
	clm_id node_id, clm_id field_id, clm_id value_id, size_t depth)
{
	if (context->options->max_depth != 0 &&
		depth >= context->options->max_depth)
		return clm_stringtify_writer_append_cstr(writer, "\"...\"");
	if (depth >= CLM_TREE_MAX_DEPTH)
		return clm_stringtify_writer_append_cstr(writer, "\"...\"");
	if (clm_tree_is_object(value_id) || clm_tree_is_array(value_id))
		return clm_stringtify_render_tree(context, writer, value_id, depth);

	clm_dictionary_any_metadata dictionary;
	if (clm_dictionary_internal_read(value_id, &dictionary) == CLM_BPA_SUCCESS) {
		return clm_stringtify_render_dictionary(context, writer, value_id, depth);
	}

	const clm_bpa_block_node *block = clm_bpa_find_block(value_id);
	if (block == NULL)
		return CLM_STRINGTIFY_NOT_FOUND;
	return clm_stringtify_render_leaf(context, writer, node_id, field_id,
		value_id, block->type, block->data,
		block->type == CLM_BPA_TYPE_STRING ? block->length : block->block_size);
}

void clm_stringtify_options_init(clm_stringtify_options *options)
{
	if (options != NULL) {
		memset(options, 0, sizeof(*options));
		options->style = CLM_STRINGTIFY_STYLE_COMPACT;
		options->indent_width = 2;
	}
}

static clm_stringtify_status clm_stringtify_run(
	clm_id id, const clm_stringtify_options *provided_options,
	char *dest, size_t dest_size, size_t *out_required_size,
	clm_stringtify_render_fn render)
{
	if ((dest == NULL && dest_size != 0) || render == NULL)
		return CLM_STRINGTIFY_INVALID_ARGUMENT;
	clm_stringtify_options defaults;
	if (provided_options == NULL) {
		clm_stringtify_options_init(&defaults);
		provided_options = &defaults;
	}
	if (provided_options->style < CLM_STRINGTIFY_STYLE_COMPACT ||
		provided_options->style > CLM_STRINGTIFY_STYLE_DETAILED)
		return CLM_STRINGTIFY_INVALID_ARGUMENT;

	clm_stringtify_writer writer = {0};
	writer.status = CLM_STRINGTIFY_SUCCESS;
	clm_stringtify_context context = {0};
	context.options = provided_options;
	clm_stringtify_status status;
	if (provided_options->interceptor != NULL) {
		status = provided_options->interceptor(id, provided_options, &writer,
			provided_options->interceptor_user);
		if (status == CLM_STRINGTIFY_USE_DEFAULT) {
			writer.length = 0;
			if (writer.data != NULL)
				writer.data[0] = '\0';
			status = render(&context, &writer, id);
		}
	} else {
		status = render(&context, &writer, id);
	}
	if (status == CLM_STRINGTIFY_SUCCESS && writer.status != CLM_STRINGTIFY_SUCCESS)
		status = writer.status;
	if (status != CLM_STRINGTIFY_SUCCESS) {
		free(writer.data);
		return status;
	}
	if (writer.length == SIZE_MAX) {
		free(writer.data);
		return CLM_STRINGTIFY_NO_MEMORY;
	}
	const size_t required = writer.length + 1;
	if (out_required_size != NULL)
		*out_required_size = required;
	if (dest == NULL && dest_size == 0) {
		free(writer.data);
		return CLM_STRINGTIFY_SUCCESS;
	}
	if (dest_size < required) {
		free(writer.data);
		return CLM_STRINGTIFY_BUFFER_TOO_SMALL;
	}
	memcpy(dest, writer.data != NULL ? writer.data : "", required);
	free(writer.data);
	return CLM_STRINGTIFY_SUCCESS;
}

static clm_stringtify_status clm_stringtify_render_block_root(
	clm_stringtify_context *context, clm_stringtify_writer *writer, clm_id id)
{
	const clm_bpa_block_node *block = clm_bpa_find_block(id);
	if (block == NULL)
		return CLM_STRINGTIFY_NOT_FOUND;
	if (block->data == NULL && block->block_size != 0 &&
		block->type != CLM_BPA_TYPE_STRING)
		return CLM_STRINGTIFY_FAILURE;
	return clm_stringtify_render_leaf(context, writer, id, 0, id,
		block->type, block->data, block->type == CLM_BPA_TYPE_STRING
			? block->length : block->block_size);
}

static clm_stringtify_status clm_stringtify_render_dictionary_root(
	clm_stringtify_context *context, clm_stringtify_writer *writer, clm_id id)
{
	if (clm_dictionary_internal_read(id, &(clm_dictionary_any_metadata){0}) !=
		CLM_BPA_SUCCESS)
		return CLM_STRINGTIFY_NOT_FOUND;
	return clm_stringtify_render_dictionary(context, writer, id, 0);
}

static clm_stringtify_status clm_stringtify_render_storage_root(
	clm_stringtify_context *context, clm_stringtify_writer *writer, clm_id id)
{
	clm_storage_info info;
	if (clm_storage_get_info(id, &info) != CLM_BPA_GET_SUCCESS)
		return CLM_STRINGTIFY_NOT_FOUND;
	if (info.element_size == 0 || info.columns == 0 || info.rows == 0 ||
		info.layers == 0)
		return CLM_STRINGTIFY_UNSUPPORTED_TYPE;
	if ((size_t)info.columns > SIZE_MAX / info.rows ||
		(size_t)info.columns * info.rows > SIZE_MAX / info.layers)
		return CLM_STRINGTIFY_FAILURE;
	unsigned char *element = (unsigned char *)malloc(info.element_size);
	if (element == NULL)
		return CLM_STRINGTIFY_NO_MEMORY;
	clm_stringtify_status result = CLM_STRINGTIFY_FAILURE;
	size_t flat_index = 0;
	result = clm_stringtify_writer_append_cstr(writer, "[");
	for (clm_uint layer = 0; result == CLM_STRINGTIFY_SUCCESS &&
		 layer < info.layers; ++layer) {
		if (layer != 0)
			result = clm_stringtify_writer_append_cstr(writer, ",");
		if (result == CLM_STRINGTIFY_SUCCESS &&
			context->options->style != CLM_STRINGTIFY_STYLE_COMPACT) {
			result = clm_stringtify_writer_append_cstr(writer, "\n");
			if (result == CLM_STRINGTIFY_SUCCESS)
				result = clm_stringtify_indent(writer, context->options, 1);
		}
		if (result == CLM_STRINGTIFY_SUCCESS)
			result = clm_stringtify_writer_append_cstr(writer, "[");
		for (clm_uint row = 0; result == CLM_STRINGTIFY_SUCCESS &&
			 row < info.rows; ++row) {
			if (row != 0)
				result = clm_stringtify_writer_append_cstr(writer, ",");
			if (result == CLM_STRINGTIFY_SUCCESS &&
				context->options->style != CLM_STRINGTIFY_STYLE_COMPACT) {
				result = clm_stringtify_writer_append_cstr(writer, "\n");
				if (result == CLM_STRINGTIFY_SUCCESS)
					result = clm_stringtify_indent(writer, context->options, 2);
			}
			if (result == CLM_STRINGTIFY_SUCCESS)
				result = clm_stringtify_writer_append_cstr(writer, "[");
			for (clm_uint column = 0; result == CLM_STRINGTIFY_SUCCESS &&
				 column < info.columns; ++column, ++flat_index) {
				if (column != 0)
					result = clm_stringtify_writer_append_cstr(writer, ",");
				if (result == CLM_STRINGTIFY_SUCCESS &&
					clm_storage_get_to(id, layer, row, column, element,
						info.element_size) != CLM_BPA_GET_SUCCESS)
					result = CLM_STRINGTIFY_FAILURE;
				if (result == CLM_STRINGTIFY_SUCCESS)
					result = clm_stringtify_render_leaf(context, writer, id,
						(clm_id)flat_index, 0, info.type,
						element, info.element_size);
			}
			if (result == CLM_STRINGTIFY_SUCCESS && info.columns != 0 &&
				context->options->style != CLM_STRINGTIFY_STYLE_COMPACT) {
				result = clm_stringtify_writer_append_cstr(writer, "\n");
				if (result == CLM_STRINGTIFY_SUCCESS)
					result = clm_stringtify_indent(writer, context->options, 2);
			}
			if (result == CLM_STRINGTIFY_SUCCESS)
				result = clm_stringtify_writer_append_cstr(writer, "]");
		}
		if (result == CLM_STRINGTIFY_SUCCESS && info.rows != 0 &&
			context->options->style != CLM_STRINGTIFY_STYLE_COMPACT) {
			result = clm_stringtify_writer_append_cstr(writer, "\n");
			if (result == CLM_STRINGTIFY_SUCCESS)
				result = clm_stringtify_indent(writer, context->options, 1);
		}
		if (result == CLM_STRINGTIFY_SUCCESS)
			result = clm_stringtify_writer_append_cstr(writer, "]");
	}
	if (result == CLM_STRINGTIFY_SUCCESS && info.layers != 0 &&
		context->options->style != CLM_STRINGTIFY_STYLE_COMPACT)
		result = clm_stringtify_writer_append_cstr(writer, "\n");
	if (result == CLM_STRINGTIFY_SUCCESS)
		result = clm_stringtify_writer_append_cstr(writer, "]");
	free(element);
	return result;
}

static clm_stringtify_status clm_stringtify_render_table_root(
	clm_stringtify_context *context, clm_stringtify_writer *writer, clm_id id)
{
	const clm_int columns = clm_table_column_count(id);
	const clm_int rows = clm_table_row_count(id);
	if (columns < 0 || rows < 0)
		return CLM_STRINGTIFY_NOT_FOUND;
	if ((size_t)columns > SIZE_MAX / sizeof(clm_id) ||
		(size_t)columns > SIZE_MAX / sizeof(char *))
		return CLM_STRINGTIFY_FAILURE;
	clm_id *column_ids = columns == 0 ? NULL :
		(clm_id *)malloc((size_t)columns * sizeof(*column_ids));
	char **names = columns == 0 ? NULL :
		(char **)calloc((size_t)columns, sizeof(*names));
	if (columns != 0 && (column_ids == NULL || names == NULL)) {
		free(column_ids);
		free(names);
		return CLM_STRINGTIFY_NO_MEMORY;
	}
	clm_stringtify_status result = CLM_STRINGTIFY_SUCCESS;
	for (clm_int column = 0; column < columns; ++column) {
		if (clm_table_column_id_at(id, column, &column_ids[column]) !=
			CLM_BPA_GET_SUCCESS) {
			result = CLM_STRINGTIFY_FAILURE;
			break;
		}
		size_t capacity = 64;
		while (capacity <= 1024 * 1024) {
			char *name = (char *)malloc(capacity);
			if (name == NULL) {
				result = CLM_STRINGTIFY_NO_MEMORY;
				break;
			}
			const clm_int name_status = clm_table_column_name_to(id, column,
				name, capacity);
			if (name_status >= 0) {
				names[column] = name;
				break;
			}
			free(name);
			capacity *= 2;
		}
		if (result != CLM_STRINGTIFY_SUCCESS)
			break;
		if (names[column] == NULL) {
			result = CLM_STRINGTIFY_FAILURE;
			break;
		}
	}

	const bool grid = context->options->table_borders ||
		context->options->align_columns;
	if (result == CLM_STRINGTIFY_SUCCESS && grid) {
		if ((size_t)rows != 0 && (size_t)columns >
			SIZE_MAX / (size_t)rows / sizeof(char *)) {
			result = CLM_STRINGTIFY_NO_MEMORY;
		} else {
			const size_t cell_count = (size_t)rows * (size_t)columns;
			char **cells = cell_count == 0 ? NULL :
				(char **)calloc(cell_count, sizeof(*cells));
			size_t *widths = columns == 0 ? NULL :
				(size_t *)calloc((size_t)columns, sizeof(*widths));
			if ((cell_count != 0 && cells == NULL) ||
				(columns != 0 && widths == NULL)) {
				result = CLM_STRINGTIFY_NO_MEMORY;
			}
			for (clm_int column = 0; result == CLM_STRINGTIFY_SUCCESS &&
				 column < columns; ++column) {
				widths[column] = strlen(names[column]);
			}
			for (clm_int row = 0; result == CLM_STRINGTIFY_SUCCESS &&
				 row < rows; ++row) {
				for (clm_int column = 0; column < columns; ++column) {
					const size_t slot = (size_t)row * (size_t)columns +
						(size_t)column;
					clm_stringtify_writer cell = {0};
					cell.status = CLM_STRINGTIFY_SUCCESS;
					if (clm_table_cell_contains(id, row, column_ids[column])) {
						clm_id row_dictionary = 0;
						clm_id value_id = 0;
						if (clm_table_internal_cell(id, row, column_ids[column],
								&row_dictionary) != CLM_BPA_SUCCESS ||
							clm_dictionary_get_id(row_dictionary,
								column_ids[column], &value_id) != CLM_BPA_GET_SUCCESS) {
							result = CLM_STRINGTIFY_FAILURE;
							free(cell.data);
							break;
						}
						result = clm_stringtify_render_value(context, &cell, id,
							column_ids[column], value_id, 0);
					}
					if (result == CLM_STRINGTIFY_SUCCESS) {
						cells[slot] = cell.data != NULL ? cell.data :
							(char *)calloc(1, 1);
						if (cells[slot] == NULL)
							result = CLM_STRINGTIFY_NO_MEMORY;
						else if (clm_stringtify_visible_width(cells[slot]) >
							widths[column])
							widths[column] =
								clm_stringtify_visible_width(cells[slot]);
					} else {
						free(cell.data);
					}
					if (result != CLM_STRINGTIFY_SUCCESS)
						break;
				}
			}
			if (result == CLM_STRINGTIFY_SUCCESS) {
				const bool borders = context->options->table_borders;
				for (clm_int row = -1; result == CLM_STRINGTIFY_SUCCESS &&
					 row < rows; ++row) {
					if (borders && (row == -1 || row == 0)) {
						for (clm_int column = 0; column < columns; ++column) {
							if (column != 0)
								result = clm_stringtify_writer_append_cstr(writer, "+");
							if (result == CLM_STRINGTIFY_SUCCESS)
								result = clm_stringtify_writer_append_cstr(writer, "-");
							for (size_t pad = 0; result == CLM_STRINGTIFY_SUCCESS &&
								 pad < widths[column]; ++pad)
								result = clm_stringtify_writer_append_cstr(writer, "-");
							if (result == CLM_STRINGTIFY_SUCCESS)
								result = clm_stringtify_writer_append_cstr(writer, "-");
						}
						if (result == CLM_STRINGTIFY_SUCCESS && columns != 0)
							result = clm_stringtify_writer_append_cstr(writer, "+");
						if (result == CLM_STRINGTIFY_SUCCESS)
							result = clm_stringtify_writer_append_cstr(writer, "\n");
					}
					if (borders)
						result = clm_stringtify_writer_append_cstr(writer, "|");
					for (clm_int column = 0; result == CLM_STRINGTIFY_SUCCESS &&
						 column < columns; ++column) {
						if (column != 0)
							result = clm_stringtify_writer_append_cstr(writer,
								borders ? "|" : "  ");
						const char *text = row == -1 ? names[column] :
							cells[(size_t)row * (size_t)columns + (size_t)column];
						const size_t length = strlen(text);
						const size_t visible = clm_stringtify_visible_width(text);
						if (result == CLM_STRINGTIFY_SUCCESS)
							result = clm_stringtify_writer_append_cstr(writer, " ");
						if (result == CLM_STRINGTIFY_SUCCESS)
							result = clm_stringtify_writer_append(writer, text, length);
						const size_t padding = widths[column] > visible
							? widths[column] - visible : 0;
						for (size_t pad = 0; result == CLM_STRINGTIFY_SUCCESS &&
							 pad < padding; ++pad)
							result = clm_stringtify_writer_append_cstr(writer, " ");
						if (result == CLM_STRINGTIFY_SUCCESS)
							result = clm_stringtify_writer_append_cstr(writer, " ");
					}
					if (result == CLM_STRINGTIFY_SUCCESS && borders && columns != 0)
						result = clm_stringtify_writer_append_cstr(writer, "|");
					if (result == CLM_STRINGTIFY_SUCCESS)
						result = clm_stringtify_writer_append_cstr(writer, "\n");
				}
				if (result == CLM_STRINGTIFY_SUCCESS && borders) {
					for (clm_int column = 0; column < columns; ++column) {
						if (column != 0)
							result = clm_stringtify_writer_append_cstr(writer, "+");
						if (result == CLM_STRINGTIFY_SUCCESS)
							result = clm_stringtify_writer_append_cstr(writer, "-");
						for (size_t pad = 0; result == CLM_STRINGTIFY_SUCCESS &&
							 pad < widths[column]; ++pad)
							result = clm_stringtify_writer_append_cstr(writer, "-");
						if (result == CLM_STRINGTIFY_SUCCESS)
							result = clm_stringtify_writer_append_cstr(writer, "-");
					}
					if (result == CLM_STRINGTIFY_SUCCESS && columns != 0)
						result = clm_stringtify_writer_append_cstr(writer, "+");
					if (result == CLM_STRINGTIFY_SUCCESS)
						result = clm_stringtify_writer_append_cstr(writer, "\n");
				}
			}
			for (size_t slot = 0; slot < (size_t)rows * (size_t)columns; ++slot)
				free(cells != NULL ? cells[slot] : NULL);
			free(cells);
			free(widths);
		}
	} else if (result == CLM_STRINGTIFY_SUCCESS) {
		result = clm_stringtify_writer_append_cstr(writer, "{\"columns\":[");
		for (clm_int column = 0; result == CLM_STRINGTIFY_SUCCESS &&
			 column < columns; ++column) {
			if (column != 0)
				result = clm_stringtify_writer_append_cstr(writer, ",");
			if (result == CLM_STRINGTIFY_SUCCESS)
				result = clm_stringtify_append_quoted(writer, names[column],
					strlen(names[column]));
		}
		if (result == CLM_STRINGTIFY_SUCCESS)
			result = clm_stringtify_writer_append_cstr(writer, "],\"rows\":[");
		for (clm_int row = 0; result == CLM_STRINGTIFY_SUCCESS && row < rows; ++row) {
			if (row != 0)
				result = clm_stringtify_writer_append_cstr(writer, ",");
			if (result == CLM_STRINGTIFY_SUCCESS)
				result = clm_stringtify_writer_append_cstr(writer, "[");
			for (clm_int column = 0; result == CLM_STRINGTIFY_SUCCESS &&
				 column < columns; ++column) {
				if (column != 0)
					result = clm_stringtify_writer_append_cstr(writer, ",");
				if (!clm_table_cell_contains(id, row, column_ids[column])) {
					if (result == CLM_STRINGTIFY_SUCCESS)
						result = clm_stringtify_writer_append_cstr(writer, "null");
					continue;
				}
				clm_id row_dictionary = 0;
				clm_id value_id = 0;
				if (clm_table_internal_cell(id, row, column_ids[column],
						&row_dictionary) != CLM_BPA_SUCCESS ||
					clm_dictionary_get_id(row_dictionary, column_ids[column],
						&value_id) != CLM_BPA_GET_SUCCESS) {
					result = CLM_STRINGTIFY_FAILURE;
					break;
				}
				result = clm_stringtify_render_value(context, writer, id,
					column_ids[column], value_id, 0);
			}
			if (result == CLM_STRINGTIFY_SUCCESS)
				result = clm_stringtify_writer_append_cstr(writer, "]");
		}
		if (result == CLM_STRINGTIFY_SUCCESS)
			result = clm_stringtify_writer_append_cstr(writer, "]}");
	}
	for (clm_int column = 0; column < columns; ++column)
		free(names != NULL ? names[column] : NULL);
	free(names);
	free(column_ids);
	return result;
}

static clm_stringtify_status clm_stringtify_render_tree_root(
	clm_stringtify_context *context, clm_stringtify_writer *writer, clm_id id)
{
	if (clm_tree_is_null(id) || clm_tree_is_boolean(id, &(bool){false}) ||
		clm_tree_is_object(id) || clm_tree_is_array(id))
		return clm_stringtify_render_tree(context, writer, id, 0);
	return CLM_STRINGTIFY_NOT_FOUND;
}

#define CLM_STRINGTIFY_DEFINE_ENTRY_POINTS(kind) \
clm_stringtify_status clm_stringtify_##kind##_to_string( \
	clm_id id, char *dest, size_t dest_size, size_t *out_required_size) \
{ \
	return clm_stringtify_##kind##_to_string_with_options( \
		id, NULL, dest, dest_size, out_required_size); \
} \
clm_stringtify_status clm_stringtify_##kind##_to_string_with_options( \
	clm_id id, const clm_stringtify_options *options, char *dest, \
	size_t dest_size, size_t *out_required_size) \
{ \
	return clm_stringtify_run(id, options, dest, dest_size, \
		out_required_size, clm_stringtify_render_##kind##_root); \
}

CLM_STRINGTIFY_DEFINE_ENTRY_POINTS(block)
CLM_STRINGTIFY_DEFINE_ENTRY_POINTS(dictionary)
CLM_STRINGTIFY_DEFINE_ENTRY_POINTS(storage)
CLM_STRINGTIFY_DEFINE_ENTRY_POINTS(table)
CLM_STRINGTIFY_DEFINE_ENTRY_POINTS(tree)

#undef CLM_STRINGTIFY_DEFINE_ENTRY_POINTS

static clm_stringtify_status clm_stringtify_json_newline(
	clm_stringtify_writer *writer, size_t depth)
{
	clm_stringtify_status status = clm_stringtify_writer_append_cstr(writer, "\n");
	for (size_t level = 0; level < depth && status == CLM_STRINGTIFY_SUCCESS; ++level)
		status = clm_stringtify_writer_append_cstr(writer, "  ");
	return status;
}

static clm_stringtify_status clm_stringtify_append_pretty_json(
	clm_stringtify_writer *writer, const char *text, size_t length)
{
	const clm_string_format_lex_rules rules = {
		.delimiters = "{}[],:", .quote_characters = "\"", .escape_character = '\\'
	};
	clm_string_format_lexer lexer;
	clm_string_format_token token;
	if (clm_string_format_lexer_init(&lexer, text, length, &rules) != CLM_BPA_SUCCESS ||
		clm_string_format_lexer_next(&lexer, &token) != CLM_BPA_SUCCESS)
		return CLM_STRINGTIFY_FAILURE;
	size_t depth = 0;
	bool previous_open = false;
	while (token.kind != CLM_STRING_FORMAT_TOKEN_END) {
		clm_string_format_token next;
		if (clm_string_format_lexer_next(&lexer, &next) != CLM_BPA_SUCCESS)
			return CLM_STRINGTIFY_FAILURE;
		const char delimiter = token.kind == CLM_STRING_FORMAT_TOKEN_DELIMITER
			? text[token.offset] : '\0';
		const bool opening = delimiter == '{' || delimiter == '[';
		const bool closing = delimiter == '}' || delimiter == ']';
		clm_stringtify_status status = CLM_STRINGTIFY_SUCCESS;
		if (closing) {
			if (depth == 0)
				return CLM_STRINGTIFY_FAILURE;
			--depth;
			if (!previous_open)
				status = clm_stringtify_json_newline(writer, depth);
		}
		if (status == CLM_STRINGTIFY_SUCCESS)
			status = clm_stringtify_writer_append(writer,
				text + token.offset, token.length);
		if (opening) {
			++depth;
			const bool empty = next.kind == CLM_STRING_FORMAT_TOKEN_DELIMITER &&
				(text[next.offset] == '}' || text[next.offset] == ']');
			if (!empty && status == CLM_STRINGTIFY_SUCCESS)
				status = clm_stringtify_json_newline(writer, depth);
		} else if (delimiter == ',' && status == CLM_STRINGTIFY_SUCCESS) {
			status = clm_stringtify_json_newline(writer, depth);
		} else if (delimiter == ':' && status == CLM_STRINGTIFY_SUCCESS) {
			status = clm_stringtify_writer_append_cstr(writer, " ");
		}
		if (status != CLM_STRINGTIFY_SUCCESS)
			return status;
		previous_open = opening;
		token = next;
	}
	return depth == 0 ? CLM_STRINGTIFY_SUCCESS : CLM_STRINGTIFY_FAILURE;
}

static clm_stringtify_status clm_stringtify_tree_format_interceptor(
	clm_id id, clm_stringtify_writer *writer,
	clm_string_format_tree_serializer_fn serialize, bool pretty_json)
{
	if (!clm_tree_is_object(id) && !clm_tree_is_array(id))
		return CLM_STRINGTIFY_INVALID_ARGUMENT;
	clm_id temporary_id;
	if (clm_string_format_temp_string_new(id, &temporary_id) != CLM_BPA_SUCCESS)
		return CLM_STRINGTIFY_NO_MEMORY;
	clm_stringtify_status status = CLM_STRINGTIFY_FAILURE;
	if (serialize(id, temporary_id) == CLM_FORMAT_SERIALIZE_SUCCESS) {
		const clm_bpa_block_node *text = clm_bpa_find_block(temporary_id);
		if (text != NULL)
			status = pretty_json
				? clm_stringtify_append_pretty_json(writer,
					(const char *)text->data, text->length)
				: clm_stringtify_writer_append(writer,
					(const char *)text->data, text->length);
	}
	(void)clm_bpa_delete(temporary_id);
	return status;
}

clm_stringtify_status clm_stringtify_tree_yaml_interceptor(
	clm_id id, const clm_stringtify_options *options,
	clm_stringtify_writer *writer, void *user)
{
	(void)options;
	(void)user;
	return clm_stringtify_tree_format_interceptor(id, writer, clm_string_yaml_dumps, false);
}

clm_stringtify_status clm_stringtify_tree_json_interceptor(
	clm_id id, const clm_stringtify_options *options,
	clm_stringtify_writer *writer, void *user)
{
	(void)options;
	(void)user;
	return clm_stringtify_tree_format_interceptor(id, writer, clm_string_json_dumps, true);
}
