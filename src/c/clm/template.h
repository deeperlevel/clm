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

/* Text templates in a Jinja2-like language.
 *
 * A template is compiled once and rendered any number of times with different
 * data. Rendering never runs code: templates can only read variables, so they
 * are safe to take from less trusted sources, and output size, loop iterations
 * and nesting are bounded.
 *
 * Syntax
 *   {{ expression }}                       print a value
 *   {% if a %}..{% elif b %}..{% else %}..{% endif %}
 *   {% for x in items %}..{% else %}..{% endfor %}   (else runs when empty)
 *   {% set name = expression %}            variable local to the enclosing block
 *   {% raw %}..{% endraw %}                text copied without interpretation
 *   {# comment #}
 *   {%- ... -%}, {{- ... -}}, {#- ... -#}  strip whitespace on that side
 * Expressions
 *   literals: 'text' "text" 12 3.5 true false none (also True False None)
 *   a.b  a['b']  a[0]  a[-1]               members and array elements
 *   + - * / // % **  ~ (join as text)  == != < <= > >=  and or not
 *   x in y  x not in y  (substring, array element, or object key)
 *   x if cond else y
 *   x is [not] test(args)                  tests: defined undefined none string
 *     number integer float boolean mapping sequence even odd divisibleby(n)
 *     true false lower upper
 *   x|filter(args)                         filters: upper lower capitalize
 *     title trim length (count) default(value, boolean) (d) join(sep)
 *     replace(old, new) escape (e) safe first last int(default) float(default)
 *     string abs round(digits) reverse truncate(length, killwords, end)
 *     urlencode sum
 *   range(stop) range(start, stop[, step]) as a for-loop iterable
 *   loop.index loop.index0 loop.revindex loop.revindex0 loop.first loop.last
 *   loop.length inside a for loop
 * Not supported: macros, includes, inheritance, list/dict literals, iterating
 * the keys of an object, calling methods, and tuple unpacking.
 *
 * Data
 *   A clm_template_context holds named values (strings, numbers, booleans,
 *   null) and can bind clm_tree nodes (objects, arrays, leaves) by reference,
 *   so JSON/YAML/TOML-parsed data renders directly. Object members are found by
 *   name through the clm_id registry, so the names a template uses must be
 *   registered (use the *_loads_auto_ids parsers or clm_id_register).
 *   Strings are UTF-8; filters change ASCII letters only.
 * Undefined names print as nothing and are falsy, unless strict_undefined is on.
 */

#include "base.h"
#include "bytes.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CLM_TEMPLATE_SUCCESS ((clm_int)0)
#define CLM_TEMPLATE_FAILURE ((clm_int)-1)
#define CLM_TEMPLATE_INVALID_ARGUMENT ((clm_int)-2)
#define CLM_TEMPLATE_NO_MEMORY ((clm_int)-3)
/** The template text is malformed; compile fails and reports line and column. */
#define CLM_TEMPLATE_SYNTAX_ERROR ((clm_int)-4)
/** A value could not be used as written, for example text divided by a number. */
#define CLM_TEMPLATE_RENDER_ERROR ((clm_int)-5)
/** Output size, loop iterations or nesting went past a configured limit. */
#define CLM_TEMPLATE_LIMIT_EXCEEDED ((clm_int)-6)
#define CLM_TEMPLATE_BUFFER_TOO_SMALL ((clm_int)-7)

/** Used when clm_template_options.max_output is 0. */
#define CLM_TEMPLATE_DEFAULT_MAX_OUTPUT ((size_t)16 * 1024 * 1024)
/** Used when clm_template_options.max_iterations is 0. */
#define CLM_TEMPLATE_DEFAULT_MAX_ITERATIONS ((size_t)1000000)

/** Filled on failure when a non-NULL pointer is passed; zeroed on success. */
typedef struct {
    clm_int code;
    /** 1-based position in the template text; 0 when unknown. */
    size_t line;
    size_t column;
    char message[128];
} clm_template_error;

/** Compile settings; a zeroed struct (or NULL) selects Jinja's defaults. */
typedef struct {
    /** Remove the first newline after a {% %} block tag or comment. */
    bool trim_blocks;
    /** Remove spaces and tabs before a {% %} block tag at the start of a line. */
    bool lstrip_blocks;
} clm_template_syntax;

