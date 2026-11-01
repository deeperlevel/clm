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

#include "template.h"
#include "bpa.h"
#include "id.h"
#include "string.h"
#include "stringtify.h"
#include "tree.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TPL_OK CLM_TEMPLATE_SUCCESS
#define TPL_SYNTAX CLM_TEMPLATE_SYNTAX_ERROR
#define TPL_RENDER CLM_TEMPLATE_RENDER_ERROR
#define TPL_LIMIT CLM_TEMPLATE_LIMIT_EXCEEDED
#define TPL_MEMORY CLM_TEMPLATE_NO_MEMORY

#define MAX_STATEMENT_DEPTH 64
#define MAX_EXPR_DEPTH 128
#define MAX_ARGS 3

/* ---- arena: memory released all at once ---------------------------------- */

typedef struct tpl_block {
    struct tpl_block* next;
    size_t used;
    size_t capacity;
} tpl_block;

typedef struct {
    tpl_block* head;
} tpl_arena;

#define ARENA_HEADER ((sizeof(tpl_block) + 15u) & ~(size_t)15u)

static void* arena_alloc(tpl_arena* arena, size_t size)
{
    if (size > SIZE_MAX - 64) return NULL;
    size = (size + 7u) & ~(size_t)7u;
    tpl_block* block = arena->head;
    if (block == NULL || block->capacity - block->used < size) {
        const size_t capacity = size > 4096 ? size : 4096;
        block = (tpl_block*)malloc(ARENA_HEADER + capacity);
        if (block == NULL) return NULL;
        block->next = arena->head;
        block->used = 0;
        block->capacity = capacity;
        arena->head = block;
    }
    void* result = (unsigned char*)block + ARENA_HEADER + block->used;
    block->used += size;
    return result;
}

static void arena_free(tpl_arena* arena)
{
    tpl_block* block = arena->head;
    while (block != NULL) {
        tpl_block* next = block->next;
        free(block);
        block = next;
    }
    arena->head = NULL;
}

