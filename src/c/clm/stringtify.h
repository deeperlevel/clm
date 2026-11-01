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

#include "base.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum clm_stringtify_status {
    CLM_STRINGTIFY_SUCCESS = 0,
    CLM_STRINGTIFY_USE_DEFAULT = 1,
    CLM_STRINGTIFY_FAILURE = -1,
    CLM_STRINGTIFY_INVALID_ARGUMENT = -2,
    CLM_STRINGTIFY_NOT_FOUND = -3,
    CLM_STRINGTIFY_UNSUPPORTED_TYPE = -4,
    CLM_STRINGTIFY_BUFFER_TOO_SMALL = -5,
    CLM_STRINGTIFY_NO_MEMORY = -6
} clm_stringtify_status;

typedef enum clm_stringtify_style {
    CLM_STRINGTIFY_STYLE_COMPACT = 0,
    CLM_STRINGTIFY_STYLE_PRETTY = 1,
    CLM_STRINGTIFY_STYLE_DETAILED = 2
} clm_stringtify_style;

typedef struct clm_stringtify_writer clm_stringtify_writer;
typedef struct clm_stringtify_options clm_stringtify_options;

/** Borrowed value view passed to adapters; data remains valid only during the
 * callback. id is zero for inline storage elements that have no BPA block.
 */
typedef struct clm_stringtify_value {
    clm_id id;
    clm_byte type;
    const void *data;
    size_t size;
} clm_stringtify_value;

/** Appends UTF-8 bytes to the callback's current result. */
clm_stringtify_status clm_stringtify_writer_append(
    clm_stringtify_writer *writer, const char *text, size_t length);

/** Appends a NUL-terminated UTF-8 string to the callback's current result. */
clm_stringtify_status clm_stringtify_writer_append_cstr(
    clm_stringtify_writer *writer, const char *text);

/** Customizes a leaf value; return USE_DEFAULT for the core formatter.
 * node_id is the containing object, field_id is its key/column (zero when not
 * applicable), and value_id is the value being formatted. Return SUCCESS after
 * appending the complete leaf representation, or an error to abort conversion.
 */
typedef clm_stringtify_status (*clm_stringtify_adapter_fn)(
    clm_id node_id, clm_id field_id, const clm_stringtify_value *value,
    const clm_stringtify_options *options,
    clm_stringtify_writer *writer, void *user);

/** Replaces conversion of the entire requested object. The callback reads
 * object_id as needed and appends the complete representation to writer.
 */
typedef clm_stringtify_status (*clm_stringtify_interceptor_fn)(
    clm_id object_id, const clm_stringtify_options *options,
    clm_stringtify_writer *writer, void *user);

/** Per-call presentation and customization settings.
 * A NULL adapter/interceptor selects default behavior. An interceptor replaces
 * full-object traversal; otherwise the adapter may customize individual leaves.
 * max_depth == 0 means no depth limit. ansi_colors is a hint for custom callbacks;
 * the core formatter emits no terminal color codes by default.
 * DETAILED output annotates scalar values with their BPA type.
 */
struct clm_stringtify_options {
    clm_stringtify_style style;
    size_t indent_width;
    size_t max_depth;
    bool table_borders;
    bool align_columns;
    bool ansi_colors;
    clm_stringtify_adapter_fn adapter;
    clm_stringtify_interceptor_fn interceptor;
    void *adapter_user;
    void *interceptor_user;
};

/** Interceptors that render a tree object/array root as YAML or two-space
 * indented JSON through clm_string_yaml/clm_string_json. Other roots return
 * INVALID_ARGUMENT. The tree is not modified; options and user are unused.
 */
clm_stringtify_status clm_stringtify_tree_yaml_interceptor(
    clm_id id, const clm_stringtify_options *options,
    clm_stringtify_writer *writer, void *user);
clm_stringtify_status clm_stringtify_tree_json_interceptor(
    clm_id id, const clm_stringtify_options *options,
    clm_stringtify_writer *writer, void *user);

/** Initializes options to compact, uncolored output with no depth limit. */
void clm_stringtify_options_init(clm_stringtify_options *options);

/** Stringifies a BPA block into a caller-owned UTF-8 buffer.
 * dest_size includes the terminating NUL. A NULL dest with size zero measures
 * only; out_required_size is optional and includes the NUL. BUFFER_TOO_SMALL
 * leaves dest unchanged.
 */
clm_stringtify_status clm_stringtify_block_to_string(
    clm_id id, char *dest, size_t dest_size, size_t *out_required_size);
clm_stringtify_status clm_stringtify_block_to_string_with_options(
    clm_id id, const clm_stringtify_options *options, char *dest,
    size_t dest_size, size_t *out_required_size);

/** Stringifies a heterogeneous CLM dictionary into a UTF-8 buffer. */
clm_stringtify_status clm_stringtify_dictionary_to_string(
    clm_id id, char *dest, size_t dest_size, size_t *out_required_size);
clm_stringtify_status clm_stringtify_dictionary_to_string_with_options(
    clm_id id, const clm_stringtify_options *options, char *dest,
    size_t dest_size, size_t *out_required_size);

/** Stringifies a CLM storage value into a UTF-8 buffer. */
clm_stringtify_status clm_stringtify_storage_to_string(
    clm_id id, char *dest, size_t dest_size, size_t *out_required_size);
clm_stringtify_status clm_stringtify_storage_to_string_with_options(
    clm_id id, const clm_stringtify_options *options, char *dest,
    size_t dest_size, size_t *out_required_size);

/** Stringifies a CLM table into a UTF-8 buffer. */
clm_stringtify_status clm_stringtify_table_to_string(
    clm_id id, char *dest, size_t dest_size, size_t *out_required_size);
clm_stringtify_status clm_stringtify_table_to_string_with_options(
    clm_id id, const clm_stringtify_options *options, char *dest,
    size_t dest_size, size_t *out_required_size);

/** Stringifies a CLM tree into a UTF-8 buffer. */
clm_stringtify_status clm_stringtify_tree_to_string(
    clm_id id, char *dest, size_t dest_size, size_t *out_required_size);
clm_stringtify_status clm_stringtify_tree_to_string_with_options(
    clm_id id, const clm_stringtify_options *options, char *dest,
    size_t dest_size, size_t *out_required_size);