/** Render settings; a zeroed struct (or NULL) selects the defaults. */
typedef struct {
    /** HTML-escape printed values unless they went through |safe or |escape. */
    bool autoescape;
    /** Printing or computing with an undefined name is an error. */
    bool strict_undefined;
    /** Cap on bytes produced by one render; 0 selects the default. */
    size_t max_output;
    /** Cap on loop iterations in one render; 0 selects the default. */
    size_t max_iterations;
} clm_template_options;

/** A compiled template. A zeroed value is empty; dispose releases it. */
typedef struct clm_template {
    struct clm_template_impl* impl;
} clm_template;

/** Named values for rendering. Treat the fields as private. */
typedef struct {
    struct clm_template_variable* variables;
    size_t count;
    size_t capacity;
    bool has_root;
    clm_id root;
} clm_template_context;

/** Parses template text into a reusable template.
 * @param source Template text; it is copied, so it need not outlive the call.
 * @param length Byte length of source.
 * @param syntax Optional compile settings.
 * @param error Optional; receives the problem and its line and column.
 * @return SUCCESS, SYNTAX_ERROR, INVALID_ARGUMENT, or NO_MEMORY.
 */
clm_int clm_template_compile(
    clm_template* tmpl, const char* source, size_t length,
    const clm_template_syntax* syntax, clm_template_error* error);

/** Compiles the text of an existing CLM string block. */
clm_int clm_template_compile_string_id(
    clm_template* tmpl, clm_id string_id,
    const clm_template_syntax* syntax, clm_template_error* error);

void clm_template_dispose(clm_template* tmpl);

clm_int clm_template_context_init(clm_template_context* context);
void clm_template_context_dispose(clm_template_context* context);

/** Names must be identifiers (letters, digits, underscore; not starting with a
 * digit). Setting an existing name replaces its value.
 * @return SUCCESS, INVALID_ARGUMENT, or NO_MEMORY.
 */
clm_int clm_template_context_set_string(
    clm_template_context* context, const char* name, const char* value);
/** Copies the text of a CLM string block. */
clm_int clm_template_context_set_string_id(
    clm_template_context* context, const char* name, clm_id string_id);
clm_int clm_template_context_set_int(
    clm_template_context* context, const char* name, int64_t value);
clm_int clm_template_context_set_double(
    clm_template_context* context, const char* name, double value);
clm_int clm_template_context_set_bool(
    clm_template_context* context, const char* name, bool value);
clm_int clm_template_context_set_null(clm_template_context* context, const char* name);
/** Binds a clm_tree node (object, array, or leaf) by reference; it must stay
 * alive and unchanged while rendering. */
clm_int clm_template_context_set_tree(
    clm_template_context* context, const char* name, clm_id tree_id);
/** Makes the members of a tree object visible as top-level names, after the
 * values set by name. Replaces any previous root. */
clm_int clm_template_context_set_root(clm_template_context* context, clm_id tree_object_id);

/** Renders into out, appending. On failure out is restored to its old length.
 * @param context Optional data; NULL renders with no names defined.
 * @param options Optional render settings.
 * @return SUCCESS, RENDER_ERROR, LIMIT_EXCEEDED, INVALID_ARGUMENT, or NO_MEMORY.
 */
clm_int clm_template_render(
    const clm_template* tmpl, const clm_template_context* context,
    const clm_template_options* options, clm_bytes_buffer* out,
    clm_template_error* error);

/** Renders into a NUL-terminated C string; a NULL dest with capacity 0 only
 * measures. required receives the size including the NUL.
 * @return SUCCESS, BUFFER_TOO_SMALL (dest unchanged), or a render error.
 */
clm_int clm_template_render_to(
    const clm_template* tmpl, const clm_template_context* context,
    const clm_template_options* options, char* dest, size_t capacity,
    size_t* required, clm_template_error* error);

/** Renders into an existing CLM string block, replacing its text. Fails if the
 * result is not valid UTF-8 or contains a NUL byte. */
clm_int clm_template_render_string_id(
    const clm_template* tmpl, const clm_template_context* context,
    const clm_template_options* options, clm_id destination_id,
    clm_template_error* error);

/** Compiles and renders template text once. */
clm_int clm_template_render_text(
    const char* source, size_t length, const clm_template_context* context,
    const clm_template_options* options, clm_bytes_buffer* out,
    clm_template_error* error);

/** One-shot render between CLM strings: reads the template from template_id and
 * replaces the text of the existing destination_id. */
clm_int clm_template_render_ids(
    clm_id template_id, const clm_template_context* context,
    const clm_template_options* options, clm_id destination_id,
    clm_template_error* error);
