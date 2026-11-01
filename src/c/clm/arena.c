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

#include "arena.h"

#include <stdalign.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct clm_arena_borrow_token {
    uint64_t value;
    struct clm_arena_borrow_token* next;
} clm_arena_borrow_token;

typedef struct clm_arena_node {
    clm_id id;
    unsigned char* data;
    size_t size;
    size_t capacity;
    uint64_t generation;
    clm_arena_borrow_token* borrows;
    struct clm_arena_node* next;
} clm_arena_node;

static clm_arena_node* clm_arena_nodes;
static atomic_flag clm_arena_lock = ATOMIC_FLAG_INIT;
static uint64_t clm_arena_generation = 1;
static uint64_t clm_arena_token = 1;

static void arena_lock(void) {
    while (atomic_flag_test_and_set_explicit(&clm_arena_lock, memory_order_acquire));
}

static void arena_unlock(void) {
    atomic_flag_clear_explicit(&clm_arena_lock, memory_order_release);
}

static clm_arena_node* arena_find(const clm_id id) {
    for (clm_arena_node* arena = clm_arena_nodes; arena; arena = arena->next) {
        if (arena->id == id) {
            return arena;
        }
    }
    return NULL;
}

static clm_int arena_grow(clm_arena_node* arena, size_t required) {
    if (required <= arena->capacity) {
        return CLM_ARENA_SUCCESS;
    }
    if (arena->borrows) {
        return CLM_ARENA_BUSY;
    }
    size_t capacity = arena->capacity ? arena->capacity : CLM_ARENA_DEFAULT_CAPACITY;
    while (capacity < required) {
        if (capacity > (size_t)-1 / 2) {
            capacity = required;
            break;
        }
        capacity *= 2;
    }
    unsigned char* resized = realloc(arena->data, capacity);
    if (!resized) {
        return CLM_ARENA_NO_MEMORY;
    }
    memset(resized + arena->capacity, 0, capacity - arena->capacity);
    arena->data = resized;
    arena->capacity = capacity;
    return CLM_ARENA_SUCCESS;
}

clm_int clm_arena_create(clm_id id, size_t initial_capacity) {
    if (!initial_capacity) {
        initial_capacity = CLM_ARENA_DEFAULT_CAPACITY;
    }
    clm_arena_node* arena = calloc(1, sizeof(*arena));
    if (!arena) {
        return CLM_ARENA_NO_MEMORY;
    }
    arena->data = calloc(1, initial_capacity);
    if (!arena->data) {
        free(arena);
        return CLM_ARENA_NO_MEMORY;
    }
    arena->capacity = initial_capacity;
    arena->id = id;

    arena_lock();
    if (arena_find(id)) {
        arena_unlock();
        free(arena->data);
        free(arena);
        return CLM_ARENA_ALREADY_EXISTS;
    }
    if (!clm_arena_generation) {
        arena_unlock();
        free(arena->data);
        free(arena);
        return CLM_ARENA_FAILURE;
    }
    arena->generation = clm_arena_generation++;
    arena->next = clm_arena_nodes;
    clm_arena_nodes = arena;
    arena_unlock();
    return CLM_ARENA_SUCCESS;
}

clm_int clm_arena_destroy(clm_id id) {
    arena_lock();
    clm_arena_node** link = &clm_arena_nodes;
    while (*link && (*link)->id != id) {
        link = &(*link)->next;
    }
    if (!*link) {
        arena_unlock();
        return CLM_ARENA_NOT_FOUND;
    }
    clm_arena_node* arena = *link;
    if (arena->borrows) {
        arena_unlock();
        return CLM_ARENA_BUSY;
    }
    *link = arena->next;
    arena_unlock();
    free(arena->data);
    free(arena);
    return CLM_ARENA_SUCCESS;
}

clm_int clm_arena_allocate(
    clm_id id, size_t size, size_t alignment, size_t* out_offset) {
    if (!size || !out_offset) {
        return CLM_ARENA_INVALID_ARGUMENT;
    }
    if (!alignment) {
        alignment = alignof(max_align_t);
    }
    if ((alignment & (alignment - 1)) != 0 || alignment > alignof(max_align_t)) {
        return CLM_ARENA_INVALID_ARGUMENT;
    }

    arena_lock();
    clm_arena_node* arena = arena_find(id);
    if (!arena) {
        arena_unlock();
        return CLM_ARENA_NOT_FOUND;
    }
    if (arena->borrows) {
        arena_unlock();
        return CLM_ARENA_BUSY;
    }
    size_t padding = (alignment - (arena->size & (alignment - 1))) & (alignment - 1);
    if (padding > (size_t)-1 - arena->size || size > (size_t)-1 - arena->size - padding) {
        arena_unlock();
        return CLM_ARENA_OUT_OF_RANGE;
    }
    const size_t offset = arena->size + padding;
    const size_t required = offset + size;
    const clm_int status = arena_grow(arena, required);
    if (status == CLM_ARENA_SUCCESS) {
        memset(arena->data + arena->size, 0, required - arena->size);
        arena->size = required;
        *out_offset = offset;
    }
    arena_unlock();
    return status;
}

