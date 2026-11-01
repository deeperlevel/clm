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
#include <string.h>

/* --------------------------------------------------------------------------
 * clm_clp — Command-Line Parser
 *
 * Parses caller-supplied argv/tokens or a simple command-line string into a
 * BPA-backed typed dictionary of UTF-8 string values.
 *
 * - Parse an already supplied argc/argv array OR a command-line string.
 *   clm_clp never reads process-global command-line arguments.
 * - On SUCCESS, create a BPA-backed clm_typed_dictionary whose value type is
 *   CLM_BPA_TYPE_STRING (using clm_typed_dictionary_new).
 * - The caller supplies an unused output_dictionary_id. The parser creates
 *   the dictionary at that ID; the caller owns it and ultimately calls
 *   clm_typed_dictionary_delete(output_dictionary_id).
 * - Dictionary keys are clm_id values. Option names are automatically
 *   registered in clm_id when unknown. Successful registrations remain;
 *   registrations made by a failed parse are rolled back.
 * - All dictionary values are UTF-8, NUL-terminated strings. No type inference
 *   or conversion to numbers/bools occurs within clm_clp.
 *
 * Supported syntax:
 *
 *   --host=127.0.0.1    => host    : "127.0.0.1"
 *   --port 8080         => port    : "8080"
 *   -p=8080            => p       : "8080"
 *   -p 8080            => p       : "8080"
 *   --verbose           => verbose : "true"
 *   output=file.txt     => output  : "file.txt"
 *
 * Repeated keys use the last value. An explicit empty value (--name=) is
 * allowed. --no-name is treated as an ordinary key named "no-name" in v1.
 * Short flags are not grouped (-abc names the key "abc").
 *
 * Positional arguments (tokens with no key=value or option introducer) are
 * outside the initial contract and should produce INVALID_SYNTAX rather than
 * being silently discarded. Shell expansions, variable substitution, and
 * response files are also outside this module's scope.
 *
 * String parsing uses simple single/double quoting and backslash escapes to
 * group tokens. It is NOT a complete POSIX shell or Windows cmd.exe parser;
 * argv parsing receives tokens already separated by the process launcher.
 *
 * On any failure the parser leaves no partially created output
 * dictionary behind. The supplied dictionary ID must be unused; clm_clp
 * never overwrites a pre-existing dictionary. Other CLM ID registrations may
 * be global and are outside the dictionary's ownership.
 * -------------------------------------------------------------------------- */

#define CLM_CLP_OPERATION_SUCCESS 0
#define CLM_CLP_OPERATION_FAILURE -1
#define CLM_CLP_OPERATION_INVALID_ARGUMENT -2
#define CLM_CLP_OPERATION_INVALID_SYNTAX -3
#define CLM_CLP_OPERATION_DUPLICATE_ID -4

typedef struct clm_clp_internal_names {
    char** values;
    size_t count;
    size_t capacity;
} clm_clp_internal_names;

typedef struct clm_clp_internal_tokens {
    char** values;
    size_t count;
    size_t capacity;
} clm_clp_internal_tokens;

typedef struct clm_clp_internal_buffer {
    char* data;
    size_t length;
    size_t capacity;
} clm_clp_internal_buffer;

clm_int clm_clp_internal_track_name(clm_clp_internal_names* registered,const char* name);

void clm_clp_internal_release_names(clm_clp_internal_names* registered, bool rollback);

clm_int clm_clp_internal_resolve_key(const char* name, clm_clp_internal_names* registered, clm_id* out_id);

clm_int clm_clp_internal_set_value(clm_id dictionary_id, const char* name, const char* value, clm_clp_internal_names* registered);

bool clm_clp_internal_is_option_token(const char* token);

clm_int clm_clp_internal_parse_arguments(clm_int count, const char* const arguments[], clm_id output_dictionary_id);

bool clm_clp_internal_space(unsigned char value);

clm_int clm_clp_internal_buffer_append(clm_clp_internal_buffer* buffer, char value);

clm_int clm_clp_internal_push_token(clm_clp_internal_tokens* tokens, clm_clp_internal_buffer* buffer);

void clm_clp_internal_free_tokens(clm_clp_internal_tokens* tokens);

clm_int clm_clp_internal_tokenize(const char* command_line, clm_clp_internal_tokens* tokens);

/** Parses conventional process argv, skipping argv[0] (program name).
 * @param argc Number of pointers in argv; must be >= 0.
 * @param argv Argument vector; argv[0] is skipped when argc > 0.
 * @param output_dictionary_id Unused BPA ID for the created STRING dictionary.
 * @return SUCCESS, INVALID_ARGUMENT, INVALID_SYNTAX, DUPLICATE_ID, or FAILURE.
 * On failure no dictionary remains created.
 */
clm_int clm_clp_parse_argv(clm_int argc, const char* const argv[], clm_id output_dictionary_id);

/** Parse an already-tokenized list, interpreting every element as an argument.
 * Unlike parse_argv(), this does not discard the first element. Handy for
 * parsers, test runners, and commands already separated into string tokens.
 * @param count Number of tokens; must be >= 0.
 * @param tokens Tokens to parse; all are considered arguments.
 * @param output_dictionary_id Unused BPA ID for the created STRING dictionary.
 * @return SUCCESS, INVALID_ARGUMENT, INVALID_SYNTAX, DUPLICATE_ID, or FAILURE.
 * On failure no dictionary remains created.
 */
clm_int clm_clp_parse_tokens(clm_int count, const char* const tokens[], clm_id output_dictionary_id);

/** Tokenize and parse a supplied UTF-8 command-line string.
 * The string contains ARGUMENTS ONLY; a program/executable name is not
 * implicitly skipped. For example: "--host localhost --port=8080 --verbose".
 * This function does not execute commands or read the actual process argv.
 * @param command_line NUL-terminated input string.
 * @param output_dictionary_id Unused BPA ID for the created STRING dictionary.
 * @return SUCCESS, INVALID_ARGUMENT, INVALID_SYNTAX, DUPLICATE_ID, or FAILURE.
 * On failure no dictionary remains created.
 */
clm_int clm_clp_parse_string(const char* command_line, clm_id output_dictionary_id);