static char* arena_copy(tpl_arena* arena, const char* text, size_t length)
{
    if (length == SIZE_MAX) return NULL;
    char* copy = (char*)arena_alloc(arena, length + 1);
    if (copy == NULL) return NULL;
    if (length != 0) memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

/* ---- values -------------------------------------------------------------- */

typedef enum { V_UNDEF, V_NONE, V_BOOL, V_INT, V_DOUBLE, V_STRING, V_TREE, V_LOOP } value_kind;

typedef struct {
    size_t index0;
    size_t length;
} loop_state;

typedef struct {
    value_kind kind;
    bool safe;
    union {
        bool boolean;
        int64_t integer;
        double real;
        struct { const char* data; size_t length; } text;
        clm_id tree;
        const loop_state* loop;
    } u;
} value;

static value v_undef(void) { value v; memset(&v, 0, sizeof(v)); v.kind = V_UNDEF; return v; }
static value v_none(void) { value v = v_undef(); v.kind = V_NONE; return v; }
static value v_bool(bool b) { value v = v_undef(); v.kind = V_BOOL; v.u.boolean = b; return v; }
static value v_int(int64_t i) { value v = v_undef(); v.kind = V_INT; v.u.integer = i; return v; }
static value v_double(double d) { value v = v_undef(); v.kind = V_DOUBLE; v.u.real = d; return v; }
static value v_tree(clm_id id) { value v = v_undef(); v.kind = V_TREE; v.u.tree = id; return v; }

static value v_text(const char* data, size_t length, bool safe)
{
    value v = v_undef();
    v.kind = V_STRING;
    v.safe = safe;
    v.u.text.data = data != NULL ? data : "";
    v.u.text.length = length;
    return v;
}

/* ---- syntax tree --------------------------------------------------------- */

typedef enum {
    EX_STR, EX_INT, EX_DOUBLE, EX_BOOL, EX_NONE, EX_VAR, EX_ATTR, EX_INDEX,
    EX_FILTER, EX_CALL, EX_UNARY, EX_BINARY, EX_TEST, EX_COND
} expr_kind;

typedef enum {
    OP_OR, OP_AND, OP_NOT, OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_IN, OP_NOT_IN,
    OP_ADD, OP_SUB, OP_CONCAT, OP_MUL, OP_DIV, OP_FLOORDIV, OP_MOD, OP_POW, OP_NEG, OP_POS
} expr_op;

typedef struct expr expr;
struct expr {
    expr_kind kind;
    expr_op op;
    size_t offset;
    size_t depth;
    const char* name;
    size_t name_length;
    int64_t integer;
    double real;
    bool boolean;
    bool negate;
    int builtin;
    expr* a;
    expr* b;
    expr* c;
    expr** args;
    size_t arg_count;
};

typedef enum { ND_TEXT, ND_OUTPUT, ND_IF, ND_FOR, ND_SET } node_kind;

typedef struct node node;
struct node {
    node_kind kind;
    node* next;
    size_t offset;
    const char* text;
    size_t length;
    expr* expression;
    const char* name;
    size_t name_length;
    node* body;
    node* else_body;
};

struct clm_template_impl {
    tpl_arena arena;
    char* source;
    size_t length;
    node* root;
};

/* ---- built-in filters and tests ------------------------------------------ */

enum {
    F_UPPER = 1, F_LOWER, F_CAPITALIZE, F_TITLE, F_TRIM, F_LENGTH, F_DEFAULT, F_JOIN,
    F_REPLACE, F_ESCAPE, F_SAFE, F_FIRST, F_LAST, F_INT, F_FLOAT, F_STRING, F_ABS,
    F_ROUND, F_REVERSE, F_TRUNCATE, F_URLENCODE, F_SUM
};

enum {
    T_DEFINED = 1, T_UNDEFINED, T_NONE, T_STRING, T_NUMBER, T_INTEGER, T_FLOAT, T_BOOLEAN,
    T_MAPPING, T_SEQUENCE, T_EVEN, T_ODD, T_DIVISIBLEBY, T_TRUE, T_FALSE, T_LOWER, T_UPPER
};

typedef struct {
    const char* name;
    int id;
    size_t min_args;
    size_t max_args;
} builtin_def;

static const builtin_def FILTERS[] = {
    {"upper", F_UPPER, 0, 0}, {"lower", F_LOWER, 0, 0}, {"capitalize", F_CAPITALIZE, 0, 0},
    {"title", F_TITLE, 0, 0}, {"trim", F_TRIM, 0, 0}, {"length", F_LENGTH, 0, 0},
    {"count", F_LENGTH, 0, 0}, {"default", F_DEFAULT, 0, 2}, {"d", F_DEFAULT, 0, 2},
    {"join", F_JOIN, 0, 1}, {"replace", F_REPLACE, 2, 2}, {"escape", F_ESCAPE, 0, 0},
    {"e", F_ESCAPE, 0, 0}, {"safe", F_SAFE, 0, 0}, {"first", F_FIRST, 0, 0},
    {"last", F_LAST, 0, 0}, {"int", F_INT, 0, 1}, {"float", F_FLOAT, 0, 1},
    {"string", F_STRING, 0, 0}, {"abs", F_ABS, 0, 0}, {"round", F_ROUND, 0, 1},
    {"reverse", F_REVERSE, 0, 0}, {"truncate", F_TRUNCATE, 0, 3},
    {"urlencode", F_URLENCODE, 0, 0}, {"sum", F_SUM, 0, 0}
};

static const builtin_def TESTS[] = {
    {"defined", T_DEFINED, 0, 0}, {"undefined", T_UNDEFINED, 0, 0}, {"none", T_NONE, 0, 0},
    {"string", T_STRING, 0, 0}, {"number", T_NUMBER, 0, 0}, {"integer", T_INTEGER, 0, 0},
    {"float", T_FLOAT, 0, 0}, {"boolean", T_BOOLEAN, 0, 0}, {"mapping", T_MAPPING, 0, 0},
    {"sequence", T_SEQUENCE, 0, 0}, {"even", T_EVEN, 0, 0}, {"odd", T_ODD, 0, 0},
    {"divisibleby", T_DIVISIBLEBY, 1, 1}, {"true", T_TRUE, 0, 0}, {"false", T_FALSE, 0, 0},
    {"lower", T_LOWER, 0, 0}, {"upper", T_UPPER, 0, 0}
};

static const builtin_def* find_builtin(
    const builtin_def* table, size_t count, const char* name, size_t length)
{
    for (size_t i = 0; i < count; ++i)
        if (strlen(table[i].name) == length && memcmp(table[i].name, name, length) == 0)
            return &table[i];
    return NULL;
}

/* ---- diagnostics --------------------------------------------------------- */

static void position_of(
    const char* source, size_t length, size_t offset, size_t* line, size_t* column)
{
    size_t current = 1;
    size_t line_start = 0;
    if (offset > length) offset = length;
    for (size_t i = 0; i < offset; ++i) {
        if (source[i] == '\n') {
            ++current;
            line_start = i + 1;
        }
    }
    *line = current;
    *column = offset - line_start + 1;
}

static void fill_error(
    clm_template_error* error, const clm_int code, const char* source, const size_t length,
    const size_t offset, const char* message)
{
    if (error == NULL) return;
    memset(error, 0, sizeof(*error));
    error->code = code;
    if (source != NULL) position_of(source, length, offset, &error->line, &error->column);
    snprintf(error->message, sizeof(error->message), "%s", message);
}

static void clear_error(clm_template_error* error)
{
    if (error != NULL) memset(error, 0, sizeof(*error));
}

/* ---- parser: tokens and expressions -------------------------------------- */

typedef struct {
    tpl_arena* arena;
    const char* base;
    size_t length;
    clm_template_error* error;
    bool failed;
    bool trim_blocks;
    bool lstrip_blocks;
    bool strip_next;
    size_t pos;
    size_t depth;
} parser;

static clm_int p_fail(parser* p, const clm_int code, const size_t offset, const char* format, ...)
{
    if (!p->failed) {
        char message[128];
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(message, sizeof(message), format, arguments);
        va_end(arguments);
        fill_error(p->error, code, p->base, p->length, offset, message);
        p->failed = true;
    }
    return code;
}

typedef enum { TK_END, TK_NAME, TK_INT, TK_FLOAT, TK_STRING, TK_OP } token_type;

typedef struct {
    token_type type;
    size_t start;
    size_t length;
    int64_t integer;
    double real;
    const char* text;
    size_t text_length;
} token;

typedef struct {
    parser* p;
    size_t pos;
    size_t end;
    size_t depth;
    token tok;
    clm_int status;
} xparser;

static bool is_space(const char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
static bool is_digit(const char c) { return c >= '0' && c <= '9'; }
static bool is_name_start(const char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool is_name_char(const char c) { return is_name_start(c) || is_digit(c); }

static void x_fail(xparser* x, const size_t offset, const char* format, ...)
{
    if (x->status == TPL_OK) {
        char message[128];
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(message, sizeof(message), format, arguments);
        va_end(arguments);
        x->status = p_fail(x->p, TPL_SYNTAX, offset, "%s", message);
    }
    x->tok.type = TK_END;
}

static void x_next(xparser* x)
{
    const char* s = x->p->base;
    token* t = &x->tok;
    while (x->pos < x->end && is_space(s[x->pos])) ++x->pos;
    memset(t, 0, sizeof(*t));
    t->start = x->pos;
    if (x->pos >= x->end) {
        t->type = TK_END;
        return;
    }
    const char c = s[x->pos];
    if (is_name_start(c)) {
        const size_t begin = x->pos;
        while (x->pos < x->end && is_name_char(s[x->pos])) ++x->pos;
        t->type = TK_NAME;
        t->length = x->pos - begin;
        t->text = s + begin;
        t->text_length = t->length;
        return;
    }
    if (is_digit(c)) {
        const size_t begin = x->pos;
        bool is_float = false;
        while (x->pos < x->end && is_digit(s[x->pos])) ++x->pos;
        if (x->pos + 1 < x->end && s[x->pos] == '.' && is_digit(s[x->pos + 1])) {
            is_float = true;
            ++x->pos;
            while (x->pos < x->end && is_digit(s[x->pos])) ++x->pos;
        }
        if (x->pos < x->end && (s[x->pos] == 'e' || s[x->pos] == 'E')) {
            size_t k = x->pos + 1;
            if (k < x->end && (s[k] == '+' || s[k] == '-')) ++k;
            if (k < x->end && is_digit(s[k])) {
                is_float = true;
                while (k < x->end && is_digit(s[k])) ++k;
                x->pos = k;
            }
        }
        char digits[64];
        const size_t count = x->pos - begin;
        if (count >= sizeof(digits)) {
            x_fail(x, begin, "number is too long");
            return;
        }
        memcpy(digits, s + begin, count);
        digits[count] = '\0';
        t->length = count;
        if (is_float) {
            t->type = TK_FLOAT;
            t->real = strtod(digits, NULL);
        } else {
            unsigned long long parsed = 0;
            for (size_t i = 0; i < count; ++i) {
                const unsigned digit = (unsigned)(digits[i] - '0');
                if (parsed > (UINT64_MAX - digit) / 10) {
                    x_fail(x, begin, "integer is too large");
                    return;
                }
                parsed = parsed * 10 + digit;
            }
            if (parsed > (unsigned long long)INT64_MAX) {
                x_fail(x, begin, "integer is too large");
                return;
            }
            t->type = TK_INT;
            t->integer = (int64_t)parsed;
        }
        return;
    }
    if (c == '\'' || c == '"') {
        const size_t begin = x->pos++;
        char* text = (char*)arena_alloc(x->p->arena, x->end - begin + 1);
        size_t out = 0;
        if (text == NULL) {
            x->status = TPL_MEMORY;
            t->type = TK_END;
            return;
        }
        for (;;) {
            if (x->pos >= x->end) {
                x_fail(x, begin, "unterminated string");
                return;
            }
            char d = s[x->pos++];
            if (d == c) break;
            if (d == '\\' && x->pos < x->end) {
                const char e = s[x->pos++];
                d = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;
            }
            text[out++] = d;
        }
        text[out] = '\0';
        t->type = TK_STRING;
        t->length = x->pos - begin;
        t->text = text;
        t->text_length = out;
        return;
    }
    static const char* const two[] = {"==", "!=", "<=", ">=", "//", "**"};
    for (size_t i = 0; i < sizeof(two) / sizeof(two[0]); ++i) {
        if (x->pos + 1 < x->end && s[x->pos] == two[i][0] && s[x->pos + 1] == two[i][1]) {
            t->type = TK_OP;
            t->length = 2;
            t->text = s + x->pos;
            x->pos += 2;
            return;
        }
    }
    if (c != '\0' && strchr("+-*/%~<>=|.,()[]:", c) != NULL) {
        t->type = TK_OP;
        t->length = 1;
        t->text = s + x->pos;
        ++x->pos;
        return;
    }
    x_fail(x, x->pos, "unexpected character '%c'", c);
}

static bool x_is_op(const xparser* x, const char* op)
{
    const size_t n = strlen(op);
    return x->tok.type == TK_OP && x->tok.length == n && memcmp(x->tok.text, op, n) == 0;
}

static bool x_is_name(const xparser* x, const char* name)
{
    const size_t n = strlen(name);
    return x->tok.type == TK_NAME && x->tok.length == n && memcmp(x->tok.text, name, n) == 0;
}

static bool is_reserved(const char* s, const size_t n)
{
    static const char* const words[] = {"and", "or", "not", "in", "is", "if", "else"};
    for (size_t i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
        if (strlen(words[i]) == n && memcmp(words[i], s, n) == 0) return true;
    return false;
}

static expr* x_new(xparser* x, const expr_kind kind, const size_t offset)
{
    expr* e = (expr*)arena_alloc(x->p->arena, sizeof(expr));
    if (e == NULL) {
        x->status = TPL_MEMORY;
        x->tok.type = TK_END;
        return NULL;
    }
    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->offset = offset;
    e->depth = 1;
    return e;
}

static expr* x_seal(xparser* x, expr* e)
{
    if (e == NULL) return NULL;
    size_t deepest = 0;
    const expr* children[3] = {e->a, e->b, e->c};
    for (size_t i = 0; i < 3; ++i)
        if (children[i] != NULL && children[i]->depth > deepest) deepest = children[i]->depth;
    for (size_t i = 0; i < e->arg_count; ++i)
        if (e->args[i]->depth > deepest) deepest = e->args[i]->depth;
    e->depth = deepest + 1;
    if (e->depth > MAX_EXPR_DEPTH) {
        x_fail(x, e->offset, "expression is nested too deeply");
        return NULL;
    }
    return e;
}

static expr* x_parse_expr(xparser* x);

static bool x_parse_args(xparser* x, expr* e)
{
    x_next(x);
    e->args = (expr**)arena_alloc(x->p->arena, sizeof(expr*) * MAX_ARGS);
    if (e->args == NULL) {
        x->status = TPL_MEMORY;
        return false;
    }
    if (x_is_op(x, ")")) {
        x_next(x);
        return true;
    }
    for (;;) {
        if (e->arg_count == MAX_ARGS) {
            x_fail(x, x->tok.start, "too many arguments");
            return false;
        }
        expr* argument = x_parse_expr(x);
        if (argument == NULL) return false;
        e->args[e->arg_count++] = argument;
        if (x_is_op(x, ",")) {
            x_next(x);
            continue;
        }
        if (x_is_op(x, ")")) {
            x_next(x);
            return true;
        }
        x_fail(x, x->tok.start, "expected ',' or ')'");
        return false;
    }
}

static expr* x_parse_primary(xparser* x)
{
    const token t = x->tok;
    expr* e = NULL;
    switch (t.type) {
    case TK_INT:
        e = x_new(x, EX_INT, t.start);
        if (e != NULL) e->integer = t.integer;
        x_next(x);
        return e;
    case TK_FLOAT:
        e = x_new(x, EX_DOUBLE, t.start);
        if (e != NULL) e->real = t.real;
        x_next(x);
        return e;
    case TK_STRING:
        e = x_new(x, EX_STR, t.start);
        if (e != NULL) {
            e->name = t.text;
            e->name_length = t.text_length;
        }
        x_next(x);
        return e;
    case TK_NAME: {
        if (is_reserved(t.text, t.length)) {
            x_fail(x, t.start, "unexpected '%.*s'", (int)t.length, t.text);
            return NULL;
        }
        x_next(x);
        if ((t.length == 4 && memcmp(t.text, "true", 4) == 0) || (t.length == 4 && memcmp(t.text, "True", 4) == 0) ||
            (t.length == 5 && memcmp(t.text, "false", 5) == 0) || (t.length == 5 && memcmp(t.text, "False", 5) == 0)) {
            e = x_new(x, EX_BOOL, t.start);
            if (e != NULL) e->boolean = t.text[0] == 't' || t.text[0] == 'T';
            return e;
        }
        if ((t.length == 4 && memcmp(t.text, "none", 4) == 0) || (t.length == 4 && memcmp(t.text, "None", 4) == 0))
            return x_new(x, EX_NONE, t.start);
        if (x_is_op(x, "(")) {
            if (t.length != 5 || memcmp(t.text, "range", 5) != 0) {
                x_fail(x, t.start, "unknown function '%.*s'", (int)t.length, t.text);
                return NULL;
            }
            e = x_new(x, EX_CALL, t.start);
            if (e == NULL || !x_parse_args(x, e)) return NULL;
            if (e->arg_count == 0) {
                x_fail(x, t.start, "range() needs at least one argument");
                return NULL;
            }
            e->name = t.text;
            e->name_length = t.length;
            return x_seal(x, e);
        }
        e = x_new(x, EX_VAR, t.start);
        if (e != NULL) {
            e->name = t.text;
            e->name_length = t.length;
        }
        return e;
    }
    case TK_OP:
        if (x_is_op(x, "(")) {
            x_next(x);
            e = x_parse_expr(x);
            if (e == NULL) return NULL;
            if (!x_is_op(x, ")")) {
                x_fail(x, x->tok.start, "expected ')'");
                return NULL;
            }
            x_next(x);
            return e;
        }
        x_fail(x, t.start, "unexpected '%.*s'", (int)t.length, t.text);
        return NULL;
    default:
        x_fail(x, t.start, "unexpected end of expression");
        return NULL;
    }
}

static expr* x_parse_postfix(xparser* x)
{
    expr* e = x_parse_primary(x);
    while (e != NULL && x->status == TPL_OK) {
        if (x_is_op(x, ".")) {
            const size_t offset = x->tok.start;
            x_next(x);
            if (x->tok.type != TK_NAME) {
                x_fail(x, x->tok.start, "expected a member name after '.'");
                return NULL;
            }
            expr* attribute = x_new(x, EX_ATTR, offset);
            if (attribute == NULL) return NULL;
            attribute->a = e;
            attribute->name = x->tok.text;
            attribute->name_length = x->tok.length;
            x_next(x);
            e = x_seal(x, attribute);
        } else if (x_is_op(x, "[")) {
            const size_t offset = x->tok.start;
            x_next(x);
            expr* index = x_parse_expr(x);
            if (index == NULL) return NULL;
            if (!x_is_op(x, "]")) {
                x_fail(x, x->tok.start, "expected ']'");
                return NULL;
            }
            x_next(x);
            expr* subscript = x_new(x, EX_INDEX, offset);
            if (subscript == NULL) return NULL;
            subscript->a = e;
            subscript->b = index;
            e = x_seal(x, subscript);
        } else if (x_is_op(x, "|")) {
            x_next(x);
            if (x->tok.type != TK_NAME) {
                x_fail(x, x->tok.start, "expected a filter name after '|'");
                return NULL;
            }
            const token name = x->tok;
            const builtin_def* def = find_builtin(
                FILTERS, sizeof(FILTERS) / sizeof(FILTERS[0]), name.text, name.length);
            if (def == NULL) {
                x_fail(x, name.start, "unknown filter '%.*s'", (int)name.length, name.text);
                return NULL;
            }
            expr* filter = x_new(x, EX_FILTER, name.start);
            if (filter == NULL) return NULL;
            filter->a = e;
            filter->builtin = def->id;
            filter->name = name.text;
            filter->name_length = name.length;
            x_next(x);
            if (x_is_op(x, "(") && !x_parse_args(x, filter)) return NULL;
            if (filter->arg_count < def->min_args || filter->arg_count > def->max_args) {
                x_fail(x, name.start, "wrong number of arguments for filter '%.*s'", (int)name.length, name.text);
                return NULL;
            }
            e = x_seal(x, filter);
        } else {
            break;
        }
    }
    return e;
}

static expr* x_binary(xparser* x, const expr_op op, const size_t offset, expr* left, expr* right)
{
    if (left == NULL || right == NULL) return NULL;
    expr* e = x_new(x, EX_BINARY, offset);
    if (e == NULL) return NULL;
    e->op = op;
    e->a = left;
    e->b = right;
    return x_seal(x, e);
}

static expr* x_parse_unary(xparser* x);

static expr* x_parse_power(xparser* x)
{
    expr* base = x_parse_postfix(x);
    if (base != NULL && x_is_op(x, "**")) {
        const size_t offset = x->tok.start;
        x_next(x);
        return x_binary(x, OP_POW, offset, base, x_parse_unary(x));
    }
    return base;
}

static expr* x_parse_unary_inner(xparser* x)
{
    if (x_is_op(x, "-") || x_is_op(x, "+")) {
        const bool negative = x_is_op(x, "-");
        const size_t offset = x->tok.start;
        x_next(x);
        expr* operand = x_parse_unary(x);
        if (operand == NULL) return NULL;
        expr* e = x_new(x, EX_UNARY, offset);
        if (e == NULL) return NULL;
        e->op = negative ? OP_NEG : OP_POS;
        e->a = operand;
        return x_seal(x, e);
    }
    return x_parse_power(x);
}

static bool x_enter(xparser* x)
{
    if (++x->depth > MAX_EXPR_DEPTH) {
        x_fail(x, x->tok.start, "expression is nested too deeply");
        --x->depth;
        return false;
    }
    return true;
}

static expr* x_parse_unary(xparser* x)
{
    if (!x_enter(x)) return NULL;
    expr* e = x_parse_unary_inner(x);
    --x->depth;
    return e;
}

static expr* x_parse_multiplicative(xparser* x)
{
    expr* left = x_parse_unary(x);
    while (left != NULL && x->status == TPL_OK) {
        expr_op op;
        if (x_is_op(x, "*")) op = OP_MUL;
        else if (x_is_op(x, "/")) op = OP_DIV;
        else if (x_is_op(x, "//")) op = OP_FLOORDIV;
        else if (x_is_op(x, "%")) op = OP_MOD;
        else break;
        const size_t offset = x->tok.start;
        x_next(x);
        left = x_binary(x, op, offset, left, x_parse_unary(x));
    }
    return left;
}

static expr* x_parse_additive(xparser* x)
{
    expr* left = x_parse_multiplicative(x);
    while (left != NULL && x->status == TPL_OK) {
        expr_op op;
        if (x_is_op(x, "+")) op = OP_ADD;
        else if (x_is_op(x, "-")) op = OP_SUB;
        else if (x_is_op(x, "~")) op = OP_CONCAT;
        else break;
        const size_t offset = x->tok.start;
        x_next(x);
        left = x_binary(x, op, offset, left, x_parse_multiplicative(x));
    }
    return left;
}

static expr* x_parse_test(xparser* x, expr* subject, const size_t offset)
{
    bool negate = false;
    if (x_is_name(x, "not")) {
        negate = true;
        x_next(x);
    }
    if (x->tok.type != TK_NAME) {
        x_fail(x, x->tok.start, "expected a test name after 'is'");
        return NULL;
    }
    const token name = x->tok;
    const builtin_def* def = find_builtin(TESTS, sizeof(TESTS) / sizeof(TESTS[0]), name.text, name.length);
    if (def == NULL) {
        x_fail(x, name.start, "unknown test '%.*s'", (int)name.length, name.text);
        return NULL;
    }
    expr* e = x_new(x, EX_TEST, offset);
    if (e == NULL) return NULL;
    e->a = subject;
    e->negate = negate;
    e->builtin = def->id;
    x_next(x);
    if (x_is_op(x, "(") && !x_parse_args(x, e)) return NULL;
    if (e->arg_count < def->min_args || e->arg_count > def->max_args) {
        x_fail(x, name.start, "wrong number of arguments for test '%.*s'", (int)name.length, name.text);
        return NULL;
    }
    return x_seal(x, e);
}

static expr* x_parse_comparison(xparser* x)
{
    expr* left = x_parse_additive(x);
    while (left != NULL && x->status == TPL_OK) {
        expr_op op;
        const size_t offset = x->tok.start;
        if (x_is_op(x, "==")) op = OP_EQ;
        else if (x_is_op(x, "!=")) op = OP_NE;
        else if (x_is_op(x, "<")) op = OP_LT;
        else if (x_is_op(x, "<=")) op = OP_LE;
        else if (x_is_op(x, ">")) op = OP_GT;
        else if (x_is_op(x, ">=")) op = OP_GE;
        else if (x_is_name(x, "in")) op = OP_IN;
        else if (x_is_name(x, "not")) {
            x_next(x);
            if (!x_is_name(x, "in")) {
                x_fail(x, x->tok.start, "expected 'in' after 'not'");
                return NULL;
            }
            op = OP_NOT_IN;
        } else if (x_is_name(x, "is")) {
            x_next(x);
            left = x_parse_test(x, left, offset);
            continue;
        } else {
            break;
        }
        x_next(x);
        left = x_binary(x, op, offset, left, x_parse_additive(x));
    }
    return left;
}

static expr* x_parse_not(xparser* x);

static expr* x_parse_not_inner(xparser* x)
{
    if (x_is_name(x, "not")) {
        const size_t offset = x->tok.start;
        x_next(x);
        expr* operand = x_parse_not(x);
        if (operand == NULL) return NULL;
        expr* e = x_new(x, EX_UNARY, offset);
        if (e == NULL) return NULL;
        e->op = OP_NOT;
        e->a = operand;
        return x_seal(x, e);
    }
    return x_parse_comparison(x);
}

static expr* x_parse_not(xparser* x)
{
    if (!x_enter(x)) return NULL;
    expr* e = x_parse_not_inner(x);
    --x->depth;
    return e;
}

static expr* x_parse_and(xparser* x)
{
    expr* left = x_parse_not(x);
    while (left != NULL && x_is_name(x, "and")) {
        const size_t offset = x->tok.start;
        x_next(x);
        left = x_binary(x, OP_AND, offset, left, x_parse_not(x));
    }
    return left;
}

static expr* x_parse_or(xparser* x)
{
    expr* left = x_parse_and(x);
    while (left != NULL && x_is_name(x, "or")) {
        const size_t offset = x->tok.start;
        x_next(x);
        left = x_binary(x, OP_OR, offset, left, x_parse_and(x));
    }
    return left;
}

static expr* x_parse_expr_inner(xparser* x)
{
    expr* value_expr = x_parse_or(x);
    if (value_expr == NULL || !x_is_name(x, "if")) return value_expr;
    const size_t offset = x->tok.start;
    x_next(x);
    expr* condition = x_parse_or(x);
    if (condition == NULL) return NULL;
    expr* otherwise = NULL;
    if (x_is_name(x, "else")) {
        x_next(x);
        otherwise = x_parse_expr(x);
        if (otherwise == NULL) return NULL;
    }
    expr* e = x_new(x, EX_COND, offset);
    if (e == NULL) return NULL;
    e->a = condition;
    e->b = value_expr;
    e->c = otherwise;
    return x_seal(x, e);
}

static expr* x_parse_expr(xparser* x)
{
    if (!x_enter(x)) return NULL;
    expr* e = x_parse_expr_inner(x);
    --x->depth;
    return e;
}

static void x_init(xparser* x, parser* p, const size_t start, const size_t end)
{
    memset(x, 0, sizeof(*x));
    x->p = p;
    x->pos = start;
    x->end = end;
    x_next(x);
}

static expr* parse_expression_range(parser* p, const size_t start, const size_t end, clm_int* status)
{
    xparser x;
    x_init(&x, p, start, end);
    expr* e = x_parse_expr(&x);
    if (x.status != TPL_OK) e = NULL;
    else if (e != NULL && x.tok.type != TK_END) {
        x_fail(&x, x.tok.start, "unexpected '%.*s'", (int)x.tok.length, p->base + x.tok.start);
        e = NULL;
    }
    if (e == NULL && x.status == TPL_OK) x.status = p_fail(p, TPL_SYNTAX, start, "invalid expression");
    *status = x.status;
    return e;
}

/* ---- parser: template structure ------------------------------------------ */

typedef enum { END_EOF, END_ELIF, END_ELSE, END_ENDIF, END_ENDFOR } end_kind;

typedef struct {
    end_kind kind;
    size_t offset;
    size_t content_start;
    size_t content_end;
} parse_end;

static node* new_node(parser* p, const node_kind kind, const size_t offset)
{
    node* n = (node*)arena_alloc(p->arena, sizeof(node));
    if (n == NULL) return NULL;
    memset(n, 0, sizeof(*n));
    n->kind = kind;
    n->offset = offset;
    return n;
}

static bool find_delimiter(const parser* p, size_t* at, char* kind)
{
    for (size_t i = p->pos; i + 1 < p->length; ++i) {
        if (p->base[i] == '{' && (p->base[i + 1] == '{' || p->base[i + 1] == '%' || p->base[i + 1] == '#')) {
            *at = i;
            *kind = p->base[i + 1];
            return true;
        }
    }
    return false;
}

static bool find_tag_end(const parser* p, const size_t from, const char* close, size_t* out)
{
    char quote = 0;
    for (size_t i = from; i + 1 < p->length; ++i) {
        const char c = p->base[i];
        if (quote != 0) {
            if (c == '\\') ++i;
            else if (c == quote) quote = 0;
            continue;
        }
        if (c == '"' || c == '\'') {
            quote = c;
            continue;
        }
        if (c == close[0] && p->base[i + 1] == close[1]) {
            *out = i;
            return true;
        }
    }
    return false;
}

static void after_tag(parser* p, const bool right_strip, const bool block)
{
    if (right_strip) {
        p->strip_next = true;
    } else if (block && p->trim_blocks) {
        if (p->pos < p->length && p->base[p->pos] == '\n') ++p->pos;
        else if (p->pos + 1 < p->length && p->base[p->pos] == '\r' && p->base[p->pos + 1] == '\n') p->pos += 2;
    }
}

static clm_int parse_nodes(parser* p, node** list, parse_end* end);

static clm_int parse_if(parser* p, const size_t cond_start, const size_t cond_end, const size_t offset, node** out)
{
    clm_int status = TPL_OK;
    expr* condition = parse_expression_range(p, cond_start, cond_end, &status);
    if (condition == NULL) return status;
    node* n = new_node(p, ND_IF, offset);
    if (n == NULL) return TPL_MEMORY;
    n->expression = condition;
    *out = n;
    parse_end end;
    status = parse_nodes(p, &n->body, &end);
    if (status != TPL_OK) return status;
    if (end.kind == END_ELIF) return parse_if(p, end.content_start, end.content_end, end.offset, &n->else_body);
    if (end.kind == END_ELSE) {
        status = parse_nodes(p, &n->else_body, &end);
        if (status != TPL_OK) return status;
    }
    if (end.kind != END_ENDIF) return p_fail(p, TPL_SYNTAX, offset, "'if' block is not closed with 'endif'");
    return TPL_OK;
}

static clm_int parse_for(parser* p, const size_t start, const size_t end_pos, const size_t offset, node** out)
{
    xparser x;
    x_init(&x, p, start, end_pos);
    if (x.tok.type != TK_NAME || is_reserved(x.tok.text, x.tok.length)) {
        x_fail(&x, x.tok.start, "expected a loop variable name");
        return x.status != TPL_OK ? x.status : TPL_SYNTAX;
    }
    const token variable = x.tok;
    x_next(&x);
    if (!x_is_name(&x, "in")) {
        x_fail(&x, x.tok.start, "expected 'in' in the for statement");
        return x.status;
    }
    x_next(&x);
    expr* iterable = x_parse_expr(&x);
    if (x.status != TPL_OK) return x.status;
    if (iterable != NULL && x.tok.type != TK_END) {
        x_fail(&x, x.tok.start, "unexpected '%.*s'", (int)x.tok.length, p->base + x.tok.start);
        iterable = NULL;
    }
    if (iterable == NULL) return x.status != TPL_OK ? x.status : TPL_SYNTAX;
    node* n = new_node(p, ND_FOR, offset);
    if (n == NULL) return TPL_MEMORY;
    n->name = variable.text;
    n->name_length = variable.length;
    n->expression = iterable;
    *out = n;
    parse_end end;
    clm_int status = parse_nodes(p, &n->body, &end);
    if (status != TPL_OK) return status;
    if (end.kind == END_ELSE) {
        status = parse_nodes(p, &n->else_body, &end);
        if (status != TPL_OK) return status;
    }
    if (end.kind != END_ENDFOR) return p_fail(p, TPL_SYNTAX, offset, "'for' block is not closed with 'endfor'");
    return TPL_OK;
}

static clm_int parse_set(parser* p, const size_t start, const size_t end_pos, const size_t offset, node** out)
{
    xparser x;
    x_init(&x, p, start, end_pos);
    if (x.tok.type != TK_NAME || is_reserved(x.tok.text, x.tok.length)) {
        x_fail(&x, x.tok.start, "expected a variable name");
        return x.status != TPL_OK ? x.status : TPL_SYNTAX;
    }
    const token variable = x.tok;
    x_next(&x);
    if (!x_is_op(&x, "=")) {
        x_fail(&x, x.tok.start, "expected '=' in the set statement");
        return x.status;
    }
    x_next(&x);
    expr* assigned = x_parse_expr(&x);
    if (x.status != TPL_OK) return x.status;
    if (assigned != NULL && x.tok.type != TK_END) {
        x_fail(&x, x.tok.start, "unexpected '%.*s'", (int)x.tok.length, p->base + x.tok.start);
        assigned = NULL;
    }
    if (assigned == NULL) return x.status != TPL_OK ? x.status : TPL_SYNTAX;
    node* n = new_node(p, ND_SET, offset);
    if (n == NULL) return TPL_MEMORY;
    n->name = variable.text;
    n->name_length = variable.length;
    n->expression = assigned;
    *out = n;
    return TPL_OK;
}

static clm_int parse_raw(parser* p, node*** tail, const size_t offset)
{
    size_t scan = p->pos;
    while (scan + 1 < p->length) {
        size_t open = scan;
        while (open + 1 < p->length && !(p->base[open] == '{' && p->base[open + 1] == '%')) ++open;
        if (open + 1 >= p->length) break;
        size_t k = open + 2;
        const bool left_strip = k < p->length && p->base[k] == '-';
        if (k < p->length && (p->base[k] == '-' || p->base[k] == '+')) ++k;
        while (k < p->length && is_space(p->base[k])) ++k;
        if (p->length - k >= 6 && memcmp(p->base + k, "endraw", 6) == 0) {
            k += 6;
            while (k < p->length && is_space(p->base[k])) ++k;
            const bool right_strip = k < p->length && p->base[k] == '-';
            if (right_strip) ++k;
            if (k + 1 < p->length && p->base[k] == '%' && p->base[k + 1] == '}') {
                size_t text_end = open;
                if (left_strip) while (text_end > p->pos && is_space(p->base[text_end - 1])) --text_end;
                if (text_end > p->pos) {
                    node* n = new_node(p, ND_TEXT, offset);
                    if (n == NULL) return TPL_MEMORY;
                    n->text = p->base + p->pos;
                    n->length = text_end - p->pos;
                    **tail = n;
                    *tail = &n->next;
                }
                p->pos = k + 2;
                if (right_strip) p->strip_next = true;
                return TPL_OK;
            }
        }
        scan = open + 2;
    }
    return p_fail(p, TPL_SYNTAX, offset, "'raw' block is not closed with 'endraw'");
}

static bool only_space(const parser* p, size_t start, const size_t end)
{
    while (start < end) {
        if (!is_space(p->base[start])) return false;
        ++start;
    }
    return true;
}

static clm_int parse_nodes_inner(parser* p, node** list, parse_end* end)
{
    node** tail = list;
    *list = NULL;
    for (;;) {
        size_t at = 0;
        char kind = 0;
        const bool found = find_delimiter(p, &at, &kind);
        size_t text_start = p->pos;
        size_t text_end = found ? at : p->length;
        const char marker = found && at + 2 < p->length ? p->base[at + 2] : 0;
        if (p->strip_next) {
            while (text_start < text_end && is_space(p->base[text_start])) ++text_start;
            p->strip_next = false;
        }
        if (found && marker == '-') {
            while (text_end > text_start && is_space(p->base[text_end - 1])) --text_end;
        } else if (found && marker != '+' && p->lstrip_blocks && kind != '{') {
            size_t k = text_end;
            while (k > text_start && (p->base[k - 1] == ' ' || p->base[k - 1] == '\t')) --k;
            const bool at_line_start = k == text_start ? (text_start == 0 || p->base[text_start - 1] == '\n')
                                                       : p->base[k - 1] == '\n';
            if (at_line_start) text_end = k;
        }
        if (text_end > text_start) {
            node* n = new_node(p, ND_TEXT, text_start);
            if (n == NULL) return TPL_MEMORY;
            n->text = p->base + text_start;
            n->length = text_end - text_start;
            *tail = n;
            tail = &n->next;
        }
        if (!found) {
            p->pos = p->length;
            end->kind = END_EOF;
            end->offset = p->length;
            return TPL_OK;
        }

        if (kind == '#') {
            size_t close = at + 2;
            bool closed = false;
            while (close + 1 < p->length) {
                if (p->base[close] == '#' && p->base[close + 1] == '}') {
                    closed = true;
                    break;
                }
                ++close;
            }
            if (!closed) return p_fail(p, TPL_SYNTAX, at, "comment is not closed with '#}'");
            const bool right_strip = close > at + 2 && p->base[close - 1] == '-';
            p->pos = close + 2;
            after_tag(p, right_strip, true);
            continue;
        }

        const size_t content = at + 2 + (marker == '-' || (kind == '%' && marker == '+') ? 1 : 0);
        size_t close = 0;
        if (!find_tag_end(p, content, kind == '{' ? "}}" : "%}", &close))
            return p_fail(p, TPL_SYNTAX, at, kind == '{' ? "'{{' is not closed with '}}'" : "'{%%' is not closed with '%%}'");
        const bool right_strip = close > content && p->base[close - 1] == '-';
        const size_t content_end = right_strip ? close - 1 : close;

        if (kind == '{') {
            clm_int status = TPL_OK;
            expr* printed = parse_expression_range(p, content, content_end, &status);
            if (printed == NULL) return status;
            node* n = new_node(p, ND_OUTPUT, at);
            if (n == NULL) return TPL_MEMORY;
            n->expression = printed;
            *tail = n;
            tail = &n->next;
            p->pos = close + 2;
            after_tag(p, right_strip, false);
            continue;
        }

        size_t word_start = content;
        while (word_start < content_end && is_space(p->base[word_start])) ++word_start;
        size_t word_end = word_start;
        while (word_end < content_end && is_name_char(p->base[word_end])) ++word_end;
        const size_t word_length = word_end - word_start;
        const char* word = p->base + word_start;
        p->pos = close + 2;
        after_tag(p, right_strip, true);

        if (word_length == 2 && memcmp(word, "if", 2) == 0) {
            clm_int status = parse_if(p, word_end, content_end, at, tail);
            if (status != TPL_OK) return status;
            tail = &(*tail)->next;
        } else if (word_length == 3 && memcmp(word, "for", 3) == 0) {
            clm_int status = parse_for(p, word_end, content_end, at, tail);
            if (status != TPL_OK) return status;
            tail = &(*tail)->next;
        } else if (word_length == 3 && memcmp(word, "set", 3) == 0) {
            clm_int status = parse_set(p, word_end, content_end, at, tail);
            if (status != TPL_OK) return status;
            tail = &(*tail)->next;
        } else if (word_length == 3 && memcmp(word, "raw", 3) == 0) {
            if (!only_space(p, word_end, content_end)) return p_fail(p, TPL_SYNTAX, word_end, "unexpected text after 'raw'");
            clm_int status = parse_raw(p, &tail, at);
            if (status != TPL_OK) return status;
        } else if ((word_length == 4 && memcmp(word, "elif", 4) == 0) || (word_length == 4 && memcmp(word, "else", 4) == 0) ||
                   (word_length == 5 && memcmp(word, "endif", 5) == 0) || (word_length == 6 && memcmp(word, "endfor", 6) == 0)) {
            const bool is_elif = word_length == 4 && word[2] == 'i';
            if (!is_elif && !only_space(p, word_end, content_end))
                return p_fail(p, TPL_SYNTAX, word_end, "unexpected text after '%.*s'", (int)word_length, word);
            end->kind = is_elif ? END_ELIF : word_length == 4 ? END_ELSE : word_length == 5 ? END_ENDIF : END_ENDFOR;
            end->offset = at;
            end->content_start = word_end;
            end->content_end = content_end;
            return TPL_OK;
        } else {
            return p_fail(p, TPL_SYNTAX, word_start, "unknown tag '%.*s'", (int)word_length, word);
        }
    }
}

static clm_int parse_nodes(parser* p, node** list, parse_end* end)
{
    if (p->depth >= MAX_STATEMENT_DEPTH) return p_fail(p, TPL_SYNTAX, p->pos, "blocks are nested too deeply");
    ++p->depth;
    const clm_int status = parse_nodes_inner(p, list, end);
    --p->depth;
    return status;
}

/* ---- public: compile ------------------------------------------------------ */

clm_int clm_template_compile(
    clm_template* tmpl, const char* source, const size_t length,
    const clm_template_syntax* syntax, clm_template_error* error)
{
    clear_error(error);
    if (tmpl == NULL || (source == NULL && length != 0)) return CLM_TEMPLATE_INVALID_ARGUMENT;
    struct clm_template_impl* impl = (struct clm_template_impl*)calloc(1, sizeof(*impl));
    if (impl == NULL) return TPL_MEMORY;
    impl->source = arena_copy(&impl->arena, source, length);
    if (impl->source == NULL) {
        arena_free(&impl->arena);
        free(impl);
        return TPL_MEMORY;
    }
    impl->length = length;

    parser p;
    memset(&p, 0, sizeof(p));
    p.arena = &impl->arena;
    p.base = impl->source;
    p.length = length;
    p.error = error;
    if (syntax != NULL) {
        p.trim_blocks = syntax->trim_blocks;
        p.lstrip_blocks = syntax->lstrip_blocks;
    }
    parse_end end;
    clm_int status = parse_nodes(&p, &impl->root, &end);
    if (status == TPL_OK && end.kind != END_EOF) {
        static const char* const names[] = {"", "elif", "else", "endif", "endfor"};
        status = p_fail(&p, TPL_SYNTAX, end.offset, "unexpected '%s'", names[end.kind]);
    }
    if (status != TPL_OK) {
        if (status == TPL_MEMORY) fill_error(error, status, source, length, 0, "out of memory");
        arena_free(&impl->arena);
        free(impl);
        return status;
    }
    tmpl->impl = impl;
    return TPL_OK;
}

clm_int clm_template_compile_string_id(
    clm_template* tmpl, const clm_id string_id,
    const clm_template_syntax* syntax, clm_template_error* error)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(string_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || (block->length != 0 && block->data == NULL)) {
        clear_error(error);
        return CLM_TEMPLATE_INVALID_ARGUMENT;
    }
    return clm_template_compile(tmpl, (const char*)block->data, block->length, syntax, error);
}

void clm_template_dispose(clm_template* tmpl)
{
    if (tmpl == NULL || tmpl->impl == NULL) return;
    arena_free(&tmpl->impl->arena);
    free(tmpl->impl);
    tmpl->impl = NULL;
}

/* ---- public: context ------------------------------------------------------ */

typedef enum { CV_NULL, CV_BOOL, CV_INT, CV_DOUBLE, CV_STRING, CV_TREE } context_kind;

struct clm_template_variable {
    char* name;
    context_kind kind;
    union {
        bool boolean;
        int64_t integer;
        double real;
        clm_id tree;
    } u;
    char* text;
    size_t length;
};

clm_int clm_template_context_init(clm_template_context* context)
{
    if (context == NULL) return CLM_TEMPLATE_INVALID_ARGUMENT;
    memset(context, 0, sizeof(*context));
    return TPL_OK;
}

static void variable_release(struct clm_template_variable* variable)
{
    free(variable->name);
    free(variable->text);
}

void clm_template_context_dispose(clm_template_context* context)
{
    if (context == NULL) return;
    for (size_t i = 0; i < context->count; ++i) variable_release(&context->variables[i]);
    free(context->variables);
    memset(context, 0, sizeof(*context));
}

static bool valid_identifier(const char* name)
{
    if (name == NULL || !is_name_start(name[0])) return false;
    for (size_t i = 1; name[i] != '\0'; ++i)
        if (!is_name_char(name[i])) return false;
    return true;
}

static struct clm_template_variable* context_slot(clm_template_context* context, const char* name, clm_int* status)
{
    *status = CLM_TEMPLATE_INVALID_ARGUMENT;
    if (context == NULL || !valid_identifier(name)) return NULL;
    for (size_t i = 0; i < context->count; ++i) {
        if (strcmp(context->variables[i].name, name) == 0) {
            free(context->variables[i].text);
            context->variables[i].text = NULL;
            context->variables[i].length = 0;
            *status = TPL_OK;
            return &context->variables[i];
        }
    }
    *status = TPL_MEMORY;
    if (context->count == context->capacity) {
        if (context->capacity > SIZE_MAX / 2 / sizeof(*context->variables))
            return NULL;
        const size_t capacity = context->capacity != 0 ? context->capacity * 2 : 8;
        struct clm_template_variable* grown = (struct clm_template_variable*)realloc(
            context->variables, capacity * sizeof(*grown));
        if (grown == NULL) return NULL;
        context->variables = grown;
        context->capacity = capacity;
    }
    struct clm_template_variable* slot = &context->variables[context->count];
    memset(slot, 0, sizeof(*slot));
    const size_t name_size = strlen(name) + 1;
    slot->name = (char*)malloc(name_size);
    if (slot->name == NULL) return NULL;
    memcpy(slot->name, name, name_size);
    ++context->count;
    *status = TPL_OK;
    return slot;
}

static clm_int context_set_text(
    clm_template_context* context, const char* name, const char* text, const size_t length)
{
    clm_int status;
    struct clm_template_variable* slot = context_slot(context, name, &status);
    if (slot == NULL) return status;
    slot->text = (char*)malloc(length + 1);
    if (slot->text == NULL) {
        slot->kind = CV_NULL;
        return TPL_MEMORY;
    }
    if (length != 0) memcpy(slot->text, text, length);
    slot->text[length] = '\0';
    slot->length = length;
    slot->kind = CV_STRING;
    return TPL_OK;
}

clm_int clm_template_context_set_string(clm_template_context* context, const char* name, const char* value)
{
    if (value == NULL) return CLM_TEMPLATE_INVALID_ARGUMENT;
    return context_set_text(context, name, value, strlen(value));
}

clm_int clm_template_context_set_string_id(clm_template_context* context, const char* name, const clm_id string_id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(string_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || (block->length != 0 && block->data == NULL))
        return CLM_TEMPLATE_INVALID_ARGUMENT;
    return context_set_text(context, name, (const char*)block->data, block->length);
}

clm_int clm_template_context_set_int(clm_template_context* context, const char* name, const int64_t value)
{
    clm_int status;
    struct clm_template_variable* slot = context_slot(context, name, &status);
    if (slot == NULL) return status;
    slot->kind = CV_INT;
    slot->u.integer = value;
    return TPL_OK;
}

clm_int clm_template_context_set_double(clm_template_context* context, const char* name, const double value)
{
    clm_int status;
    struct clm_template_variable* slot = context_slot(context, name, &status);
    if (slot == NULL) return status;
    slot->kind = CV_DOUBLE;
    slot->u.real = value;
    return TPL_OK;
}

clm_int clm_template_context_set_bool(clm_template_context* context, const char* name, const bool value)
{
    clm_int status;
    struct clm_template_variable* slot = context_slot(context, name, &status);
    if (slot == NULL) return status;
    slot->kind = CV_BOOL;
    slot->u.boolean = value;
    return TPL_OK;
}

clm_int clm_template_context_set_null(clm_template_context* context, const char* name)
{
    clm_int status;
    struct clm_template_variable* slot = context_slot(context, name, &status);
    if (slot == NULL) return status;
    slot->kind = CV_NULL;
    return TPL_OK;
}

clm_int clm_template_context_set_tree(clm_template_context* context, const char* name, const clm_id tree_id)
{
    clm_int status;
    struct clm_template_variable* slot = context_slot(context, name, &status);
    if (slot == NULL) return status;
    slot->kind = CV_TREE;
    slot->u.tree = tree_id;
    return TPL_OK;
}

clm_int clm_template_context_set_root(clm_template_context* context, const clm_id tree_object_id)
{
    if (context == NULL || !clm_tree_is_object(tree_object_id)) return CLM_TEMPLATE_INVALID_ARGUMENT;
    context->has_root = true;
    context->root = tree_object_id;
    return TPL_OK;
}

/* ---- rendering: state ----------------------------------------------------- */

typedef struct {
    const char* name;
    size_t name_length;
    value val;
    char* owned;
} scope_var;

typedef struct {
    const struct clm_template_impl* impl;
    const clm_template_context* context;
    bool autoescape;
    bool strict;
    size_t max_output;
    size_t max_iterations;
    clm_bytes_buffer* out;
    size_t produced;
    size_t iterations;
    tpl_arena scratch;
    scope_var* scope;
    size_t scope_count;
    size_t scope_capacity;
    size_t frame_base;
    clm_int status;
    size_t error_offset;
    char message[128];
} render;

static clm_int r_fail(render* r, const clm_int code, const size_t offset, const char* format, ...)
{
    if (r->status == TPL_OK) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(r->message, sizeof(r->message), format, arguments);
        va_end(arguments);
        r->status = code;
        r->error_offset = offset;
    }
    return code;
}

static char* scratch_alloc(render* r, const size_t length, const size_t offset)
{
    if (length >= SIZE_MAX || length > r->max_output) {
        r_fail(r, TPL_LIMIT, offset, "temporary result exceeds the output limit");
        return NULL;
    }
    char* memory = (char*)arena_alloc(&r->scratch, length + 1);
    if (memory == NULL) r_fail(r, TPL_MEMORY, offset, "out of memory");
    return memory;
}

static value scratch_text(render* r, const char* data, const size_t length, const bool safe, const size_t offset)
{
    char* copy = scratch_alloc(r, length, offset);
    if (copy == NULL) return v_undef();
    if (length != 0) memcpy(copy, data, length);
    copy[length] = '\0';
    return v_text(copy, length, safe);
}

/* All temporary filter buffers are bounded by the configured render output
 * limit. Check append failures instead of silently emitting truncated text. */
static bool buffer_append_checked(
    render* r, clm_bytes_buffer* buffer, const void* data,
    const size_t length, const size_t offset)
{
    if (r->status != TPL_OK) return false;
    if (length > r->max_output - clm_bytes_buffer_length(buffer)) {
        r_fail(r, TPL_LIMIT, offset, "temporary result exceeds the output limit");
        return false;
    }
    const clm_int status = clm_bytes_buffer_append(buffer, data, length);
    if (status != CLM_BYTES_SUCCESS) {
        r_fail(r, status == CLM_BYTES_LIMIT_EXCEEDED ? TPL_LIMIT : TPL_MEMORY,
            offset, "cannot grow a temporary buffer");
        return false;
    }
    return true;
}

static bool buffer_append_byte_checked(
    render* r, clm_bytes_buffer* buffer, const unsigned char byte, const size_t offset)
{
    return buffer_append_checked(r, buffer, &byte, 1, offset);
}

static bool out_put(render* r, const char* data, const size_t length, const size_t offset)
{
    if (r->status != TPL_OK) return false;
    if (length > r->max_output - r->produced) {
        r_fail(r, TPL_LIMIT, offset, "output is larger than the limit of %zu bytes", r->max_output);
        return false;
    }
    const clm_int status = clm_bytes_buffer_append(r->out, data, length);
    if (status != CLM_BYTES_SUCCESS) {
        r_fail(r, status == CLM_BYTES_LIMIT_EXCEEDED ? TPL_LIMIT : TPL_MEMORY, offset, "cannot grow the output buffer");
        return false;
    }
    r->produced += length;
    return true;
}

static bool out_put_escaped(render* r, const char* data, const size_t length, const size_t offset)
{
    size_t run = 0;
    for (size_t i = 0; i < length; ++i) {
        const char* replacement = NULL;
        switch (data[i]) {
        case '&': replacement = "&amp;"; break;
        case '<': replacement = "&lt;"; break;
        case '>': replacement = "&gt;"; break;
        case '"': replacement = "&#34;"; break;
        case '\'': replacement = "&#39;"; break;
        default: break;
        }
        if (replacement != NULL) {
            if (!out_put(r, data + run, i - run, offset) || !out_put(r, replacement, strlen(replacement), offset))
                return false;
            run = i + 1;
        }
    }
    return out_put(r, data + run, length - run, offset);
}

static bool scope_push(render* r, const char* name, const size_t name_length, const value val, char* owned)
{
    if (r->scope_count == r->scope_capacity) {
        const size_t capacity = r->scope_capacity != 0 ? r->scope_capacity * 2 : 16;
        scope_var* grown = (scope_var*)realloc(r->scope, capacity * sizeof(*grown));
        if (grown == NULL) {
            free(owned);
            r_fail(r, TPL_MEMORY, 0, "out of memory");
            return false;
        }
        r->scope = grown;
        r->scope_capacity = capacity;
    }
    scope_var* slot = &r->scope[r->scope_count++];
    slot->name = name;
    slot->name_length = name_length;
    slot->val = val;
    slot->owned = owned;
    return true;
}

static void scope_truncate(render* r, const size_t count)
{
    while (r->scope_count > count) free(r->scope[--r->scope_count].owned);
}

/* ---- rendering: values from trees and context ------------------------------ */

static value node_value(const clm_id id)
{
    if (clm_tree_is_object(id) || clm_tree_is_array(id)) return v_tree(id);
    bool flag = false;
    if (clm_tree_is_boolean(id, &flag)) return v_bool(flag);
    if (clm_tree_is_null(id)) return v_none();
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->data == NULL) return v_undef();
    if (block->type == CLM_BPA_TYPE_STRING) return v_text((const char*)block->data, block->length, false);
    switch (block->type) {
    case CLM_BPA_TYPE_SIGNED_BYTE: if (block->block_size == 1) { int8_t x; memcpy(&x, block->data, 1); return v_int(x); } break;
    case CLM_BPA_TYPE_BYTE: if (block->block_size == 1) { uint8_t x; memcpy(&x, block->data, 1); return v_int(x); } break;
    case CLM_BPA_TYPE_SHORT: if (block->block_size == 2) { int16_t x; memcpy(&x, block->data, 2); return v_int(x); } break;
    case CLM_BPA_TYPE_UNSIGNED_SHORT: if (block->block_size == 2) { uint16_t x; memcpy(&x, block->data, 2); return v_int(x); } break;
    case CLM_BPA_TYPE_INT: if (block->block_size == 4) { int32_t x; memcpy(&x, block->data, 4); return v_int(x); } break;
    case CLM_BPA_TYPE_UNSIGNED_INT: if (block->block_size == 4) { uint32_t x; memcpy(&x, block->data, 4); return v_int(x); } break;
    case CLM_BPA_TYPE_LONG: if (block->block_size == 8) { int64_t x; memcpy(&x, block->data, 8); return v_int(x); } break;
    case CLM_BPA_TYPE_UNSIGNED_LONG:
        if (block->block_size == 8) {
            uint64_t x;
            memcpy(&x, block->data, 8);
            return x <= (uint64_t)INT64_MAX ? v_int((int64_t)x) : v_double((double)x);
        }
        break;
    case CLM_BPA_TYPE_FLOAT: if (block->block_size == 4) { float x; memcpy(&x, block->data, 4); return v_double(x); } break;
    case CLM_BPA_TYPE_DOUBLE: if (block->block_size == 8) { double x; memcpy(&x, block->data, 8); return v_double(x); } break;
    default: break;
    }
    return v_undef();
}

static value tree_member(const clm_id object, const char* name, const size_t length)
{
    char buffer[256];
    if (length == 0 || length >= sizeof(buffer) || !clm_tree_is_object(object)) return v_undef();
    memcpy(buffer, name, length);
    buffer[length] = '\0';
    clm_id key = 0;
    clm_id child = 0;
    if (clm_id_try_get(buffer, &key) != CLM_BPA_GET_SUCCESS ||
        clm_tree_get_id(object, key, &child) != CLM_BPA_GET_SUCCESS)
        return v_undef();
    return node_value(child);
}

static value tree_element(const clm_id array, const int64_t index)
{
    const clm_int count = clm_tree_array_count(array);
    if (count < 0) return v_undef();
    int64_t position = index;
    if (position < 0) position += count;
    if (position < 0 || position >= count) return v_undef();
    clm_id child = 0;
    if (clm_tree_array_get_id(array, (clm_int)position, &child) != CLM_BPA_GET_SUCCESS) return v_undef();
    return node_value(child);
}

static value lookup_variable(const render* r, const char* name, const size_t length)
{
    for (size_t i = r->scope_count; i-- > 0;)
        if (r->scope[i].name_length == length && memcmp(r->scope[i].name, name, length) == 0)
            return r->scope[i].val;
    const clm_template_context* context = r->context;
    if (context == NULL) return v_undef();
    for (size_t i = 0; i < context->count; ++i) {
        const struct clm_template_variable* variable = &context->variables[i];
        if (strlen(variable->name) != length || memcmp(variable->name, name, length) != 0) continue;
        switch (variable->kind) {
        case CV_NULL: return v_none();
        case CV_BOOL: return v_bool(variable->u.boolean);
        case CV_INT: return v_int(variable->u.integer);
        case CV_DOUBLE: return v_double(variable->u.real);
        case CV_STRING: return v_text(variable->text, variable->length, false);
        case CV_TREE: return node_value(variable->u.tree);
        }
    }
    return context->has_root ? tree_member(context->root, name, length) : v_undef();
}

/* ---- rendering: conversions ------------------------------------------------ */

static bool is_number(const value v) { return v.kind == V_INT || v.kind == V_DOUBLE || v.kind == V_BOOL; }

static double as_double(const value v)
{
    return v.kind == V_DOUBLE ? v.u.real : v.kind == V_BOOL ? (v.u.boolean ? 1.0 : 0.0) : (double)v.u.integer;
}

static int64_t as_int(const value v) { return v.kind == V_BOOL ? (v.u.boolean ? 1 : 0) : v.u.integer; }

static bool truthy(const value v)
{
    switch (v.kind) {
    case V_UNDEF:
    case V_NONE: return false;
    case V_BOOL: return v.u.boolean;
    case V_INT: return v.u.integer != 0;
    case V_DOUBLE: return v.u.real != 0.0;
    case V_STRING: return v.u.text.length != 0;
    case V_TREE:
        return clm_tree_is_array(v.u.tree) ? clm_tree_array_count(v.u.tree) > 0 : clm_tree_count(v.u.tree) > 0;
    default: return true;
    }
}

static void format_double(const double d, char* buffer, const size_t capacity)
{
    if (isnan(d)) {
        snprintf(buffer, capacity, "nan");
        return;
    }
    if (isinf(d)) {
        snprintf(buffer, capacity, d < 0 ? "-inf" : "inf");
        return;
    }
    for (int precision = 1; precision <= 17; ++precision) {
        snprintf(buffer, capacity, "%.*g", precision, d);
        if (strtod(buffer, NULL) == d) break;
    }
    if (strpbrk(buffer, ".eE") == NULL) {
        const size_t used = strlen(buffer);
        snprintf(buffer + used, capacity - used, ".0");
    }
}

static bool defined_or_fail(render* r, const value v, const expr* e)
{
    if (v.kind != V_UNDEF || !r->strict) return true;
    if (e != NULL && (e->kind == EX_VAR || e->kind == EX_ATTR))
        r_fail(r, TPL_RENDER, e->offset, "'%.*s' is undefined", (int)e->name_length, e->name);
    else
        r_fail(r, TPL_RENDER, e != NULL ? e->offset : 0, "undefined value");
    return false;
}

/* Gives the text form of v; the result lives until the scratch arena is reset. */
static bool value_text(render* r, const value v, const expr* e, const char** text, size_t* length)
{
    char number[64];
    const size_t offset = e != NULL ? e->offset : 0;
    switch (v.kind) {
    case V_UNDEF:
        if (!defined_or_fail(r, v, e)) return false;
        *text = "";
        *length = 0;
        return true;
    case V_NONE: *text = "None"; *length = 4; return true;
    case V_BOOL: *text = v.u.boolean ? "True" : "False"; *length = strlen(*text); return true;
    case V_STRING: *text = v.u.text.data; *length = v.u.text.length; return true;
    case V_INT:
        snprintf(number, sizeof(number), "%lld", (long long)v.u.integer);
        break;
    case V_DOUBLE:
        format_double(v.u.real, number, sizeof(number));
        break;
    case V_TREE: {
        size_t needed = 0;
        if (clm_stringtify_tree_to_string_with_options(v.u.tree, NULL, NULL, 0, &needed) != CLM_STRINGTIFY_SUCCESS || needed == 0) {
            r_fail(r, TPL_RENDER, offset, "cannot print this value");
            return false;
        }
        char* buffer = scratch_alloc(r, needed, offset);
        if (buffer == NULL) return false;
        if (clm_stringtify_tree_to_string_with_options(v.u.tree, NULL, buffer, needed, &needed) != CLM_STRINGTIFY_SUCCESS) {
            r_fail(r, TPL_RENDER, offset, "cannot print this value");
            return false;
        }
        *text = buffer;
        *length = needed - 1;
        return true;
    }
    default:
        r_fail(r, TPL_RENDER, offset, "cannot print a loop object");
        return false;
    }
    const value copy = scratch_text(r, number, strlen(number), false, offset);
    if (r->status != TPL_OK) return false;
    *text = copy.u.text.data;
    *length = copy.u.text.length;
    return true;
}

static size_t utf8_count(const char* s, const size_t n)
{
    size_t count = 0;
    for (size_t i = 0; i < n; ++i)
        if (((unsigned char)s[i] & 0xC0u) != 0x80u) ++count;
    return count;
}

static size_t utf8_offset(const char* s, const size_t n, size_t characters)
{
    size_t i = 0;
    while (i < n) {
        if (((unsigned char)s[i] & 0xC0u) != 0x80u) {
            if (characters == 0) return i;
            --characters;
        }
        ++i;
    }
    return n;
}

/* ---- rendering: comparison and arithmetic ---------------------------------- */

static bool values_equal(const value a, const value b)
{
    if (is_number(a) && is_number(b)) {
        if (a.kind != V_DOUBLE && b.kind != V_DOUBLE) return as_int(a) == as_int(b);
        return as_double(a) == as_double(b);
    }
    if (a.kind != b.kind) return false;
    switch (a.kind) {
    case V_STRING:
        return a.u.text.length == b.u.text.length &&
            (a.u.text.length == 0 || memcmp(a.u.text.data, b.u.text.data, a.u.text.length) == 0);
    case V_TREE: return a.u.tree == b.u.tree;
    default: return a.kind == V_NONE || a.kind == V_UNDEF;
    }
}

static bool values_order(const value a, const value b, int* order)
{
    if (is_number(a) && is_number(b)) {
        if (a.kind != V_DOUBLE && b.kind != V_DOUBLE) {
            const int64_t x = as_int(a);
            const int64_t y = as_int(b);
            *order = x < y ? -1 : x > y ? 1 : 0;
        } else {
            const double x = as_double(a);
            const double y = as_double(b);
            *order = x < y ? -1 : x > y ? 1 : 0;
        }
        return true;
    }
    if (a.kind == V_STRING && b.kind == V_STRING) {
        const size_t shared = a.u.text.length < b.u.text.length ? a.u.text.length : b.u.text.length;
        const int c = shared != 0 ? memcmp(a.u.text.data, b.u.text.data, shared) : 0;
        *order = c != 0 ? (c < 0 ? -1 : 1) : a.u.text.length < b.u.text.length ? -1 : a.u.text.length > b.u.text.length ? 1 : 0;
        return true;
    }
    return false;
}

static bool add_overflows(const int64_t a, const int64_t b) { return (b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b); }
static bool sub_overflows(const int64_t a, const int64_t b) { return (b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b); }

static bool mul_overflows(const int64_t a, const int64_t b)
{
    if (a == 0 || b == 0) return false;
    if (a == -1) return b == INT64_MIN;
    if (b == -1) return a == INT64_MIN;
    if (a > 0) return b > 0 ? a > INT64_MAX / b : b < INT64_MIN / a;
    return b > 0 ? a < INT64_MIN / b : b < INT64_MAX / a;
}

static value int_power(const int64_t base, const int64_t exponent)
{
    if (exponent < 0) return v_double(pow((double)base, (double)exponent));
    int64_t result = 1;
    int64_t factor = base;
    int64_t remaining = exponent;
    while (remaining > 0) {
        if (remaining & 1) {
            if (mul_overflows(result, factor)) return v_double(pow((double)base, (double)exponent));
            result *= factor;
        }
        remaining >>= 1;
        if (remaining > 0) {
            if (mul_overflows(factor, factor)) return v_double(pow((double)base, (double)exponent));
            factor *= factor;
        }
    }
    return v_int(result);
}

static value arithmetic(render* r, const expr* e, const value a, const value b)
{
    if (e->op == OP_ADD && a.kind == V_STRING && b.kind == V_STRING) {
        if (b.u.text.length > SIZE_MAX - a.u.text.length ||
            a.u.text.length + b.u.text.length > r->max_output) {
            r_fail(r, TPL_LIMIT, e->offset, "concatenated result exceeds the output limit");
            return v_undef();
        }
        const size_t length = a.u.text.length + b.u.text.length;
        char* joined = scratch_alloc(r, length, e->offset);
        if (joined == NULL) return v_undef();
        memcpy(joined, a.u.text.data, a.u.text.length);
        memcpy(joined + a.u.text.length, b.u.text.data, b.u.text.length);
        joined[length] = '\0';
        return v_text(joined, length, a.safe && b.safe);
    }
    if (!is_number(a) || !is_number(b)) {
        r_fail(r, TPL_RENDER, e->offset, "unsupported operand types for the operator");
        return v_undef();
    }
    const bool integral = a.kind != V_DOUBLE && b.kind != V_DOUBLE;
    switch (e->op) {
    case OP_ADD:
        if (integral && !add_overflows(as_int(a), as_int(b))) return v_int(as_int(a) + as_int(b));
        return v_double(as_double(a) + as_double(b));
    case OP_SUB:
        if (integral && !sub_overflows(as_int(a), as_int(b))) return v_int(as_int(a) - as_int(b));
        return v_double(as_double(a) - as_double(b));
    case OP_MUL:
        if (integral && !mul_overflows(as_int(a), as_int(b))) return v_int(as_int(a) * as_int(b));
        return v_double(as_double(a) * as_double(b));
    case OP_DIV:
        if (as_double(b) == 0.0) {
            r_fail(r, TPL_RENDER, e->offset, "division by zero");
            return v_undef();
        }
        return v_double(as_double(a) / as_double(b));
    case OP_FLOORDIV:
    case OP_MOD:
        if (as_double(b) == 0.0) {
            r_fail(r, TPL_RENDER, e->offset, "division by zero");
            return v_undef();
        }
        if (integral) {
            const int64_t x = as_int(a);
            const int64_t y = as_int(b);
            if (y == -1) return e->op == OP_MOD ? v_int(0) : (x == INT64_MIN ? v_double(-(double)x) : v_int(-x));
            int64_t quotient = x / y;
            int64_t remainder = x % y;
            if (remainder != 0 && ((remainder < 0) != (y < 0))) {
                --quotient;
                remainder += y;
            }
            return e->op == OP_MOD ? v_int(remainder) : v_int(quotient);
        } else {
            const double x = as_double(a);
            const double y = as_double(b);
            return e->op == OP_MOD ? v_double(x - y * floor(x / y)) : v_double(floor(x / y));
        }
    case OP_POW:
        return integral ? int_power(as_int(a), as_int(b)) : v_double(pow(as_double(a), as_double(b)));
    default:
        r_fail(r, TPL_RENDER, e->offset, "unsupported operator");
        return v_undef();
    }
}

static bool contains_value(render* r, const value needle, const value haystack)
{
    if (haystack.kind == V_STRING && needle.kind == V_STRING) {
        const size_t n = needle.u.text.length;
        const size_t h = haystack.u.text.length;
        if (n == 0) return true;
        if (n > h) return false;
        for (size_t i = 0; i + n <= h; ++i)
            if (memcmp(haystack.u.text.data + i, needle.u.text.data, n) == 0) return true;
        return false;
    }
    if (haystack.kind != V_TREE) return false;
    if (clm_tree_is_array(haystack.u.tree)) {
        const clm_int count = clm_tree_array_count(haystack.u.tree);
        for (clm_int i = 0; i < count; ++i)
            if (values_equal(needle, tree_element(haystack.u.tree, i))) return true;
        return false;
    }
    (void)r;
    if (needle.kind != V_STRING) return false;
    const value member = tree_member(haystack.u.tree, needle.u.text.data, needle.u.text.length);
    return member.kind != V_UNDEF;
}

static value eval(render* r, const expr* e);

static bool eval_args(render* r, const expr* e, value* values)
{
    for (size_t i = 0; i < e->arg_count; ++i) {
        values[i] = eval(r, e->args[i]);
        if (r->status != TPL_OK) return false;
    }
    return true;
}

/* ---- rendering: tests and filters ----------------------------------------- */

static bool run_test(const int id, const value v, const value* args, const size_t arg_count)
{
    (void)arg_count;
    switch (id) {
    case T_DEFINED: return v.kind != V_UNDEF;
    case T_UNDEFINED: return v.kind == V_UNDEF;
    case T_NONE: return v.kind == V_NONE;
    case T_STRING: return v.kind == V_STRING;
    case T_NUMBER: return v.kind == V_INT || v.kind == V_DOUBLE;
    case T_INTEGER: return v.kind == V_INT;
    case T_FLOAT: return v.kind == V_DOUBLE;
    case T_BOOLEAN: return v.kind == V_BOOL;
    case T_MAPPING: return v.kind == V_TREE && clm_tree_is_object(v.u.tree);
    case T_SEQUENCE: return v.kind == V_STRING || (v.kind == V_TREE && clm_tree_is_array(v.u.tree));
    case T_EVEN: return v.kind == V_INT && v.u.integer % 2 == 0;
    case T_ODD: return v.kind == V_INT && v.u.integer % 2 != 0;
    case T_DIVISIBLEBY: return v.kind == V_INT && args[0].kind == V_INT && args[0].u.integer != 0 &&
                               (args[0].u.integer == -1 || v.u.integer % args[0].u.integer == 0);
    case T_TRUE: return v.kind == V_BOOL && v.u.boolean;
    case T_FALSE: return v.kind == V_BOOL && !v.u.boolean;
    case T_LOWER:
    case T_UPPER: {
        if (v.kind != V_STRING) return false;
        bool cased = false;
        for (size_t i = 0; i < v.u.text.length; ++i) {
            const char c = v.u.text.data[i];
            const bool upper = c >= 'A' && c <= 'Z';
            const bool lower = c >= 'a' && c <= 'z';
            if (id == T_LOWER ? upper : lower) return false;
            cased = cased || upper || lower;
        }
        return cased;
    }
    default: return false;
    }
}

static value parse_number_text(const value text, const value fallback, const bool want_float)
{
    char buffer[64];
    size_t start = 0;
    size_t end = text.u.text.length;
    while (start < end && is_space(text.u.text.data[start])) ++start;
    while (end > start && is_space(text.u.text.data[end - 1])) --end;
    if (start == end || end - start >= sizeof(buffer)) return fallback;
    memcpy(buffer, text.u.text.data + start, end - start);
    buffer[end - start] = '\0';
    char* tail = NULL;
    if (!want_float) {
        errno = 0;
        const long long whole = strtoll(buffer, &tail, 10);
        if (*tail == '\0' && errno != ERANGE) return v_int((int64_t)whole);
    }
    errno = 0;
    const double real = strtod(buffer, &tail);
    if (*tail != '\0' || errno == ERANGE || !isfinite(real)) return fallback;
    if (want_float) return v_double(real);
    /* INT64_MIN is representable; INT64_MAX rounds to 2^63 in double. */
    if (real < -0x1p63 || real >= 0x1p63) return fallback;
    return v_int((int64_t)real);
}

static value apply_filter(render* r, const expr* e, const value in, const value* args, const size_t arg_count)
{
    const size_t offset = e->offset;
    const char* text = NULL;
    size_t length = 0;
    switch (e->builtin) {
    case F_UPPER:
    case F_LOWER:
    case F_CAPITALIZE:
    case F_TITLE: {
        if (!value_text(r, in, e->a, &text, &length)) return v_undef();
        value copy = scratch_text(r, text, length, in.kind == V_STRING && in.safe, offset);
        if (r->status != TPL_OK) return v_undef();
        char* s = (char*)copy.u.text.data;
        bool word_start = true;
        for (size_t i = 0; i < length; ++i) {
            const bool is_letter = (s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z');
            const bool first = e->builtin == F_CAPITALIZE ? i == 0 : word_start;
            if (e->builtin == F_UPPER) {
                if (s[i] >= 'a' && s[i] <= 'z') s[i] = (char)(s[i] - 32);
            } else if (e->builtin == F_LOWER) {
                if (s[i] >= 'A' && s[i] <= 'Z') s[i] = (char)(s[i] + 32);
            } else if (first && s[i] >= 'a' && s[i] <= 'z') {
                s[i] = (char)(s[i] - 32);
            } else if (!first && s[i] >= 'A' && s[i] <= 'Z') {
                s[i] = (char)(s[i] + 32);
            }
            word_start = !(is_letter || is_digit(s[i]) || s[i] == '\'');
        }
        return copy;
    }
    case F_TRIM: {
        if (!value_text(r, in, e->a, &text, &length)) return v_undef();
        size_t start = 0;
        while (start < length && is_space(text[start])) ++start;
        while (length > start && is_space(text[length - 1])) --length;
        return v_text(text + start, length - start, in.kind == V_STRING && in.safe);
    }
    case F_LENGTH:
        if (in.kind == V_UNDEF || in.kind == V_NONE) return v_int(0);
        if (in.kind == V_STRING) return v_int((int64_t)utf8_count(in.u.text.data, in.u.text.length));
        if (in.kind == V_TREE)
            return v_int(clm_tree_is_array(in.u.tree) ? clm_tree_array_count(in.u.tree) : clm_tree_count(in.u.tree));
        r_fail(r, TPL_RENDER, offset, "this value has no length");
        return v_undef();
    case F_DEFAULT: {
        const value fallback = arg_count >= 1 ? args[0] : v_text("", 0, false);
        const bool replace_falsy = arg_count >= 2 && truthy(args[1]);
        return in.kind == V_UNDEF || (replace_falsy && !truthy(in)) ? fallback : in;
    }
    case F_JOIN: {
        const char* separator = "";
        size_t separator_length = 0;
        if (in.kind == V_UNDEF) return v_text("", 0, false);
        if (in.kind != V_TREE || !clm_tree_is_array(in.u.tree)) {
            r_fail(r, TPL_RENDER, offset, "join needs an array");
            return v_undef();
        }
        if (arg_count >= 1 && !value_text(r, args[0], e->args[0], &separator, &separator_length)) return v_undef();
        clm_bytes_buffer joined = {0};
        const clm_int count = clm_tree_array_count(in.u.tree);
        for (clm_int i = 0; i < count && r->status == TPL_OK; ++i) {
            if (i != 0 && !buffer_append_checked(r, &joined, separator, separator_length, offset)) break;
            if (!value_text(r, tree_element(in.u.tree, i), e->a, &text, &length) ||
                !buffer_append_checked(r, &joined, text, length, offset)) break;
        }
        const clm_bytes_view view = clm_bytes_buffer_view(&joined);
        const value result = r->status == TPL_OK ? scratch_text(r, (const char*)view.data, view.length, false, offset) : v_undef();
        clm_bytes_buffer_dispose(&joined);
        return result;
    }
    case F_REPLACE: {
        const char* from = NULL;
        const char* to = NULL;
        size_t from_length = 0;
        size_t to_length = 0;
        if (!value_text(r, in, e->a, &text, &length) || !value_text(r, args[0], e->args[0], &from, &from_length) ||
            !value_text(r, args[1], e->args[1], &to, &to_length))
            return v_undef();
        if (from_length == 0) return v_text(text, length, in.kind == V_STRING && in.safe);
        clm_bytes_buffer replaced = {0};
        size_t i = 0;
        bool replaced_any = false;
        while (i < length && r->status == TPL_OK) {
            if (length - i >= from_length && memcmp(text + i, from, from_length) == 0) {
                replaced_any = true;
                if (!buffer_append_checked(r, &replaced, to, to_length, offset)) break;
                i += from_length;
            } else {
                if (!buffer_append_byte_checked(r, &replaced, (unsigned char)text[i], offset)) break;
                ++i;
            }
        }
        const clm_bytes_view view = clm_bytes_buffer_view(&replaced);
        /* Markup stays safe only if newly inserted text is explicitly safe. */
        const bool safe = in.kind == V_STRING && in.safe &&
            (!replaced_any || (args[1].kind == V_STRING && args[1].safe));
        const value result = r->status == TPL_OK
            ? scratch_text(r, (const char*)view.data, view.length, safe, offset) : v_undef();
        clm_bytes_buffer_dispose(&replaced);
        return result;
    }
    case F_ESCAPE: {
        if (in.kind == V_STRING && in.safe) return in;
        if (!value_text(r, in, e->a, &text, &length)) return v_undef();
        clm_bytes_buffer escaped = {0};
        for (size_t i = 0; i < length; ++i) {
            const char* replacement = text[i] == '&' ? "&amp;" : text[i] == '<' ? "&lt;" : text[i] == '>' ? "&gt;" :
                text[i] == '"' ? "&#34;" : text[i] == '\'' ? "&#39;" : NULL;
            if (replacement != NULL) {
                if (!buffer_append_checked(r, &escaped, replacement, strlen(replacement), offset)) break;
            } else if (!buffer_append_byte_checked(r, &escaped, (unsigned char)text[i], offset)) {
                break;
            }
        }
        const clm_bytes_view view = clm_bytes_buffer_view(&escaped);
        const value result = r->status == TPL_OK
            ? scratch_text(r, (const char*)view.data, view.length, true, offset) : v_undef();
        clm_bytes_buffer_dispose(&escaped);
        return result;
    }
    case F_SAFE: {
        if (in.kind == V_STRING) {
            value marked = in;
            marked.safe = true;
            return marked;
        }
        return in;
    }
    case F_FIRST:
    case F_LAST:
        if (in.kind == V_TREE && clm_tree_is_array(in.u.tree)) return tree_element(in.u.tree, e->builtin == F_FIRST ? 0 : -1);
        if (in.kind == V_STRING && in.u.text.length != 0) {
            const size_t count = utf8_count(in.u.text.data, in.u.text.length);
            const size_t from = utf8_offset(in.u.text.data, in.u.text.length, e->builtin == F_FIRST ? 0 : count - 1);
            const size_t to = utf8_offset(in.u.text.data, in.u.text.length, e->builtin == F_FIRST ? 1 : count);
            return v_text(in.u.text.data + from, to - from, in.safe);
        }
        return v_undef();
    case F_INT:
    case F_FLOAT: {
        const bool want_float = e->builtin == F_FLOAT;
        const value fallback = arg_count >= 1 ? args[0] : want_float ? v_double(0.0) : v_int(0);
        if (in.kind == V_INT) return want_float ? v_double((double)in.u.integer) : in;
        if (in.kind == V_DOUBLE) {
            if (want_float) return in;
            if (!isfinite(in.u.real) || in.u.real < -0x1p63 || in.u.real >= 0x1p63)
                return fallback;
            return v_int((int64_t)in.u.real);
        }
        if (in.kind == V_BOOL) return want_float ? v_double(in.u.boolean ? 1.0 : 0.0) : v_int(in.u.boolean ? 1 : 0);
        if (in.kind == V_STRING) return parse_number_text(in, fallback, want_float);
        return fallback;
    }
    case F_STRING:
        if (!value_text(r, in, e->a, &text, &length)) return v_undef();
        return v_text(text, length, in.kind == V_STRING && in.safe);
    case F_ABS:
        if (in.kind == V_INT || in.kind == V_BOOL) {
            const int64_t x = as_int(in);
            return x == INT64_MIN ? v_double(-(double)x) : v_int(x < 0 ? -x : x);
        }
        if (in.kind == V_DOUBLE) return v_double(fabs(in.u.real));
        r_fail(r, TPL_RENDER, offset, "abs needs a number");
        return v_undef();
    case F_ROUND: {
        if (!is_number(in)) {
            r_fail(r, TPL_RENDER, offset, "round needs a number");
            return v_undef();
        }
        const int64_t digits = arg_count >= 1 && args[0].kind == V_INT ? args[0].u.integer : 0;
        const double factor = pow(10.0, (double)digits);
        return v_double(floor(as_double(in) * factor + 0.5) / factor);
    }
    case F_REVERSE: {
        if (in.kind != V_STRING) {
            r_fail(r, TPL_RENDER, offset, "reverse needs a string");
            return v_undef();
        }
        value copy = scratch_text(r, in.u.text.data, in.u.text.length, in.safe, offset);
        if (r->status != TPL_OK) return v_undef();
        char* out = (char*)copy.u.text.data;
        size_t written = 0;
        size_t end = in.u.text.length;
        while (end > 0) {
            size_t start = end - 1;
            while (start > 0 && ((unsigned char)in.u.text.data[start] & 0xC0u) == 0x80u) --start;
            memcpy(out + written, in.u.text.data + start, end - start);
            written += end - start;
            end = start;
        }
        return copy;
    }
    case F_TRUNCATE: {
        if (!value_text(r, in, e->a, &text, &length)) return v_undef();
        const int64_t limit = arg_count >= 1 && args[0].kind == V_INT ? args[0].u.integer : 255;
        const bool kill_words = arg_count >= 2 && truthy(args[1]);
        const char* ending = "...";
        size_t ending_length = 3;
        if (arg_count >= 3 && !value_text(r, args[2], e->args[2], &ending, &ending_length)) return v_undef();
        const size_t characters = utf8_count(text, length);
        if (limit < 0 || (uint64_t)limit >= characters) return v_text(text, length, in.kind == V_STRING && in.safe);
        const size_t ending_characters = utf8_count(ending, ending_length);
        size_t keep = (size_t)limit > ending_characters ? (size_t)limit - ending_characters : 0;
        size_t cut = utf8_offset(text, length, keep);
        if (!kill_words) {
            size_t space = cut;
            while (space > 0 && text[space - 1] != ' ') --space;
            if (space > 0) cut = space - 1;
        }
        if (ending_length > SIZE_MAX - cut || cut + ending_length > r->max_output) {
            r_fail(r, TPL_LIMIT, offset, "truncated result exceeds the output limit");
            return v_undef();
        }
        char* joined = scratch_alloc(r, cut + ending_length, offset);
        if (joined == NULL) return v_undef();
        memcpy(joined, text, cut);
        memcpy(joined + cut, ending, ending_length);
        joined[cut + ending_length] = '\0';
        return v_text(joined, cut + ending_length, false);
    }
    case F_URLENCODE: {
        if (!value_text(r, in, e->a, &text, &length)) return v_undef();
        size_t encoded_length = 0;
        for (size_t i = 0; i < length; ++i) {
            const unsigned char c = (unsigned char)text[i];
            const bool plain = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') || c == '-' || c == '_' || c == '.' ||
                c == '~' || c == '/';
            const size_t count = plain ? 1 : 3;
            if (count > r->max_output - encoded_length) {
                r_fail(r, TPL_LIMIT, offset, "URL-encoded result exceeds the output limit");
                return v_undef();
            }
            encoded_length += count;
        }
        char* encoded = scratch_alloc(r, encoded_length, offset);
        if (encoded == NULL) return v_undef();
        size_t written = 0;
        for (size_t i = 0; i < length; ++i) {
            const unsigned char c = (unsigned char)text[i];
            if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_' ||
                c == '.' || c == '~' || c == '/') {
                encoded[written++] = (char)c;
            } else {
                static const char hex[] = "0123456789ABCDEF";
                encoded[written++] = '%';
                encoded[written++] = hex[c >> 4];
                encoded[written++] = hex[c & 15];
            }
        }
        encoded[written] = '\0';
        return v_text(encoded, written, false);
    }
    case F_SUM: {
        if (in.kind == V_UNDEF) return v_int(0);
        if (in.kind != V_TREE || !clm_tree_is_array(in.u.tree)) {
            r_fail(r, TPL_RENDER, offset, "sum needs an array");
            return v_undef();
        }
        value total = v_int(0);
        const clm_int count = clm_tree_array_count(in.u.tree);
        for (clm_int i = 0; i < count; ++i) {
            const value item = tree_element(in.u.tree, i);
            if (!is_number(item)) {
                r_fail(r, TPL_RENDER, offset, "sum needs an array of numbers");
                return v_undef();
            }
            const bool integral = total.kind == V_INT && item.kind != V_DOUBLE;
            if (integral && !add_overflows(total.u.integer, as_int(item))) total = v_int(total.u.integer + as_int(item));
            else total = v_double(as_double(total) + as_double(item));
        }
        return total;
    }
    default:
        r_fail(r, TPL_RENDER, offset, "unsupported filter");
        return v_undef();
    }
}

/* ---- rendering: expressions ------------------------------------------------ */

static value loop_attribute(render* r, const loop_state* loop, const expr* e)
{
    const char* name = e->name;
    const size_t n = e->name_length;
    const size_t index0 = loop->index0;
    if (n == 5 && memcmp(name, "index", 5) == 0) return v_int((int64_t)index0 + 1);
    if (n == 6 && memcmp(name, "index0", 6) == 0) return v_int((int64_t)index0);
    if (n == 8 && memcmp(name, "revindex", 8) == 0) return v_int((int64_t)(loop->length - index0));
    if (n == 9 && memcmp(name, "revindex0", 9) == 0) return v_int((int64_t)(loop->length - index0 - 1));
    if (n == 5 && memcmp(name, "first", 5) == 0) return v_bool(index0 == 0);
    if (n == 4 && memcmp(name, "last", 4) == 0) return v_bool(index0 + 1 == loop->length);
    if (n == 6 && memcmp(name, "length", 6) == 0) return v_int((int64_t)loop->length);
    r_fail(r, TPL_RENDER, e->offset, "loop has no member '%.*s'", (int)n, name);
    return v_undef();
}

static value eval(render* r, const expr* e)
{
    if (r->status != TPL_OK) return v_undef();
    switch (e->kind) {
    case EX_STR: return v_text(e->name, e->name_length, false);
    case EX_INT: return v_int(e->integer);
    case EX_DOUBLE: return v_double(e->real);
    case EX_BOOL: return v_bool(e->boolean);
    case EX_NONE: return v_none();
    case EX_VAR: return lookup_variable(r, e->name, e->name_length);
    case EX_ATTR: {
        const value base = eval(r, e->a);
        if (r->status != TPL_OK) return v_undef();
        if (base.kind == V_LOOP) return loop_attribute(r, base.u.loop, e);
        if (base.kind == V_TREE) return tree_member(base.u.tree, e->name, e->name_length);
        return v_undef();
    }
    case EX_INDEX: {
        const value base = eval(r, e->a);
        const value index = eval(r, e->b);
        if (r->status != TPL_OK || base.kind != V_TREE) return v_undef();
        if (index.kind == V_INT && clm_tree_is_array(base.u.tree)) return tree_element(base.u.tree, index.u.integer);
        if (index.kind == V_STRING) return tree_member(base.u.tree, index.u.text.data, index.u.text.length);
        return v_undef();
    }
    case EX_FILTER: {
        const value input = eval(r, e->a);
        value args[MAX_ARGS];
        if (r->status != TPL_OK || !eval_args(r, e, args)) return v_undef();
        return apply_filter(r, e, input, args, e->arg_count);
    }
    case EX_TEST: {
        const value subject = eval(r, e->a);
        value args[MAX_ARGS];
        if (r->status != TPL_OK || !eval_args(r, e, args)) return v_undef();
        return v_bool(run_test(e->builtin, subject, args, e->arg_count) != e->negate);
    }
    case EX_CALL:
        r_fail(r, TPL_RENDER, e->offset, "range() can only be used as the iterable of a for loop");
        return v_undef();
    case EX_COND: {
        const value condition = eval(r, e->a);
        if (r->status != TPL_OK) return v_undef();
        if (truthy(condition)) return eval(r, e->b);
        return e->c != NULL ? eval(r, e->c) : v_undef();
    }
    case EX_UNARY: {
        const value operand = eval(r, e->a);
        if (r->status != TPL_OK) return v_undef();
        if (e->op == OP_NOT) return v_bool(!truthy(operand));
        if (!defined_or_fail(r, operand, e->a)) return v_undef();
        if (!is_number(operand)) {
            r_fail(r, TPL_RENDER, e->offset, "unary operator needs a number");
            return v_undef();
        }
        if (e->op == OP_POS) return operand.kind == V_BOOL ? v_int(as_int(operand)) : operand;
        if (operand.kind == V_DOUBLE) return v_double(-operand.u.real);
        return as_int(operand) == INT64_MIN ? v_double(-(double)INT64_MIN) : v_int(-as_int(operand));
    }
    case EX_BINARY: {
        if (e->op == OP_AND || e->op == OP_OR) {
            const value left = eval(r, e->a);
            if (r->status != TPL_OK) return v_undef();
            if (truthy(left) == (e->op == OP_OR)) return left;
            return eval(r, e->b);
        }
        const value left = eval(r, e->a);
        const value right = eval(r, e->b);
        if (r->status != TPL_OK) return v_undef();
        switch (e->op) {
        case OP_EQ: return v_bool(values_equal(left, right));
        case OP_NE: return v_bool(!values_equal(left, right));
        case OP_IN: return v_bool(contains_value(r, left, right));
        case OP_NOT_IN: return v_bool(!contains_value(r, left, right));
        case OP_LT:
        case OP_LE:
        case OP_GT:
        case OP_GE: {
            int order = 0;
            if (!defined_or_fail(r, left, e->a) || !defined_or_fail(r, right, e->b)) return v_undef();
            if (!values_order(left, right, &order)) {
                r_fail(r, TPL_RENDER, e->offset, "these values cannot be compared");
                return v_undef();
            }
            return v_bool(e->op == OP_LT ? order < 0 : e->op == OP_LE ? order <= 0 : e->op == OP_GT ? order > 0 : order >= 0);
        }
        case OP_CONCAT: {
            const char* a = NULL;
            const char* b = NULL;
            size_t an = 0;
            size_t bn = 0;
            if (!value_text(r, left, e->a, &a, &an) || !value_text(r, right, e->b, &b, &bn)) return v_undef();
            if (bn > SIZE_MAX - an || an + bn > r->max_output) {
                r_fail(r, TPL_LIMIT, e->offset, "concatenated result exceeds the output limit");
                return v_undef();
            }
            char* joined = scratch_alloc(r, an + bn, e->offset);
            if (joined == NULL) return v_undef();
            memcpy(joined, a, an);
            memcpy(joined + an, b, bn);
            joined[an + bn] = '\0';
            return v_text(joined, an + bn, left.kind == V_STRING && left.safe && right.kind == V_STRING && right.safe);
        }
        default:
            if (!defined_or_fail(r, left, e->a) || !defined_or_fail(r, right, e->b)) return v_undef();
            return arithmetic(r, e, left, right);
        }
    }
    }
    return v_undef();
}

/* ---- rendering: statements ------------------------------------------------- */

static clm_int render_nodes(render* r, const node* n);

static void render_output(render* r, const node* n)
{
    const value v = eval(r, n->expression);
    if (r->status != TPL_OK) return;
    const char* text = NULL;
    size_t length = 0;
    if (!value_text(r, v, n->expression, &text, &length)) return;
    if (r->autoescape && !(v.kind == V_STRING && v.safe)) out_put_escaped(r, text, length, n->offset);
    else out_put(r, text, length, n->offset);
}

static void render_set(render* r, const node* n)
{
    value v = eval(r, n->expression);
    if (r->status != TPL_OK) return;
    if (v.kind == V_LOOP) {
        r_fail(r, TPL_RENDER, n->offset, "the loop object cannot be stored");
        return;
    }
    char* owned = NULL;
    if (v.kind == V_STRING) {
        owned = (char*)malloc(v.u.text.length + 1);
        if (owned == NULL) {
            r_fail(r, TPL_MEMORY, n->offset, "out of memory");
            return;
        }
        if (v.u.text.length != 0) memcpy(owned, v.u.text.data, v.u.text.length);
        owned[v.u.text.length] = '\0';
        v.u.text.data = owned;
    }
    for (size_t i = r->scope_count; i-- > r->frame_base;) {
        if (r->scope[i].name_length == n->name_length && memcmp(r->scope[i].name, n->name, n->name_length) == 0) {
            free(r->scope[i].owned);
            r->scope[i].val = v;
            r->scope[i].owned = owned;
            return;
        }
    }
    scope_push(r, n->name, n->name_length, v, owned);
}

static void render_for(render* r, const node* n)
{
    const expr* iterable = n->expression;
    size_t count = 0;
    clm_id array = 0;
    bool use_range = false;
    int64_t start = 0;
    int64_t step = 1;
    if (iterable->kind == EX_CALL) {
        value args[MAX_ARGS];
        if (!eval_args(r, iterable, args)) return;
        for (size_t i = 0; i < iterable->arg_count; ++i) {
            if (args[i].kind != V_INT) {
                r_fail(r, TPL_RENDER, iterable->offset, "range() needs whole numbers");
                return;
            }
        }
        int64_t stop = args[0].u.integer;
        if (iterable->arg_count >= 2) {
            start = args[0].u.integer;
            stop = args[1].u.integer;
        }
        if (iterable->arg_count == 3) step = args[2].u.integer;
        if (step == 0) {
            r_fail(r, TPL_RENDER, iterable->offset, "range() step must not be zero");
            return;
        }
        uint64_t total = 0;
        if (step > 0 && stop > start) total = ((uint64_t)stop - (uint64_t)start - 1) / (uint64_t)step + 1;
        if (step < 0 && start > stop) total = ((uint64_t)start - (uint64_t)stop - 1) / ((uint64_t)0 - (uint64_t)step) + 1;
        if (total > r->max_iterations) {
            r_fail(r, TPL_LIMIT, iterable->offset, "range() is larger than the loop limit of %zu", r->max_iterations);
            return;
        }
        count = (size_t)total;
        use_range = true;
    } else {
        const value sequence = eval(r, iterable);
        if (r->status != TPL_OK) return;
        if (sequence.kind == V_UNDEF || sequence.kind == V_NONE) {
            if (!defined_or_fail(r, sequence, iterable)) return;
        } else if (sequence.kind == V_TREE && clm_tree_is_array(sequence.u.tree)) {
            array = sequence.u.tree;
            const clm_int total = clm_tree_array_count(array);
            count = total > 0 ? (size_t)total : 0;
        } else {
            r_fail(r, TPL_RENDER, iterable->offset, "only arrays and range() can be looped over");
            return;
        }
    }
    arena_free(&r->scratch);

    if (count == 0) {
        render_nodes(r, n->else_body);
        return;
    }
    const size_t base = r->scope_count;
    const size_t saved_frame = r->frame_base;
    loop_state loop = {0, count};
    value loop_value = v_undef();
    loop_value.kind = V_LOOP;
    loop_value.u.loop = &loop;
    if (!scope_push(r, n->name, n->name_length, v_undef(), NULL) || !scope_push(r, "loop", 4, loop_value, NULL)) {
        scope_truncate(r, base);
        return;
    }
    r->frame_base = base + 2;
    for (size_t i = 0; i < count && r->status == TPL_OK; ++i) {
        if (++r->iterations > r->max_iterations) {
            r_fail(r, TPL_LIMIT, n->offset, "loops ran more than the limit of %zu iterations", r->max_iterations);
            break;
        }
        loop.index0 = i;
        r->scope[base].val = use_range ? v_int((int64_t)((uint64_t)start + (uint64_t)i * (uint64_t)step))
                                       : tree_element(array, (int64_t)i);
        render_nodes(r, n->body);
        scope_truncate(r, base + 2);
    }
    scope_truncate(r, base);
    r->frame_base = saved_frame;
}

static clm_int render_nodes(render* r, const node* n)
{
    for (; n != NULL && r->status == TPL_OK; n = n->next) {
        switch (n->kind) {
        case ND_TEXT:
            out_put(r, n->text, n->length, n->offset);
            break;
        case ND_OUTPUT:
            render_output(r, n);
            arena_free(&r->scratch);
            break;
        case ND_SET:
            render_set(r, n);
            arena_free(&r->scratch);
            break;
        case ND_IF: {
            const value condition = eval(r, n->expression);
            const bool yes = r->status == TPL_OK && truthy(condition);
            arena_free(&r->scratch);
            if (r->status == TPL_OK) render_nodes(r, yes ? n->body : n->else_body);
            break;
        }
        case ND_FOR:
            render_for(r, n);
            break;
        }
    }
    return r->status;
}

/* ---- public: render --------------------------------------------------------- */

clm_int clm_template_render(
    const clm_template* tmpl, const clm_template_context* context,
    const clm_template_options* options, clm_bytes_buffer* out, clm_template_error* error)
{
    clear_error(error);
    if (tmpl == NULL || tmpl->impl == NULL || out == NULL) return CLM_TEMPLATE_INVALID_ARGUMENT;
    render r;
    memset(&r, 0, sizeof(r));
    r.impl = tmpl->impl;
    r.context = context;
    r.out = out;
    r.max_output = CLM_TEMPLATE_DEFAULT_MAX_OUTPUT;
    r.max_iterations = CLM_TEMPLATE_DEFAULT_MAX_ITERATIONS;
    if (options != NULL) {
        r.autoescape = options->autoescape;
        r.strict = options->strict_undefined;
        if (options->max_output != 0) r.max_output = options->max_output;
        if (options->max_iterations != 0) r.max_iterations = options->max_iterations;
    }
    const size_t start_length = clm_bytes_buffer_length(out);
    render_nodes(&r, tmpl->impl->root);
    arena_free(&r.scratch);
    scope_truncate(&r, 0);
    free(r.scope);
    if (r.status != TPL_OK) {
        clm_bytes_buffer_resize(out, start_length, 0);
        fill_error(error, r.status, tmpl->impl->source, tmpl->impl->length, r.error_offset, r.message);
        return r.status;
    }
    return TPL_OK;
}

clm_int clm_template_render_to(
    const clm_template* tmpl, const clm_template_context* context,
    const clm_template_options* options, char* dest, const size_t capacity,
    size_t* required, clm_template_error* error)
{
    clear_error(error);
    if (required == NULL || (dest == NULL && capacity != 0)) return CLM_TEMPLATE_INVALID_ARGUMENT;
    clm_bytes_buffer out = {0};
    clm_int status = clm_template_render(tmpl, context, options, &out, error);
    if (status == TPL_OK) {
        const clm_bytes_view view = clm_bytes_buffer_view(&out);
        *required = view.length + 1;
        if (dest != NULL) {
            if (capacity < *required) {
                status = CLM_TEMPLATE_BUFFER_TOO_SMALL;
            } else {
                if (view.length != 0) memcpy(dest, view.data, view.length);
                dest[view.length] = '\0';
            }
        }
    }
    clm_bytes_buffer_dispose(&out);
    return status;
}

clm_int clm_template_render_string_id(
    const clm_template* tmpl, const clm_template_context* context,
    const clm_template_options* options, const clm_id destination_id, clm_template_error* error)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(destination_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING) {
        clear_error(error);
        return CLM_TEMPLATE_INVALID_ARGUMENT;
    }
    clm_bytes_buffer out = {0};
    clm_int status = clm_template_render(tmpl, context, options, &out, error);
    if (status == TPL_OK) {
        const clm_bytes_view view = clm_bytes_buffer_view(&out);
        if ((view.length != 0 && memchr(view.data, '\0', view.length) != NULL) ||
            clm_bytes_buffer_append_byte(&out, 0) != CLM_BYTES_SUCCESS ||
            clm_string_set(destination_id, (const char*)clm_bytes_buffer_view(&out).data) == CLM_BPA_FAILURE) {
            status = CLM_TEMPLATE_FAILURE;
            fill_error(error, status, NULL, 0, 0, "the result is not valid UTF-8 text");
        }
    }
    clm_bytes_buffer_dispose(&out);
    return status;
}

clm_int clm_template_render_text(
    const char* source, const size_t length, const clm_template_context* context,
    const clm_template_options* options, clm_bytes_buffer* out, clm_template_error* error)
{
    clm_template tmpl = {0};
    clm_int status = clm_template_compile(&tmpl, source, length, NULL, error);
    if (status != TPL_OK) return status;
    status = clm_template_render(&tmpl, context, options, out, error);
    clm_template_dispose(&tmpl);
    return status;
}

clm_int clm_template_render_ids(
    const clm_id template_id, const clm_template_context* context,
    const clm_template_options* options, const clm_id destination_id, clm_template_error* error)
{
    clm_template tmpl = {0};
    clm_int status = clm_template_compile_string_id(&tmpl, template_id, NULL, error);
    if (status != TPL_OK) return status;
    status = clm_template_render_string_id(&tmpl, context, options, destination_id, error);
    clm_template_dispose(&tmpl);
    return status;
}
