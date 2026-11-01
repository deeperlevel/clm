#pragma once

#include "test.h"
#include "../clm/template.h"
#include "../clm/string_json.h"
#include "../clm/tree.h"
#include "../clm/id.h"

static clm_int clm_template_test_status(
    const char* source, const clm_template_context* context, const clm_template_options* options,
    clm_template_error* error, clm_bytes_buffer* out)
{
    return clm_template_render_text(source, strlen(source), context, options, out, error);
}

static int clm_template_test_render(
    const char* source, const clm_template_context* context, const clm_template_options* options,
    const char* expected)
{
    clm_bytes_buffer out = {0};
    clm_template_error error;
    const clm_int status = clm_template_test_status(source, context, options, &error, &out);
    const int ok = status == CLM_TEMPLATE_SUCCESS &&
        clm_bytes_view_equals(clm_bytes_buffer_view(&out), clm_bytes_view_from_string(expected));
    if (!ok && status != CLM_TEMPLATE_SUCCESS)
        clm_test_write_format("      template failure %d at %zu:%zu: %s\n", (int)status, error.line, error.column,
                              error.message);
    clm_bytes_buffer_dispose(&out);
    return ok;
}

static int clm_template_test_fails(
    const char* source, const clm_template_context* context, const clm_template_options* options,
    const clm_int code, const size_t line)
{
    clm_bytes_buffer out = {0};
    clm_template_error error;
    const clm_int status = clm_template_test_status(source, context, options, &error, &out);
    const int ok = status == code && error.code == code && (line == 0 || error.line == line) &&
        error.message[0] != '\0' && clm_bytes_buffer_length(&out) == 0;
    clm_bytes_buffer_dispose(&out);
    return ok;
}

