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



clm_int clm_string_toml_internal_value(
    const char* text, size_t offset, size_t length,
    const clm_id parent_id, const bool parent_is_array, const clm_id key,
    const size_t depth);

clm_int clm_string_toml_internal_parse_array(
    const char* text, const size_t offset, const size_t length,
    const clm_id parent_id, const bool parent_is_array, const clm_id key,
    const size_t depth);

clm_int clm_string_toml_internal_value(
    const char* text, size_t offset, size_t length,
    const clm_id parent_id, const bool parent_is_array, const clm_id key,
    const size_t depth);

clm_int clm_string_toml_internal_parse(
    clm_string_format_parser* parser,
    const clm_id root_id);

clm_int clm_string_toml_internal_serialize_value(
    const clm_id value_id,
    clm_string_format_writer* writer,
    const size_t depth);

clm_int clm_string_toml_internal_serialize_object(
    const clm_id object_id,
    clm_string_format_writer* writer,
    clm_string_format_writer* path,
    const bool root,
    const size_t depth);

clm_int clm_string_toml_internal_serialize(
    const clm_id root_id, const clm_id destination_id);

clm_int clm_string_toml_internal_run(
    const char* source, const size_t length, const clm_id destination_id);

/** Parses an existing CLM string block into a new owned TOML tree. 
 * @param source_id CLM ID identifying the source.
 * @param destination_tree_id CLM ID identifying the destination tree.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_toml_loads(
    const clm_id source_id, const clm_id destination_tree_id);

/** Reads UTF-8 TOML text from a filename registered by CLM file IDs. 
 * @param filename_id CLM ID identifying the filename.
 * @param destination_tree_id CLM ID identifying the destination tree.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_toml_load(
    const clm_id filename_id, const clm_id destination_tree_id);

/** Serializes a TOML object into an existing CLM string block. 
 * @param object_id CLM ID identifying the object.
 * @param destination_id CLM ID identifying the destination.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_toml_dumps(
    const clm_id object_id, const clm_id destination_id);

/** Writes a TOML tree to a filename registered by CLM file IDs. 
 * @param object_id CLM ID identifying the object.
 * @param filename_id CLM ID identifying the filename.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_toml_dump(
    const clm_id object_id, const clm_id filename_id);


/** Opt-in TOML import with automatic clm_id mapping (rollback on error). 
 * @param source_id CLM ID identifying the source.
 * @param destination_tree_id CLM ID identifying the destination tree.
 * @return Operation status or value described in the summary above.
 */
clm_int clm_string_toml_loads_auto_ids(
    const clm_id source_id, const clm_id destination_tree_id);

clm_int clm_string_toml_load_auto_ids(
    const clm_id filename_id, const clm_id destination_tree_id);
