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
#include "string_format.h"
#include "file.h"



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

bool clm_string_yaml_internal_sequence_line(
    const clm_string_yaml_document* document,
    const size_t index);

clm_int clm_string_yaml_internal_prepare_lines(
    const char* text,
    const size_t length,
    clm_string_yaml_document* document);

clm_int clm_string_yaml_internal_scalar(
    const clm_string_yaml_document* document,
    const size_t offset,
    const size_t length,
    const clm_id parent_id,
    const bool parent_is_array,
    const clm_id key);

clm_int clm_string_yaml_internal_parse_block(
    clm_string_yaml_document* document,
    const clm_id container_id,
    const bool is_array,
    const size_t indentation,
    const size_t depth);

size_t clm_string_yaml_internal_colon(
    const char* text,
    const size_t offset,
    const size_t length);

clm_int clm_string_yaml_internal_key(
    clm_string_yaml_document* document,
    size_t offset,
    size_t length,
    clm_id* key);

clm_int clm_string_yaml_internal_map_entry(
    clm_string_yaml_document* document,
    const clm_id object_id,
    const size_t key_offset,
    const size_t key_length,
    const size_t value_offset,
    const size_t value_length,
    const size_t logical_indentation,
    const size_t depth);

clm_int clm_string_yaml_internal_parse_block(
    clm_string_yaml_document* document,
    const clm_id container_id,
    const bool is_array,
    const size_t indentation,
    const size_t depth);

clm_int clm_string_yaml_internal_prepare(
    const char* input,
    const size_t input_length,
    clm_string_yaml_document* document);

clm_int clm_string_yaml_internal_detect_root(
    const char* input,
    const size_t input_length,
    clm_string_format_root_kind* kind);

clm_int clm_string_yaml_internal_parse(
    clm_string_format_parser* parser,
    const clm_id root_id);

clm_int clm_string_yaml_internal_emit_indent(
    clm_string_format_writer* writer,
    const size_t indentation);

clm_int clm_string_yaml_internal_emit_scalar(
    const clm_id value_id,
    clm_string_format_writer* writer);

clm_int clm_string_yaml_internal_emit_node(
    const clm_id node_id,
    clm_string_format_writer* writer,
    const size_t indentation,
    const size_t depth);

clm_int clm_string_yaml_internal_serialize(
    const clm_id root_id, const clm_id destination_id);

clm_int clm_string_yaml_internal_parse_text(
    const char* source, const size_t length, const clm_id destination_id);

/** Parses an existing CLM string block into a new owned YAML tree. 
 * @param source_id CLM ID identifying the source.
 * @param destination_tree_id CLM ID identifying the destination tree.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_yaml_loads(
    const clm_id source_id, const clm_id destination_tree_id);

clm_int clm_string_yaml_internal_load_file(
    const clm_id filename_id,
    const clm_id destination_tree_id);

/** Reads YAML text from a filename registered by CLM file IDs. 
 * @param filename_id CLM ID identifying the filename.
 * @param destination_tree_id CLM ID identifying the destination tree.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_yaml_load(
    const clm_id filename_id, const clm_id destination_tree_id);

/** Serializes a YAML object or array into an existing CLM string block. 
 * @param object_id CLM ID identifying the object.
 * @param destination_id CLM ID identifying the destination.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_yaml_dumps(
    const clm_id object_id, const clm_id destination_id);

/** Writes a YAML tree to a filename registered by CLM file IDs. 
 * @param object_id CLM ID identifying the object.
 * @param filename_id CLM ID identifying the filename.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_yaml_dump(
    const clm_id object_id, const clm_id filename_id);


/** Opt-in YAML import with automatic clm_id mapping (rollback on error). 
 * @param source_id CLM ID identifying the source.
 * @param destination_tree_id CLM ID identifying the destination tree.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_yaml_loads_auto_ids(
    const clm_id source_id, const clm_id destination_tree_id);

clm_int clm_string_yaml_load_auto_ids(
    const clm_id filename_id, const clm_id destination_tree_id);