clm_int clm_arena_reserve(clm_id id, size_t requested_capacity) {
    arena_lock();
    clm_arena_node* arena = arena_find(id);
    if (!arena) {
        arena_unlock();
        return CLM_ARENA_NOT_FOUND;
    }
    const clm_int status = arena_grow(arena, requested_capacity);
    arena_unlock();
    return status;
}

clm_int clm_arena_reset(clm_id id) {
    arena_lock();
    clm_arena_node* arena = arena_find(id);
    if (!arena) {
        arena_unlock();
        return CLM_ARENA_NOT_FOUND;
    }
    if (arena->borrows) {
        arena_unlock();
        return CLM_ARENA_BUSY;
    }
    arena->size = 0;
    arena_unlock();
    return CLM_ARENA_SUCCESS;
}

clm_int clm_arena_rewind(clm_id id, size_t offset) {
    arena_lock();
    clm_arena_node* arena = arena_find(id);
    if (!arena) {
        arena_unlock();
        return CLM_ARENA_NOT_FOUND;
    }
    if (arena->borrows) {
        arena_unlock();
        return CLM_ARENA_BUSY;
    }
    if (offset > arena->size) {
        arena_unlock();
        return CLM_ARENA_OUT_OF_RANGE;
    }
    arena->size = offset;
    arena_unlock();
    return CLM_ARENA_SUCCESS;
}

clm_int clm_arena_get_info(
    const clm_id id, size_t* out_size, size_t* out_capacity) {
    if (!out_size || !out_capacity) {
        return CLM_ARENA_INVALID_ARGUMENT;
    }
    arena_lock();
    clm_arena_node* arena = arena_find(id);
    if (!arena) {
        arena_unlock();
        return CLM_ARENA_NOT_FOUND;
    }
    *out_size = arena->size;
    *out_capacity = arena->capacity;
    arena_unlock();
    return CLM_ARENA_SUCCESS;
}

clm_int clm_arena_borrow(clm_id id, clm_arena_borrowed* out_borrow) {
    if (!out_borrow) {
        return CLM_ARENA_INVALID_ARGUMENT;
    }
    clm_arena_borrow_token* token = malloc(sizeof(*token));
    if (!token) {
        return CLM_ARENA_NO_MEMORY;
    }
    arena_lock();
    clm_arena_node* arena = arena_find(id);
    if (!arena) {
        arena_unlock();
        free(token);
        return CLM_ARENA_NOT_FOUND;
    }
    if (!clm_arena_token) {
        arena_unlock();
        free(token);
        return CLM_ARENA_FAILURE;
    }
    token->value = clm_arena_token++;
    token->next = arena->borrows;
    arena->borrows = token;
    *out_borrow = (clm_arena_borrowed){
        .id = id, .size = arena->size, .capacity = arena->capacity, .generation = arena->generation,
        .data = arena->data, ._owner = arena, ._token = token->value
    };
    arena_unlock();
    return CLM_ARENA_SUCCESS;
}

clm_int clm_arena_return(clm_arena_borrowed* borrowed) {
    if (!borrowed || !borrowed->_owner || !borrowed->_token) {
        return CLM_ARENA_INVALID_ARGUMENT;
    }
    arena_lock();
    clm_arena_node* arena = arena_find(borrowed->id);
    if (!arena || arena != borrowed->_owner || arena->generation != borrowed->generation) {
        arena_unlock();
        return CLM_ARENA_NOT_FOUND;
    }
    clm_arena_borrow_token** link = &arena->borrows;
    while (*link && (*link)->value != borrowed->_token) {
        link = &(*link)->next;
    }
    if (!*link) {
        arena_unlock();
        return CLM_ARENA_INVALID_ARGUMENT;
    }
    clm_arena_borrow_token* token = *link;
    *link = token->next;
    memset(borrowed, 0, sizeof(*borrowed));
    arena_unlock();
    free(token);
    return CLM_ARENA_SUCCESS;
}