static int clm_template_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Template");
    const clm_id source_id = 77600;
    const clm_id root_id = 77601;
    const clm_id template_id = 77602;
    const clm_id destination_id = 77603;
    const char* json =
        "{\"tpl_user\":{\"tpl_name\":\"Ada\",\"tpl_age\":36,\"tpl_tags\":[\"math\",\"code\"]},"
        "\"tpl_items\":[{\"tpl_label\":\"a\",\"tpl_price\":1.5},{\"tpl_label\":\"b\",\"tpl_price\":2}],"
        "\"tpl_nums\":[1,2,3.5],\"tpl_flag\":true,\"tpl_nothing\":null}";
    const char* names[] = {"tpl_user", "tpl_name", "tpl_age", "tpl_tags", "tpl_items", "tpl_label",
                           "tpl_price", "tpl_nums", "tpl_flag", "tpl_nothing"};

    clm_template_context ctx;
    clm_template_context_init(&ctx);
    clm_template_context_set_string(&ctx, "name", "Ada");
    clm_template_context_set_int(&ctx, "age", 36);
    clm_template_context_set_double(&ctx, "ratio", 2.5);
    clm_template_context_set_bool(&ctx, "admin", true);
    clm_template_context_set_null(&ctx, "nothing");
    CLM_ASSERT(clm_test_parse_text(source_id, json, root_id, clm_string_json_loads_auto_ids) ==
                   CLM_FORMAT_DESERIALIZE_SUCCESS &&
                   clm_template_context_set_root(&ctx, root_id) == CLM_TEMPLATE_SUCCESS,
               "build a context and a parsed JSON tree as the template root");

    CLM_ASSERT(clm_template_test_render("Hello {{ name }}!", &ctx, NULL, "Hello Ada!") &&
                   clm_template_test_render("{{ age }} {{ ratio }} {{ admin }} {{ nothing }}", &ctx, NULL,
                                            "36 2.5 True None") &&
                   clm_template_test_render("plain text only", NULL, NULL, "plain text only") &&
                   clm_template_test_render("", NULL, NULL, ""),
               "print text and context values of each type");
    CLM_ASSERT(clm_template_test_render("{{ tpl_user.tpl_name }} {{ tpl_user.tpl_age }} {{ tpl_flag }} {{ tpl_nothing }}",
                                        &ctx, NULL, "Ada 36 True None") &&
                   clm_template_test_render("{{ tpl_user.tpl_tags[0] }} {{ tpl_user.tpl_tags[-1] }} "
                                            "{{ tpl_user['tpl_name'] }} [{{ tpl_user.nope.deeper }}]",
                                            &ctx, NULL, "math code Ada []"),
               "read tree members, array elements, negative indexes, and missing members");
    CLM_ASSERT(clm_template_test_render("{{ 1 + 2 * 3 }} {{ (1 + 2) * 3 }} {{ 7 // 2 }} {{ -7 // 2 }} {{ 7 % 3 }} "
                                        "{{ -7 % 3 }} {{ 2 ** 10 }} {{ 7 / 2 }} {{ 10 / 2 }} {{ 1.0 }}",
                                        NULL, NULL, "7 9 3 -4 1 2 1024 3.5 5.0 1.0") &&
                   clm_template_test_render("{{ 'a' ~ 1 ~ 'b' }} {{ 'ab' + 'cd' }} {{ 9223372036854775807 + 1 }}",
                                            NULL, NULL, "a1b abcd 9.223372036854776e+18"),
               "evaluate arithmetic with Python semantics and overflow to float");
    CLM_ASSERT(clm_template_test_render("{{ 1 < 2 and 2 < 3 }} {{ 'a' < 'b' }} {{ not true }} {{ none or 'x' }} "
                                        "{{ 0 or 'x' }} {{ 'x' and 'y' }} {{ 1 == 1.0 }} {{ 'a' != 'b' }}",
                                        NULL, NULL, "True True False x x y True True") &&
                   clm_template_test_render("{{ 'ell' in 'hello' }} {{ 'z' not in 'hello' }} "
                                            "{{ 'math' in tpl_user.tpl_tags }} {{ 'tpl_name' in tpl_user }} "
                                            "{{ 'nope' in tpl_user }}",
                                            &ctx, NULL, "True True True True False"),
               "evaluate comparisons, logic, and membership");
    CLM_ASSERT(clm_template_test_render("{{ 'yes' if admin else 'no' }} {{ 'yes' if false else 'no' }} [{{ 'x' if false }}]",
                                        &ctx, NULL, "yes no []"),
               "evaluate conditional expressions");

    CLM_ASSERT(clm_template_test_render("{% if age < 18 %}minor{% elif age < 65 %}adult{% else %}senior{% endif %}",
                                        &ctx, NULL, "adult") &&
                   clm_template_test_render("{% if false %}a{% elif false %}b{% else %}c{% endif %}", NULL, NULL, "c") &&
                   clm_template_test_render("{% if tpl_user.tpl_tags %}has{% endif %}{% if missing %}no{% endif %}",
                                            &ctx, NULL, "has"),
               "choose branches with if, elif, and else");
    CLM_ASSERT(clm_template_test_render("{% for t in tpl_user.tpl_tags %}{{ t }},{% endfor %}", &ctx, NULL, "math,code,") &&
                   clm_template_test_render("{% for it in tpl_items %}{{ it.tpl_label }}={{ it.tpl_price }};{% endfor %}",
                                            &ctx, NULL, "a=1.5;b=2;") &&
                   clm_template_test_render("{% for x in tpl_nothing %}x{% else %}empty{% endfor %}", &ctx, NULL, "empty") &&
                   clm_template_test_render("{% for x in missing %}x{% else %}empty{% endfor %}", &ctx, NULL, "empty"),
               "loop over arrays of values and objects and run else for empty loops");
    CLM_ASSERT(clm_template_test_render("{% for i in range(3) %}{{ loop.index }}/{{ loop.length }}"
                                        "{% if not loop.last %},{% endif %}{% endfor %}",
                                        NULL, NULL, "1/3,2/3,3/3") &&
                   clm_template_test_render("{% for i in range(2, 10, 3) %}{{ i }} {% endfor %}", NULL, NULL, "2 5 8 ") &&
                   clm_template_test_render("{% for i in range(3, 0, -1) %}{{ i }} {% endfor %}", NULL, NULL, "3 2 1 ") &&
                   clm_template_test_render("{% for i in range(3) %}{{ loop.index0 }}{{ loop.revindex }}{{ loop.revindex0 }}"
                                            "{{ loop.first }} {% endfor %}",
                                            NULL, NULL, "032True 121False 210False ") &&
                   clm_template_test_render("{% for i in range(2) %}{% for j in range(2) %}{{ loop.index }}{% endfor %}-"
                                            "{{ loop.index }};{% endfor %}",
                                            NULL, NULL, "12-1;12-2;"),
               "use range, loop variables, and nested loops");
    CLM_ASSERT(clm_template_test_render("{% set x = 3 %}{% set y = x * 2 %}{{ x }}{{ y }}", NULL, NULL, "36") &&
                   clm_template_test_render("{% set x = 1 %}{% for i in range(2) %}{% set x = i %}{{ x }}{% endfor %}{{ x }}",
                                            NULL, NULL, "011") &&
                   clm_template_test_render("{% set s = 'a' ~ 'b' %}{{ s|upper }}{% set s = s ~ 'c' %}{{ s }}", NULL, NULL,
                                            "ABabc"),
               "assign variables with scoping and reassignment");

    CLM_ASSERT(clm_template_test_render("{{ 'hello world'|upper }} {{ 'HELLO'|lower }} {{ 'hello world'|capitalize }} "
                                        "{{ 'hello big world'|title }} [{{ '  x  '|trim }}]",
                                        NULL, NULL, "HELLO WORLD hello Hello world Hello Big World [x]") &&
                   clm_template_test_render("{{ 'h\xC3\xA9llo'|length }} {{ tpl_user.tpl_tags|length }} {{ tpl_user|length }} "
                                            "{{ missing|length }}",
                                            &ctx, NULL, "5 2 3 0"),
               "apply case, trim, and length filters");
    CLM_ASSERT(clm_template_test_render("{{ missing|default('none') }} {{ ''|default('d', true) }} {{ 'x'|default('d') }} "
                                        "{{ tpl_user.tpl_tags|join(', ') }} {{ tpl_user.tpl_tags|join }}",
                                        &ctx, NULL, "none d x math, code mathcode") &&
                   clm_template_test_render("{{ 'a-b-c'|replace('-', '+') }} {{ tpl_user.tpl_tags|first }} "
                                            "{{ tpl_user.tpl_tags|last }} {{ 'abc'|first }}{{ 'abc'|last }} {{ 'abc'|reverse }}",
                                            &ctx, NULL, "a+b+c math code ac cba"),
               "apply default, join, replace, first, last, and reverse");
    CLM_ASSERT(clm_template_test_render("{{ '42'|int + 1 }} {{ 'x'|int(7) }} {{ 3|float }} {{ (-5)|abs }} {{ 3.14159|round(2) }} "
                                        "{{ 2.5|round }} {{ 12|string ~ '!' }} {{ 5|string|length }} {{ '3.9'|int }}",
                                        NULL, NULL, "43 7 3.0 5 3.14 3.0 12! 1 3") &&
                   clm_template_test_render("{{ tpl_nums|sum }} {{ 'The quick brown fox'|truncate(10) }} "
                                            "{{ 'The quick brown fox'|truncate(10, true) }} {{ 'short'|truncate(10) }} "
                                            "{{ 'a b&c/d'|urlencode }}",
                                            &ctx, NULL, "6.5 The... The qui... short a%20b%26c/d"),
               "apply numeric, truncate, sum, and urlencode filters");
    CLM_ASSERT(clm_template_test_render("{{ missing is defined }} {{ name is defined }} {% if missing is not defined %}u{% endif %} "
                                        "{{ 4 is even }} {{ 3 is odd }} {{ 9 is divisibleby(3) }} {{ 'a' is string }} "
                                        "{{ nothing is none }} {{ 1.5 is float }} {{ 2 is integer }} {{ true is true }}",
                                        &ctx, NULL, "False True u True True True True True True True True") &&
                   clm_template_test_render("{{ tpl_user is mapping }} {{ tpl_user.tpl_tags is sequence }} {{ 'ab' is lower }} "
                                            "{{ 'Ab' is upper }}",
                                            &ctx, NULL, "True True True False"),
               "evaluate tests with is and is not");

    CLM_ASSERT(clm_template_test_render("a  {%- if true -%}  b  {%- endif -%}  c", NULL, NULL, "abc") &&
                   clm_template_test_render("[ {{- 'x' -}} ]", NULL, NULL, "[x]") &&
                   clm_template_test_render("a{# hidden #}b{#- trimmed -#}  c", NULL, NULL, "abc") &&
                   clm_template_test_render("{% raw %}{{ x }} {% if %}{% endraw %}!", NULL, NULL, "{{ x }} {% if %}!"),
               "strip whitespace explicitly, skip comments, and copy raw blocks");
    clm_template_syntax syntax = {0};
    syntax.trim_blocks = true;
    clm_bytes_buffer out = {0};
    clm_template tmpl = {0};
    CLM_ASSERT(clm_template_compile(&tmpl, "{% if true %}\nx\n{% endif %}\ny", 29, &syntax, NULL) == CLM_TEMPLATE_SUCCESS &&
                   clm_template_render(&tmpl, NULL, NULL, &out, NULL) == CLM_TEMPLATE_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&out), clm_bytes_view_from_string("x\ny")),
               "remove the newline after block tags with trim_blocks");
    clm_template_dispose(&tmpl);
    clm_bytes_buffer_clear(&out);
    syntax.lstrip_blocks = true;
    const char* indented = "  {% if true %}\n  x\n  {% endif %}\n";
    CLM_ASSERT(clm_template_compile(&tmpl, indented, strlen(indented), &syntax, NULL) == CLM_TEMPLATE_SUCCESS &&
                   clm_template_render(&tmpl, NULL, NULL, &out, NULL) == CLM_TEMPLATE_SUCCESS &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&out), clm_bytes_view_from_string("  x\n")),
               "strip indentation before block tags with lstrip_blocks");
    clm_template_dispose(&tmpl);
    clm_bytes_buffer_dispose(&out);

    clm_template_options escaping = {0};
    escaping.autoescape = true;
    CLM_ASSERT(clm_template_test_render("{{ '<b>' }}|{{ '<b>'|safe }}|{{ '<b>'|escape }}|{{ 1 }}", NULL, &escaping,
                                        "&lt;b&gt;|<b>|&lt;b&gt;|1") &&
                   clm_template_test_render("<p>{{ 'a&b\"c\\'d' }}</p>", NULL, &escaping, "<p>a&amp;b&#34;c&#39;d</p>") &&
                   clm_template_test_render("{{ '<b>' }}", NULL, NULL, "<b>"),
               "escape printed values with autoescape unless they are safe");

    clm_template_options strict = {0};
    strict.strict_undefined = true;
    clm_template_error error;
    clm_bytes_buffer scratch = {0};
    CLM_ASSERT(clm_template_test_render("[{{ missing }}]", &ctx, NULL, "[]") &&
                   clm_template_test_status("{{ missing }}", &ctx, &strict, &error, &scratch) == CLM_TEMPLATE_RENDER_ERROR &&
                   strstr(error.message, "missing") != NULL &&
                   clm_template_test_render("{{ missing|default('x') }}{% if missing %}a{% endif %}", &ctx, &strict, "x") &&
                   clm_template_test_fails("{{ 1 + missing }}", &ctx, &strict, CLM_TEMPLATE_RENDER_ERROR, 1),
               "treat undefined names as empty unless strict_undefined is set");
    clm_bytes_buffer_dispose(&scratch);

    CLM_ASSERT(clm_template_test_fails("a\nb\n{{ 1 + }}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 3) &&
                   clm_template_test_fails("{% if %}x{% endif %}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{% for x in %}{% endfor %}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{% endif %}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("x\n{% if true %}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 2) &&
                   clm_template_test_fails("{% for x in range(2) %}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{{ x|nope }}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{{ x is nope }}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{{ 'abc }}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{% foo %}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{{ x ", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{# never closed", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{% raw %}never closed", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{{ x|upper(1) }}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{{ f(1) }}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{{ 99999999999999999999 }}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails("{{ 1 $ 2 }}", NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1),
               "report syntax errors with their line");
    CLM_ASSERT(clm_template_test_fails("{{ 1 / 0 }}", NULL, NULL, CLM_TEMPLATE_RENDER_ERROR, 1) &&
                   clm_template_test_fails("{{ 5 % 0 }}", NULL, NULL, CLM_TEMPLATE_RENDER_ERROR, 1) &&
                   clm_template_test_fails("{{ 'a' < 1 }}", NULL, NULL, CLM_TEMPLATE_RENDER_ERROR, 1) &&
                   clm_template_test_fails("{{ 'a' * 2 }}", NULL, NULL, CLM_TEMPLATE_RENDER_ERROR, 1) &&
                   clm_template_test_fails("{{ range(3) }}", NULL, NULL, CLM_TEMPLATE_RENDER_ERROR, 1) &&
                   clm_template_test_fails("{% for x in 5 %}{% endfor %}", NULL, NULL, CLM_TEMPLATE_RENDER_ERROR, 1) &&
                   clm_template_test_fails("{% for i in range('a') %}{% endfor %}", NULL, NULL, CLM_TEMPLATE_RENDER_ERROR, 1) &&
                   clm_template_test_fails("{% for i in range(0, 5, 0) %}{% endfor %}", NULL, NULL, CLM_TEMPLATE_RENDER_ERROR, 1) &&
                   clm_template_test_fails("a{{ 'x'|abs }}", NULL, NULL, CLM_TEMPLATE_RENDER_ERROR, 1),
               "report render errors without leaving partial output");

    clm_template_options capped = {0};
    capped.max_output = 10;
    clm_bytes_buffer kept = {0};
    clm_bytes_buffer_append_string(&kept, "keep");
    CLM_ASSERT(clm_template_test_status("{% for i in range(100) %}abcdef{% endfor %}", NULL, &capped, &error, &kept) ==
                   CLM_TEMPLATE_LIMIT_EXCEEDED &&
                   clm_bytes_view_equals(clm_bytes_buffer_view(&kept), clm_bytes_view_from_string("keep")),
               "stop at the output limit and restore the buffer");
    clm_bytes_buffer_dispose(&kept);
    capped.max_output = 0;
    capped.max_iterations = 5;
    CLM_ASSERT(clm_template_test_render("{% for i in range(5) %}x{% endfor %}", NULL, &capped, "xxxxx") &&
                   clm_template_test_fails("{% for i in range(6) %}x{% endfor %}", NULL, &capped, CLM_TEMPLATE_LIMIT_EXCEEDED, 1) &&
                   clm_template_test_fails("{% for i in range(3) %}{% for j in range(3) %}{% endfor %}{% endfor %}", NULL,
                                           &capped, CLM_TEMPLATE_LIMIT_EXCEEDED, 1) &&
                   clm_template_test_fails("{% for i in range(9223372036854775807) %}{% endfor %}", NULL, NULL,
                                           CLM_TEMPLATE_LIMIT_EXCEEDED, 1),
               "stop loops at the iteration limit, including enormous ranges");

    char deep[2048];
    size_t used = 0;
    for (int i = 0; i < 100; ++i)
        used += (size_t)snprintf(deep + used, sizeof(deep) - used, "{%% if true %%}");
    CLM_ASSERT(clm_template_test_fails(deep, NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1),
               "reject blocks nested too deeply");
    char nested[2048];
    used = 0;
    used += (size_t)snprintf(nested + used, sizeof(nested) - used, "{{ ");
    for (int i = 0; i < 300; ++i) used += (size_t)snprintf(nested + used, sizeof(nested) - used, "(");
    used += (size_t)snprintf(nested + used, sizeof(nested) - used, "1");
    for (int i = 0; i < 300; ++i) used += (size_t)snprintf(nested + used, sizeof(nested) - used, ")");
    snprintf(nested + used, sizeof(nested) - used, " }}");
    char chain[2048];
    used = (size_t)snprintf(chain, sizeof(chain), "{{ 1");
    for (int i = 0; i < 300; ++i) used += (size_t)snprintf(chain + used, sizeof(chain) - used, "+1");
    snprintf(chain + used, sizeof(chain) - used, " }}");
    char negations[2048];
    used = (size_t)snprintf(negations, sizeof(negations), "{{ ");
    for (int i = 0; i < 400; ++i) used += (size_t)snprintf(negations + used, sizeof(negations) - used, "- ");
    snprintf(negations + used, sizeof(negations) - used, "1 }}");
    CLM_ASSERT(clm_template_test_fails(nested, NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails(chain, NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1) &&
                   clm_template_test_fails(negations, NULL, NULL, CLM_TEMPLATE_SYNTAX_ERROR, 1),
               "reject expressions nested too deeply");

    clm_template_context other;
    clm_id user_id = 0;
    clm_template_context_init(&other);
    CLM_ASSERT(clm_tree_get_object(root_id, clm_id_get("tpl_user"), &user_id) == CLM_BPA_GET_SUCCESS &&
                   clm_template_context_set_tree(&other, "user", user_id) == CLM_TEMPLATE_SUCCESS &&
                   clm_template_context_set_string(&other, "name", "first") == CLM_TEMPLATE_SUCCESS &&
                   clm_template_context_set_string(&other, "name", "second") == CLM_TEMPLATE_SUCCESS &&
                   clm_template_test_render("{{ user.tpl_name }} {{ name }} {{ user.tpl_tags|length }}", &other, NULL,
                                            "Ada second 2") &&
                   clm_template_context_set_string(&other, "9bad", "x") == CLM_TEMPLATE_INVALID_ARGUMENT &&
                   clm_template_context_set_string(&other, "bad name", "x") == CLM_TEMPLATE_INVALID_ARGUMENT &&
                   clm_template_context_set_string(&other, "ok", NULL) == CLM_TEMPLATE_INVALID_ARGUMENT &&
                   clm_template_context_set_root(&other, 77999) == CLM_TEMPLATE_INVALID_ARGUMENT,
               "bind tree nodes by name, replace values, and reject invalid names");
    clm_template_context_dispose(&other);

    const char* greeting = "{{ greeting }}, {{ who }}!";
    CLM_ASSERT(clm_template_compile(&tmpl, greeting, strlen(greeting), NULL, NULL) == CLM_TEMPLATE_SUCCESS,
               "compile a reusable template");
    clm_template_context first;
    clm_template_context second;
    clm_template_context_init(&first);
    clm_template_context_init(&second);
    clm_template_context_set_string(&first, "greeting", "Hello");
    clm_template_context_set_string(&first, "who", "World");
    clm_template_context_set_string(&second, "greeting", "Bye");
    clm_template_context_set_string(&second, "who", "Ada");
    char text[64];
    size_t required = 0;
    CLM_ASSERT(clm_template_render_to(&tmpl, &first, NULL, text, sizeof(text), &required, NULL) == CLM_TEMPLATE_SUCCESS &&
                   strcmp(text, "Hello, World!") == 0 && required == 14 &&
                   clm_template_render_to(&tmpl, &second, NULL, text, sizeof(text), &required, NULL) == CLM_TEMPLATE_SUCCESS &&
                   strcmp(text, "Bye, Ada!") == 0 &&
                   clm_template_render_to(&tmpl, &first, NULL, NULL, 0, &required, NULL) == CLM_TEMPLATE_SUCCESS &&
                   required == 14 &&
                   clm_template_render_to(&tmpl, &first, NULL, text, 5, &required, NULL) == CLM_TEMPLATE_BUFFER_TOO_SMALL &&
                   required == 14 &&
                   clm_template_render_to(&tmpl, &first, NULL, NULL, 3, &required, NULL) == CLM_TEMPLATE_INVALID_ARGUMENT,
               "render one compiled template with different contexts into C buffers");
    clm_template_dispose(&tmpl);
    clm_template_context_dispose(&first);
    clm_template_context_dispose(&second);
    CLM_ASSERT(tmpl.impl == NULL && clm_template_render(&tmpl, NULL, NULL, &out, NULL) == CLM_TEMPLATE_INVALID_ARGUMENT &&
                   clm_template_compile(NULL, "x", 1, NULL, NULL) == CLM_TEMPLATE_INVALID_ARGUMENT &&
                   clm_template_compile(&tmpl, NULL, 1, NULL, NULL) == CLM_TEMPLATE_INVALID_ARGUMENT,
               "reject disposed templates and invalid compile arguments");

    clm_template_context strings;
    clm_template_context_init(&strings);
    CLM_ASSERT(clm_string_new(template_id, "Dear {{ who }}: {{ n }} items") && clm_string_new(destination_id, "old") &&
                   clm_string_new(77604, "Grace") &&
                   clm_template_context_set_string_id(&strings, "who", 77604) == CLM_TEMPLATE_SUCCESS &&
                   clm_template_context_set_int(&strings, "n", 3) == CLM_TEMPLATE_SUCCESS &&
                   clm_template_render_ids(template_id, &strings, NULL, destination_id, NULL) == CLM_TEMPLATE_SUCCESS &&
                   strcmp((const char*)clm_bpa_find_block(destination_id)->data, "Dear Grace: 3 items") == 0 &&
                   clm_template_render_ids(template_id, &strings, NULL, 77999, NULL) == CLM_TEMPLATE_INVALID_ARGUMENT &&
                   clm_template_render_ids(77999, &strings, NULL, destination_id, NULL) == CLM_TEMPLATE_INVALID_ARGUMENT &&
                   clm_template_context_set_string_id(&strings, "who", 77999) == CLM_TEMPLATE_INVALID_ARGUMENT,
               "render between CLM strings");
    CLM_ASSERT(clm_string_set(template_id, "{{ 1 +") >= 0 &&
                   clm_template_render_ids(template_id, &strings, NULL, destination_id, &error) == CLM_TEMPLATE_SYNTAX_ERROR &&
                   error.line == 1 &&
                   strcmp((const char*)clm_bpa_find_block(destination_id)->data, "Dear Grace: 3 items") == 0,
               "leave the destination string unchanged when a template fails");
    CLM_ASSERT(clm_string_delete(template_id) && clm_string_delete(destination_id) && clm_string_delete(77604),
               "release the template strings");
    clm_template_context_dispose(&strings);

    clm_template_context_dispose(&ctx);
    CLM_ASSERT(clm_tree_delete(root_id) == CLM_BPA_SUCCESS, "release the template data tree");
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
        clm_id_remove(names[i]);

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
