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

#include "bpa.h"

#include <stdalign.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

clm_bpa_pool_class clm_bpa_global_pools[CLM_BPA_POOL_CLASSES] = {{0}};
clm_bpa_block_node clm_bpa_global_block_tombstone = {0};
clm_bpa_block_node** clm_bpa_global_blocks = NULL;
size_t clm_bpa_global_blocks_capacity = 0;
size_t clm_bpa_global_blocks_count = 0;
size_t clm_bpa_global_blocks_tombstones = 0;

clm_bpaa_array_node clm_bpa_global_array_tombstone = {0};
clm_bpaa_array_node** clm_bpa_global_arrays = NULL;
size_t clm_bpa_global_arrays_capacity = 0;
size_t clm_bpa_global_arrays_count = 0;
size_t clm_bpa_global_arrays_tombstones = 0;

static atomic_uint_fast64_t clm_bpa_pool_arena_ids =
    ATOMIC_VAR_INIT(UINT64_C(0xbfa0000000000001));

static int clm_bpa_pool_slab_create(
    const size_t bytes, clm_id* out_arena_id,
    clm_arena_borrowed* out_borrowed, void** out_memory)
{
    for (size_t attempt = 0; attempt < 1024; ++attempt) {
        clm_id arena_id = (clm_id)atomic_fetch_add_explicit(
            &clm_bpa_pool_arena_ids, 1, memory_order_relaxed);
        if (arena_id == 0)
            continue;
        clm_int status = clm_arena_create(arena_id, bytes);
        if (status == CLM_ARENA_ALREADY_EXISTS)
            continue;
        if (status != CLM_ARENA_SUCCESS)
            return CLM_BPA_FAILURE;

        size_t offset = 0;
        status = clm_arena_allocate(arena_id, bytes, alignof(max_align_t), &offset);
        if (status == CLM_ARENA_SUCCESS)
            status = clm_arena_borrow(arena_id, out_borrowed);
        if (status != CLM_ARENA_SUCCESS) {
            (void)clm_arena_destroy(arena_id);
            return CLM_BPA_FAILURE;
        }
        *out_arena_id = arena_id;
        *out_memory = (unsigned char*)out_borrowed->data + offset;
        return CLM_BPA_SUCCESS;
    }
    return CLM_BPA_FAILURE;
}

size_t clm_bpa_pool_class_for(const size_t bytes) {
    size_t index = 0;
    size_t size = sizeof(void*) > 8 ? sizeof(void*) : 8;
    if (bytes == 0 || bytes > CLM_BPA_POOL_MAX_SLOT) {
        return CLM_BPA_POOL_CLASSES;
    }
    while (size < bytes && index + 1 < CLM_BPA_POOL_CLASSES) {
        size *= 2;
        ++index;
    }
    return size >= bytes ? index : CLM_BPA_POOL_CLASSES;
}

void* clm_bpa_pool_alloc(const size_t bytes) {
    const size_t idx = clm_bpa_pool_class_for(bytes);
    if (idx == CLM_BPA_POOL_CLASSES) {
        return bytes ? calloc(1, bytes) : NULL;
    }
    clm_bpa_pool_class* pool = &clm_bpa_global_pools[idx];
    if (pool->slot_size == 0) {
        size_t size = sizeof(void*) > 8 ? sizeof(void*) : 8;
        for (size_t i = 0; i < idx; ++i) size *= 2;
        /* malloc alignment is sufficient for fundamental C object types. */
        pool->slot_size = size;
    }
    if (pool->free_list == NULL) {
        /* Grow by one slab only when this class has no reusable slot. */
        const size_t stride = pool->slot_size;
        if (stride > ((size_t)-1 / CLM_BPA_POOL_SLAB_SLOTS)) {
            return NULL;
        }
        clm_bpa_pool_slab* slab = malloc(sizeof(*slab));
        if (slab == NULL) {
            return NULL;
        }
        const size_t slab_bytes = stride * CLM_BPA_POOL_SLAB_SLOTS;
        void* memory = NULL;
        if (clm_bpa_pool_slab_create(slab_bytes, &slab->arena_id,
                &slab->borrowed, &memory) != CLM_BPA_SUCCESS) {
            free(slab);
            return NULL;
        }
        slab->memory = memory;
        slab->next = pool->slabs;
        pool->slabs = slab;
        unsigned char* slab_memory = memory;
        for (size_t i = 0; i < CLM_BPA_POOL_SLAB_SLOTS; ++i) {
            void* slot = slab_memory + i * stride;
            *(void**)slot = pool->free_list;
            pool->free_list = slot;
        }
        pool->capacity += CLM_BPA_POOL_SLAB_SLOTS;
    }
    void* result = pool->free_list;
    pool->free_list = *(void**)result;
    ++pool->active;
    /* A returned slot may contain its old free-list link or stale payload. */
    memset(result, 0, bytes);
    return result;
}

void clm_bpa_pool_free(void* pointer, const size_t bytes) {
    if (pointer == NULL) return;
    const size_t idx = clm_bpa_pool_class_for(bytes);
    if (idx == CLM_BPA_POOL_CLASSES) {
        free(pointer);
        return;
    }
    clm_bpa_pool_class* pool = &clm_bpa_global_pools[idx];
    /* Free slots store the next pointer in-band; slab memory remains reserved. */
    *(void**)pointer = pool->free_list;
    pool->free_list = pointer;
    --pool->active;
}

int clm_bpa_pool_cleanup(void) {
    for (size_t i = 0; i < CLM_BPA_POOL_CLASSES; ++i) {
        if (clm_bpa_global_pools[i].active != 0) {
            return CLM_BPA_FAILURE;
        }
    }
    for (size_t i = 0; i < CLM_BPA_POOL_CLASSES; ++i) {
        clm_bpa_pool_class* pool = &clm_bpa_global_pools[i];
        clm_bpa_pool_slab* slab = pool->slabs;
        while (slab != NULL) {
            clm_bpa_pool_slab* next = slab->next;
            if (clm_arena_return(&slab->borrowed) != CLM_ARENA_SUCCESS ||
                clm_arena_destroy(slab->arena_id) != CLM_ARENA_SUCCESS)
                return CLM_BPA_FAILURE;
            free(slab);
            slab = next;
        }
        memset(pool, 0, sizeof(*pool));
    }
    return CLM_BPA_SUCCESS;
}

int clm_bpa_pool_stats(const size_t bytes, size_t* active, size_t* capacity) {
    const size_t idx = clm_bpa_pool_class_for(bytes);
    if (idx == CLM_BPA_POOL_CLASSES || active == NULL || capacity == NULL) {
        return CLM_BPA_FAILURE;
    }
    *active = clm_bpa_global_pools[idx].active;
    *capacity = clm_bpa_global_pools[idx].capacity;
    return CLM_BPA_SUCCESS;
}

size_t clm_bpa_hash_id(clm_id id) {
    id ^= id >> 30;
    id *= UINT64_C(0xbf58476d1ce4e5b9);
    id ^= id >> 27;
    id *= UINT64_C(0x94d049bb133111eb);
    id ^= id >> 31;
    return id;
}

int clm_bpa_rehash_blocks(const size_t capacity) {
    clm_bpa_block_node** slots = calloc(capacity, sizeof(*slots));
    if (slots == NULL) {
        return CLM_BPA_FAILURE;
    }
    /* Reinsert live nodes into the new table; tombstones are not entries. */
    for (size_t index = 0; index < clm_bpa_global_blocks_capacity; ++index) {
        clm_bpa_block_node* block = clm_bpa_global_blocks[index];
        if (block != NULL && block != CLM_BPA_BLOCK_TOMBSTONE) {
            size_t slot = clm_bpa_hash_id(block->id) & (capacity - 1);
            while (slots[slot] != NULL) {
                slot = slot + 1 & capacity - 1;
            }
            slots[slot] = block;
        }
    }
    free(clm_bpa_global_blocks);
    clm_bpa_global_blocks = slots;
    clm_bpa_global_blocks_capacity = capacity;
    clm_bpa_global_blocks_tombstones = 0;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_rehash_arrays(const size_t capacity) {
    clm_bpaa_array_node** slots = calloc(capacity, sizeof(*slots));
    if (slots == NULL) {
        return CLM_BPA_FAILURE;
    }
    /* Rehashing also compacts deleted-slot tombstones. */
    for (size_t index = 0; index < clm_bpa_global_arrays_capacity; ++index) {
        clm_bpaa_array_node* array = clm_bpa_global_arrays[index];
        if (array != NULL && array != CLM_BPA_ARRAY_TOMBSTONE) {
            size_t slot = clm_bpa_hash_id(array->id) & (capacity - 1);
            while (slots[slot] != NULL) {
                slot = slot + 1 & capacity - 1;
            }
            slots[slot] = array;
        }
    }
    free(clm_bpa_global_arrays);
    clm_bpa_global_arrays = slots;
    clm_bpa_global_arrays_capacity = capacity;
    clm_bpa_global_arrays_tombstones = 0;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_prepare_blocks(void) {
    const size_t capacity = clm_bpa_global_blocks_capacity;
    if (capacity == 0) {
        return clm_bpa_rehash_blocks(CLM_BPA_INITIAL_CAPACITY);
    }
    if (clm_bpa_global_blocks_count + clm_bpa_global_blocks_tombstones + 1 >= capacity - capacity / 4) {
        if (clm_bpa_global_blocks_count < capacity / 4 && clm_bpa_global_blocks_tombstones != 0) {
            return clm_bpa_rehash_blocks(capacity);
        }
        if (capacity > (size_t)-1 / 2) {
            return CLM_BPA_FAILURE;
        }
        return clm_bpa_rehash_blocks(capacity * 2);
    }
    return CLM_BPA_SUCCESS;
}

int clm_bpa_prepare_arrays(void) {
    const size_t capacity = clm_bpa_global_arrays_capacity;
    if (capacity == 0) {
        return clm_bpa_rehash_arrays(CLM_BPA_INITIAL_CAPACITY);
    }
    if (clm_bpa_global_arrays_count + clm_bpa_global_arrays_tombstones + 1 >= capacity - capacity / 4) {
        if (clm_bpa_global_arrays_count < capacity / 4 && clm_bpa_global_arrays_tombstones != 0) {
            return clm_bpa_rehash_arrays(capacity);
        }
        if (capacity > (size_t)-1 / 2) {
            return CLM_BPA_FAILURE;
        }
        return clm_bpa_rehash_arrays(capacity * 2);
    }
    return CLM_BPA_SUCCESS;
}

clm_uint clm_bpa_type_size(const clm_byte type) {
    switch (type) {
    case CLM_BPA_TYPE_SIGNED_BYTE: return sizeof(clm_sbyte);
    case CLM_BPA_TYPE_BYTE: return sizeof(clm_byte);
    case CLM_BPA_TYPE_SHORT: return sizeof(clm_short);
    case CLM_BPA_TYPE_UNSIGNED_SHORT: return sizeof(clm_ushort);
    case CLM_BPA_TYPE_INT: return sizeof(clm_int);
    case CLM_BPA_TYPE_UNSIGNED_INT: return sizeof(clm_uint);
    case CLM_BPA_TYPE_LONG: return sizeof(clm_long);
    case CLM_BPA_TYPE_UNSIGNED_LONG: return sizeof(clm_ulong);
    case CLM_BPA_TYPE_FLOAT: return sizeof(clm_float);
    case CLM_BPA_TYPE_DOUBLE: return sizeof(clm_double);
    case CLM_BPA_TYPE_UUID: return 16;
    case CLM_BPA_TYPE_DATETIME: return sizeof(int64_t);
    default: return 0;
    }
}

clm_bpa_block_node* clm_bpa_find_block(const clm_id id) {
    if (clm_bpa_global_blocks_capacity == 0) {
        return NULL;
    }
    size_t slot = clm_bpa_hash_id(id) & clm_bpa_global_blocks_capacity - 1;
    /* Linear probing skips tombstones but stops at a never-used empty slot. */
    for (size_t probes = 0; probes < clm_bpa_global_blocks_capacity; ++probes) {
        clm_bpa_block_node* block = clm_bpa_global_blocks[slot];
        if (block == NULL) {
            return NULL;
        }
        if (block != CLM_BPA_BLOCK_TOMBSTONE && block->id == id) {
            return block;
        }
        slot = slot + 1 & clm_bpa_global_blocks_capacity - 1;
    }
    return NULL;
}

bool clm_bpa_is_existing(const clm_id id) {
    return clm_bpa_find_block(id) != NULL;
}

clm_bpaa_array_node* clm_bpaa_find_array(const clm_id id) {
    if (clm_bpa_global_arrays_capacity == 0) {
        return NULL;
    }
    size_t slot = clm_bpa_hash_id(id) & clm_bpa_global_arrays_capacity - 1;
    /* Keep probing past tombstones so later colliding entries remain findable. */
    for (size_t probes = 0; probes < clm_bpa_global_arrays_capacity; ++probes) {
        clm_bpaa_array_node* array = clm_bpa_global_arrays[slot];
        if (array == NULL) {
            return NULL;
        }
        if (array != CLM_BPA_ARRAY_TOMBSTONE && array->id == id) {
            return array;
        }
        slot = slot + 1 & clm_bpa_global_arrays_capacity - 1;
    }
    return NULL;
}

bool clm_bpaa_is_existing(const clm_id id) {
    return clm_bpaa_find_array(id) != NULL;
}

void clm_bpa_insert_block(clm_bpa_block_node* block) {
    size_t slot = clm_bpa_hash_id(block->id) & clm_bpa_global_blocks_capacity - 1;
    size_t first_tombstone = (size_t)-1;
    while (clm_bpa_global_blocks[slot] != NULL) {
        if (clm_bpa_global_blocks[slot] == CLM_BPA_BLOCK_TOMBSTONE) {
            if (first_tombstone == (size_t)-1) {
                first_tombstone = slot;
            }
        }
        slot = slot + 1 & clm_bpa_global_blocks_capacity - 1;
    }
    /* Reuse the first tombstone in the probe chain to preserve lookup rules. */
    if (first_tombstone != (size_t)-1) {
        slot = first_tombstone;
        --clm_bpa_global_blocks_tombstones;
    }
    clm_bpa_global_blocks[slot] = block;
    ++clm_bpa_global_blocks_count;
}

void clm_bpa_insert_array(clm_bpaa_array_node* array) {
    size_t slot = clm_bpa_hash_id(array->id) & clm_bpa_global_arrays_capacity - 1;
    size_t first_tombstone = (size_t)-1;
    while (clm_bpa_global_arrays[slot] != NULL) {
        if (clm_bpa_global_arrays[slot] == CLM_BPA_ARRAY_TOMBSTONE) {
            if (first_tombstone == (size_t)-1) {
                first_tombstone = slot;
            }
        }
        slot = slot + 1 & clm_bpa_global_arrays_capacity - 1;
    }
    /* Reuse a deleted slot rather than extending the probe chain. */
    if (first_tombstone != (size_t)-1) {
        slot = first_tombstone;
        --clm_bpa_global_arrays_tombstones;
    }
    clm_bpa_global_arrays[slot] = array;
    ++clm_bpa_global_arrays_count;
}

int clm_bpa_remove_block(const clm_id id) {
    if (clm_bpa_global_blocks_capacity == 0) {
        return CLM_BPA_FAILURE;
    }
    size_t slot = clm_bpa_hash_id(id) & clm_bpa_global_blocks_capacity - 1;
    for (size_t probes = 0; probes < clm_bpa_global_blocks_capacity; ++probes) {
        clm_bpa_block_node* block = clm_bpa_global_blocks[slot];
        if (block == NULL) {
            return CLM_BPA_FAILURE;
        }
        if (block != CLM_BPA_BLOCK_TOMBSTONE && block->id == id) {
            clm_bpa_global_blocks[slot] = CLM_BPA_BLOCK_TOMBSTONE;
            --clm_bpa_global_blocks_count;
            ++clm_bpa_global_blocks_tombstones;
            clm_bpa_pool_free(block->data, block->type == CLM_BPA_TYPE_STRING
                                               ? (size_t)block->length + 1
                                               : (size_t)block->block_size);
            free(block);
            return CLM_BPA_SUCCESS;
        }
        slot = slot + 1 & clm_bpa_global_blocks_capacity - 1;
    }
    return CLM_BPA_FAILURE;
}

int clm_bpa_remove_array(const clm_id id) {
    if (clm_bpa_global_arrays_capacity == 0) {
        return CLM_BPA_FAILURE;
    }
    size_t slot = clm_bpa_hash_id(id) & (clm_bpa_global_arrays_capacity - 1);
    for (size_t probes = 0; probes < clm_bpa_global_arrays_capacity; ++probes) {
        clm_bpaa_array_node* array = clm_bpa_global_arrays[slot];
        if (array == NULL) {
            return CLM_BPA_FAILURE;
        }
        if (array != CLM_BPA_ARRAY_TOMBSTONE && array->id == id) {
            clm_bpa_global_arrays[slot] = CLM_BPA_ARRAY_TOMBSTONE;
            --clm_bpa_global_arrays_count;
            ++clm_bpa_global_arrays_tombstones;
            if (array->type == CLM_BPA_TYPE_STRING) {
                for (clm_uint index = 0; index < array->count; ++index) {
                    clm_bpa_pool_free(array->items.strings[index], strlen(array->items.strings[index]) + 1);
                }
                free(array->items.strings);
            }
            else {
                free(array->items.blocks);
            }
            free(array);
            return CLM_BPA_SUCCESS;
        }
        slot = slot + 1 & clm_bpa_global_arrays_capacity - 1;
    }
    return CLM_BPA_FAILURE;
}

int clm_bpa_copy_string(const void* buffer, const clm_uint buffer_size, char** value, clm_uint* length) {
    const char* bytes = buffer;

    if (buffer == NULL || buffer_size == 0 || value == NULL || length == NULL) {
        return CLM_BPA_FAILURE;
    }

    const char* terminator = memchr(bytes, '\0', buffer_size);
    const size_t string_length = terminator == NULL ? (size_t)buffer_size : (size_t)(terminator - bytes);
    if ((size_t)(clm_uint)string_length != string_length || string_length == (size_t)-1) {
        return CLM_BPA_FAILURE;
    }

    char* copy = clm_bpa_pool_alloc(string_length + 1);
    if (copy == NULL) {
        return CLM_BPA_FAILURE;
    }
    memcpy(copy, bytes, string_length);
    copy[string_length] = '\0';

    *value = copy;
    *length = (clm_uint)string_length;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_create_sized(const clm_id id, const clm_byte type, const clm_uint block_size) {
    clm_uint resolved_size = clm_bpa_type_size(type);

    if (clm_bpa_find_block(id) != NULL || clm_bpa_prepare_blocks() != CLM_BPA_SUCCESS) {
        return CLM_BPA_FAILURE;
    }

    /* Built-in types determine their own size; STRUCT delegates the layout to
     * the caller, while STRING stores a separately sized NUL-terminated value. */
    if (type == CLM_BPA_TYPE_STRING) {
        if (block_size != 0) {
            return CLM_BPA_FAILURE;
        }
    }
    else if (type == CLM_BPA_TYPE_STRUCT) {
        if (block_size == 0) {
            return CLM_BPA_FAILURE;
        }
        resolved_size = block_size;
    }
    else if (resolved_size == 0 || (block_size != 0 && block_size != resolved_size)) {
        return CLM_BPA_FAILURE;
    }

    clm_bpa_block_node* block = calloc(1, sizeof(*block));
    if (block == NULL) {
        return CLM_BPA_FAILURE;
    }
    if (type != CLM_BPA_TYPE_STRING) {
        block->data = clm_bpa_pool_alloc((size_t)resolved_size);
        if (block->data == NULL) {
            free(block);
            return CLM_BPA_FAILURE;
        }
    }

    block->id = id;
    block->type = type;
    block->refcount = 1;
    block->block_size = resolved_size;
    clm_bpa_insert_block(block);
    return CLM_BPA_SUCCESS;
}

int clm_bpa_create(const clm_id id, const clm_byte type) {
    if (type == CLM_BPA_TYPE_STRUCT) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpa_create_sized(id, type, 0);
}

int clm_bpa_delete(const clm_id id) {
    return clm_bpa_remove_block(id);
}

int clm_bpa_ref(const clm_id id) {
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->refcount == USHRT_MAX) {
        return CLM_BPA_FAILURE;
    }
    ++block->refcount;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_unref(const clm_id id) {
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->refcount == 0) {
        return CLM_BPA_FAILURE;
    }
    if (--block->refcount != 0) {
        return CLM_BPA_SUCCESS;
    }
    return clm_bpa_remove_block(id);
}

int clm_bpa_get(const clm_id id, void* buffer, const clm_uint buffer_size) {
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type == CLM_BPA_TYPE_STRING || buffer == NULL || buffer_size < block->block_size) {
        return CLM_BPA_FAILURE;
    }
    memcpy(buffer, block->data, block->block_size);
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpa_set(const clm_id id, const void* buffer, const clm_uint buffer_size) {
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type == CLM_BPA_TYPE_STRING || buffer == NULL || buffer_size != block->block_size) {
        return CLM_BPA_FAILURE;
    }
    memmove(block->data, buffer, block->block_size);
    return CLM_BPA_SET_SUCCESS;
}

int clm_bpa_get_string(const clm_id id, void* buffer, const clm_uint buffer_size) {
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || buffer == NULL) {
        return CLM_BPA_FAILURE;
    }
    const size_t required = (size_t)block->length + 1;
    if ((size_t)buffer_size < required || block->length > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    if (block->length != 0) {
        memcpy(buffer, block->data, block->length);
    }
    ((char*)buffer)[block->length] = '\0';
    return (int)block->length;
}

int clm_bpa_set_string(const clm_id id, const void* buffer, const clm_uint buffer_size) {
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    char* copy;
    clm_uint length;
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || clm_bpa_copy_string(buffer, buffer_size, &copy, &length) != CLM_BPA_SUCCESS) {
        return CLM_BPA_FAILURE;
    }
    if (length > INT_MAX) {
        clm_bpa_pool_free(copy, (size_t)length + 1);
        return CLM_BPA_FAILURE;
    }
    /* Build the replacement first so invalid input/allocation failure leaves
     * the block's old string intact. */
    clm_bpa_pool_free(block->data, (size_t)block->length + 1);
    block->data = copy;
    block->length = length;
    return (int)length;
}

int clm_bpa_replace(const clm_id id, const void* buffer, const clm_uint buffer_size) {
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL) {
        return CLM_BPA_FAILURE;
    }
    return block->type == CLM_BPA_TYPE_STRING
               ? clm_bpa_set_string(id, buffer, buffer_size)
               : clm_bpa_set(id, buffer, buffer_size);
}

int clm_bpaa_create_sized(const clm_id id, const clm_byte type, const clm_int reserved, const clm_uint block_size,
                          const clm_byte encoding) {
    clm_uint resolved_size = clm_bpa_type_size(type);
    if (clm_bpaa_find_array(id) != NULL || clm_bpa_prepare_arrays() != CLM_BPA_SUCCESS || reserved < 0) {
        return CLM_BPA_FAILURE;
    }

    if (type == CLM_BPA_TYPE_STRING) {
        if (block_size != 0) {
            return CLM_BPA_FAILURE;
        }
    }
    else if (type == CLM_BPA_TYPE_STRUCT) {
        if (block_size == 0) {
            return CLM_BPA_FAILURE;
        }
        resolved_size = block_size;
    }
    else if (resolved_size == 0 || (block_size != 0 && block_size != resolved_size)) {
        return CLM_BPA_FAILURE;
    }

    clm_bpaa_array_node* array = calloc(1, sizeof(*array));
    if (array == NULL) {
        return CLM_BPA_FAILURE;
    }
    array->id = id;
    array->type = type;
    array->refcount = 1;
    array->block_size = resolved_size;
    array->encoding = encoding;
    array->capacity = (clm_uint)reserved;

    if (array->capacity != 0) {
        if (type == CLM_BPA_TYPE_STRING) {
            if ((size_t)array->capacity > (size_t)-1 / sizeof(char*)) {
                free(array);
                return CLM_BPA_FAILURE;
            }
            array->items.strings = (char**)calloc(array->capacity, sizeof(char*));
        }
        else {
            if ((size_t)array->capacity > (size_t)-1 / (size_t)resolved_size) {
                free(array);
                return CLM_BPA_FAILURE;
            }
            array->items.blocks = (unsigned char*)calloc(array->capacity, (size_t)resolved_size);
        }
        if ((type == CLM_BPA_TYPE_STRING && array->items.strings == NULL) ||
            (type != CLM_BPA_TYPE_STRING && array->items.blocks == NULL)) {
            free(array);
            return CLM_BPA_FAILURE;
        }
    }

    clm_bpa_insert_array(array);
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_create(const clm_id id, const clm_byte type, const clm_int reserved, const clm_byte encoding) {
    if (type == CLM_BPA_TYPE_STRUCT) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_create_sized(id, type, reserved, 0, encoding);
}

int clm_bpaa_delete(const clm_id id) {
    return clm_bpa_remove_array(id);
}

int clm_bpaa_ref(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->refcount == USHRT_MAX) {
        return CLM_BPA_FAILURE;
    }
    ++array->refcount;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_unref(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->refcount == 0) {
        return CLM_BPA_FAILURE;
    }
    if (--array->refcount != 0) {
        return CLM_BPA_SUCCESS;
    }
    return clm_bpaa_delete(id);
}

int clm_bpaa_slice(const clm_id source_id, const clm_int start, const clm_int length, const clm_id destination_id) {
    const clm_bpaa_array_node* source = clm_bpaa_find_array(source_id);
    if (source == NULL || start < 0 || length < 0 ||
        (clm_uint)start > source->count ||
        (clm_uint)length > source->count - (clm_uint)start ||
        clm_bpaa_is_existing(destination_id)) {
        return CLM_BPA_FAILURE;
    }

    if (clm_bpaa_create_sized(destination_id, source->type, length,
                              source->type == CLM_BPA_TYPE_STRUCT ? source->block_size : 0,
                              source->encoding) != CLM_BPA_SUCCESS) {
        return CLM_BPA_FAILURE;
    }

    clm_bpaa_array_node* destination = clm_bpaa_find_array(destination_id);
    if (destination == NULL) return CLM_BPA_FAILURE; /* defensive */
    if (source->type == CLM_BPA_TYPE_STRING) {
        for (clm_uint i = 0; i < (clm_uint)length; ++i) {
            const char* text = source->items.strings[(clm_uint)start + i];
            const size_t bytes = strlen(text) + 1;
            char* copy = clm_bpa_pool_alloc(bytes);
            if (copy == NULL) {
                (void)clm_bpaa_delete(destination_id);
                return CLM_BPA_FAILURE;
            }
            memcpy(copy, text, bytes);
            destination->items.strings[i] = copy;
            ++destination->count; /* cleanup owns each completed copy */
        }
    }
    else if (length != 0) {
        if (source->block_size == 0 ||
            (size_t)length > ((size_t)-1) / source->block_size) {
            (void)clm_bpaa_delete(destination_id);
            return CLM_BPA_FAILURE;
        }
        memcpy(destination->items.blocks,
               source->items.blocks + (size_t)start * source->block_size,
               (size_t)length * source->block_size);
        destination->count = (clm_uint)length;
    }
    destination->deck_size = destination->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_slice_borrow(
    const clm_id source_id,
    const clm_int start,
    const clm_int length,
    const void** buffer,
    size_t* buffer_size
) {
    const clm_bpaa_array_node* source = clm_bpaa_find_array(source_id);
    if (buffer == NULL || buffer_size == NULL) {
        return CLM_BPA_FAILURE;
    }
    *buffer = NULL;
    *buffer_size = 0;
    if (source == NULL || source->type == CLM_BPA_TYPE_STRING ||
        start < 0 || length < 0 || (clm_uint)start > source->count ||
        (clm_uint)length > source->count - (clm_uint)start ||
        source->block_size == 0 ||
        (length != 0 && (size_t)length > ((size_t)-1) / source->block_size)) {
        return CLM_BPA_FAILURE;
    }
    if (length != 0) {
        *buffer = source->items.blocks + (size_t)start * source->block_size;
        *buffer_size = (size_t)length * source->block_size;
    }
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_slice_to(
    const clm_id source_id,
    const clm_int start,
    const clm_int length,
    void* buffer,
    const size_t buffer_size
) {
    const clm_bpaa_array_node* source = clm_bpaa_find_array(source_id);
    if (source == NULL || source->type == CLM_BPA_TYPE_STRING ||
        start < 0 || length < 0 || (clm_uint)start > source->count ||
        (clm_uint)length > source->count - (clm_uint)start) {
        return CLM_BPA_FAILURE;
    }
    if (length == 0) {
        return CLM_BPA_SUCCESS;
    }
    if (source->block_size == 0 || (size_t)length > (size_t)-1 / source->block_size) {
        return CLM_BPA_FAILURE;
    }
    const size_t bytes = (size_t)length * source->block_size;
    if (buffer == NULL || buffer_size < bytes) {
        return CLM_BPA_FAILURE;
    }
    memmove(buffer, source->items.blocks + (size_t)start * source->block_size, bytes);
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_get_raw(const clm_id id, const clm_int index, void* buffer, const clm_uint buffer_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        buffer_size < array->block_size) {
        return CLM_BPA_FAILURE;
    }
    memcpy(buffer, array->items.blocks + (size_t)index * array->block_size, array->block_size);
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_get_string_raw(const clm_id id, const clm_int index, void* buffer, const clm_uint buffer_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type != CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL) {
        return CLM_BPA_FAILURE;
    }
    const clm_uint length = (clm_uint)strlen(array->items.strings[index]);
    const size_t required = (size_t)length + 1;
    if ((size_t)buffer_size < required || length > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    memcpy(buffer, array->items.strings[index], required);
    return (int)length;
}

int clm_bpaa_set_string_raw(const clm_id id, const clm_int index, const void* buffer, const clm_uint buffer_size) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    char* copy;
    clm_uint length;
    if (array == NULL || array->type != CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        clm_bpa_copy_string(buffer, buffer_size, &copy, &length) != CLM_BPA_SUCCESS) {
        return CLM_BPA_FAILURE;
    }
    if (length > INT_MAX) {
        clm_bpa_pool_free(copy, (size_t)length + 1);
        return CLM_BPA_FAILURE;
    }
    clm_bpa_pool_free(array->items.strings[index], strlen(array->items.strings[index]) + 1);
    array->items.strings[index] = copy;
    array->deck_size = array->count;
    return (int)length;
}

int clm_bpaa_get_first_raw(const clm_id id, void* buffer, const clm_uint buffer_size) {
    return clm_bpaa_get_raw(id, 0, buffer, buffer_size);
}

int clm_bpaa_get_last_raw(const clm_id id, void* buffer, const clm_uint buffer_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->count == 0 || array->count - 1 > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_get_raw(id, (clm_int)(array->count - 1), buffer, buffer_size);
}

int clm_bpaa_get_count_to(const clm_id id, clm_uint* count) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || count == NULL) {
        return CLM_BPA_FAILURE;
    }
    *count = array->count;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_get_length_to(const clm_id id, clm_uint* length) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || length == NULL || array->type != CLM_BPA_TYPE_STRING || array->index >= array->count) {
        return CLM_BPA_FAILURE;
    }
    *length = (clm_uint)strlen(array->items.strings[array->index]);
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_get_capacity_to(const clm_id id, clm_uint* capacity) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || capacity == NULL) {
        return CLM_BPA_FAILURE;
    }
    *capacity = array->capacity;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_resize(clm_bpaa_array_node* array, const clm_uint capacity) {
    /* A successful realloc may relocate the payload; borrowed data pointers
     * must therefore not be kept across any operation that changes capacity. */
    void* resized;
    if (array->type == CLM_BPA_TYPE_STRING) {
        if (capacity == 0) {
            free(array->items.strings);
            array->items.strings = NULL;
        }
        else {
            if ((size_t)capacity > (size_t)-1 / sizeof(char*)) {
                return CLM_BPA_FAILURE;
            }
            resized = realloc(array->items.strings, (size_t)capacity * sizeof(char*));
            if (resized == NULL) {
                return CLM_BPA_FAILURE;
            }
            array->items.strings = (char**)resized;
            if (capacity > array->capacity)
                memset(array->items.strings + array->capacity, 0,
                       (size_t)(capacity - array->capacity) * sizeof(char*));
        }
    }
    else {
        if (capacity == 0) {
            free(array->items.blocks);
            array->items.blocks = NULL;
        }
        else {
            if ((size_t)capacity > (size_t)-1 / (size_t)array->block_size) {
                return CLM_BPA_FAILURE;
            }
            resized = realloc(array->items.blocks,
                              (size_t)capacity * array->block_size);
            if (resized == NULL) {
                return CLM_BPA_FAILURE;
            }
            array->items.blocks = (unsigned char*)resized;
        }
    }
    array->capacity = capacity;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_ensure_capacity(clm_bpaa_array_node* array, const clm_uint needed) {
    if (needed <= array->capacity) {
        return CLM_BPA_SUCCESS;
    }
    if (array->sealed) {
        return CLM_BPA_FAILURE;
    }
    /* Geometric growth avoids reallocating for every individual append. */
    clm_uint capacity = array->capacity == 0 ? 4 : array->capacity;
    while (capacity < needed) {
        if (capacity > (clm_uint)-1 >> 1) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    return clm_bpaa_resize(array, capacity);
}

int clm_bpaa_extend(const clm_id id, const clm_uint additional_capacity) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->sealed || additional_capacity == 0 || additional_capacity > (clm_uint)-1 - array->capacity) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_resize(array, array->capacity + additional_capacity);
}

int clm_bpaa_shrink(const clm_id id, const clm_uint new_capacity) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->sealed || new_capacity >= array->capacity || new_capacity < array->count) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_resize(array, new_capacity);
}

int clm_bpaa_get_current_raw(const clm_id id, void* buffer, const clm_uint buffer_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->index >= array->count || array->index > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_get_raw(id, (clm_int)array->index, buffer, buffer_size);
}

int clm_bpaa_set_raw(const clm_id id, const clm_int index, const void* buffer, const clm_uint buffer_size) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        buffer_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }
    memmove(array->items.blocks + (size_t)index * array->block_size, buffer, array->block_size);
    array->deck_size = array->count;
    return CLM_BPA_SET_SUCCESS;
}

int clm_bpaa_insert_raw(const clm_id id, const clm_int index, const void* buffer, const clm_uint buffer_size) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    char* string_copy = NULL;
    clm_uint string_length = 0;
    if (array == NULL || index < 0 || (clm_uint)index > array->count || array->count == (clm_uint)-1) {
        return CLM_BPA_FAILURE;
    }

    if (array->type == CLM_BPA_TYPE_STRING) {
        if (clm_bpa_copy_string(buffer, buffer_size, &string_copy, &string_length) != CLM_BPA_SUCCESS) {
            return CLM_BPA_FAILURE;
        }
    }
    else if (buffer == NULL || buffer_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }

    /* Snapshot the source before realloc/memmove: it may alias this array. */
    void* value_copy = NULL;
    if (array->type != CLM_BPA_TYPE_STRING) {
        value_copy = malloc(array->block_size);
        if (value_copy == NULL) {
            return CLM_BPA_FAILURE;
        }
        memcpy(value_copy, buffer, array->block_size);
    }
    if (clm_bpaa_ensure_capacity(array, array->count + 1) != CLM_BPA_SUCCESS) {
        clm_bpa_pool_free(string_copy, (size_t)string_length + 1);
        free(value_copy);
        return CLM_BPA_FAILURE;
    }
    if (array->type == CLM_BPA_TYPE_STRING) {
        memmove(array->items.strings + index + 1, array->items.strings + index,
                (size_t)(array->count - (clm_uint)index) * sizeof(char*));
        array->items.strings[index] = string_copy;
    }
    else {
        unsigned char* destination = array->items.blocks + (size_t)index * array->block_size;
        memmove(destination + array->block_size, destination,
                (size_t)(array->count - (clm_uint)index) * array->block_size);
        memcpy(destination, value_copy, array->block_size);
    }
    free(value_copy);
    ++array->count;
    array->index = (clm_uint)index;
    array->deck_size = array->count;
    (void)string_length;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_append_raw(const clm_id id, const void* buffer, const clm_uint buffer_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->count > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_insert_raw(id, (clm_int)array->count, buffer, buffer_size);
}

int clm_bpaa_remove(const clm_id id, const clm_int index) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || index < 0 || (clm_uint)index >= array->count) {
        return CLM_BPA_FAILURE;
    }
    if (array->type == CLM_BPA_TYPE_STRING) {
        clm_bpa_pool_free(array->items.strings[index], strlen(array->items.strings[index]) + 1);
        memmove(array->items.strings + index, array->items.strings + index + 1,
                (size_t)(array->count - (clm_uint)index - 1) * sizeof(char*));
        array->items.strings[array->count - 1] = NULL;
    }
    else {
        unsigned char* item = array->items.blocks + (size_t)index * array->block_size;
        memmove(item, item + array->block_size,
                (size_t)(array->count - (clm_uint)index - 1) * array->block_size);
    }
    --array->count;
    if (array->count == 0) {
        array->index = 0;
    }
    else if (array->index > (clm_uint)index) {
        --array->index;
    }
    else if (array->index >= array->count) {
        array->index = array->count - 1;
    }
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_remove_first(const clm_id id) {
    return clm_bpaa_remove(id, 0);
}

int clm_bpaa_remove_last(const clm_id id) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->count == 0 || array->count - 1 > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_remove(id, (clm_int)(array->count - 1));
}

int clm_bpaa_clear(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL) {
        return CLM_BPA_FAILURE;
    }
    if (array->type == CLM_BPA_TYPE_STRING) {
        for (clm_uint index = 0; index < array->count; ++index) {
            clm_bpa_pool_free(array->items.strings[index], strlen(array->items.strings[index]) + 1);
            array->items.strings[index] = NULL;
        }
    }
    else if (array->capacity != 0) {
        memset(array->items.blocks, 0, (size_t)array->capacity * array->block_size);
    }
    array->count = 0;
    array->index = 0;
    array->deck_size = 0;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_advanced_borrow(const clm_id id, clm_bpaa_array_node** out_array) {
    if (out_array == NULL) {
        return CLM_BPA_FAILURE;
    }
    *out_array = NULL;
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->refcount == USHRT_MAX) {
        return CLM_BPA_FAILURE;
    }
    ++array->refcount;
    *out_array = array;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_advanced_release(clm_bpaa_array_node** borrowed) {
    if (borrowed == NULL || *borrowed == NULL) {
        return CLM_BPA_FAILURE;
    }
    clm_bpaa_array_node* array = *borrowed;
    if (array->refcount == 0) {
        return CLM_BPA_FAILURE;
    }
    const clm_id id = array->id;
    *borrowed = NULL;
    if (--array->refcount != 0) {
        return CLM_BPA_SUCCESS;
    }
    return clm_bpa_remove_array(id);
}

void* clm_bpaa_advanced_data(const clm_bpaa_array_node* array) {
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING) {
        return NULL;
    }
    return array->items.blocks;
}

int clm_bpaa_advanced_get_raw(
    const clm_bpaa_array_node* array,
    const clm_int index,
    void* buffer,
    const clm_uint buffer_size) {
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        buffer_size < array->block_size) {
        return CLM_BPA_FAILURE;
    }
    memcpy(buffer, array->items.blocks + (size_t)index * array->block_size, array->block_size);
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_advanced_set_raw(
    clm_bpaa_array_node* array,
    const clm_int index,
    const void* buffer,
    const clm_uint buffer_size
) {
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        buffer_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }
    memmove(array->items.blocks + (size_t)index * array->block_size, buffer, array->block_size);
    array->deck_size = array->count;
    return CLM_BPA_SET_SUCCESS;
}

int clm_bpaa_advanced_insert_raw(
    clm_bpaa_array_node* array,
    const clm_int index,
    const void* buffer,
    const clm_uint buffer_size) {
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index > array->count || array->count >= INT_MAX ||
        buffer == NULL || buffer_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }
    void* value_copy = malloc(array->block_size);
    if (value_copy == NULL) {
        return CLM_BPA_FAILURE;
    }
    memcpy(value_copy, buffer, array->block_size);
    if (clm_bpaa_ensure_capacity(array, array->count + 1) != CLM_BPA_SUCCESS) {
        free(value_copy);
        return CLM_BPA_FAILURE;
    }
    unsigned char* destination = array->items.blocks + (size_t)index * array->block_size;
    memmove(destination + array->block_size, destination,
            (size_t)(array->count - (clm_uint)index) * array->block_size);
    memcpy(destination, value_copy, array->block_size);
    free(value_copy);
    ++array->count;
    array->index = (clm_uint)index;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_advanced_append_raw(
    clm_bpaa_array_node* array,
    const void* buffer,
    const clm_uint buffer_size) {
    if (array == NULL || array->count > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_advanced_insert_raw(array, (clm_int)array->count, buffer, buffer_size);
}

int clm_bpaa_advanced_remove(clm_bpaa_array_node* array, const clm_int index) {
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 || (clm_uint)index >= array->count) {
        return CLM_BPA_FAILURE;
    }
    unsigned char* item = array->items.blocks + (size_t)index * array->block_size;
    memmove(item, item + array->block_size,
            (size_t)(array->count - (clm_uint)index - 1) * array->block_size);
    --array->count;
    if (array->count == 0) {
        array->index = 0;
    }
    else if (array->index > (clm_uint)index) {
        --array->index;
    }
    else if (array->index >= array->count) {
        array->index = array->count - 1;
    }
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_advanced_clear(clm_bpaa_array_node* array) {
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING) {
        return CLM_BPA_FAILURE;
    }
    if (array->capacity != 0)
        memset(array->items.blocks, 0, (size_t)array->capacity * array->block_size);
    array->count = 0;
    array->index = 0;
    array->deck_size = 0;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_advanced_extend(clm_bpaa_array_node* array, const clm_uint additional_capacity) {
    if (array == NULL || array->sealed || additional_capacity == 0 ||
        additional_capacity > (clm_uint)-1 - array->capacity) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_resize(array, array->capacity + additional_capacity);
}

int clm_bpaa_advanced_shrink(clm_bpaa_array_node* array, const clm_uint new_capacity) {
    if (array == NULL || array->sealed || new_capacity >= array->capacity || new_capacity < array->count) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_resize(array, new_capacity);
}

int clm_bpaa_reset(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL) {
        return CLM_BPA_FAILURE;
    }
    array->index = 0;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_move_first(const clm_id id) {
    return clm_bpaa_reset(id);
}

int clm_bpaa_move_previous(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->index == 0) {
        return CLM_BPA_FAILURE;
    }
    --array->index;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_move_next(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->index + 1 >= array->count) {
        return CLM_BPA_FAILURE;
    }
    ++array->index;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_move_last(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->count == 0) {
        return CLM_BPA_FAILURE;
    }
    array->index = array->count - 1;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_move_to(const clm_id id, const clm_int index) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || index < 0 || (clm_uint)index >= array->count) {
        return CLM_BPA_FAILURE;
    }
    array->index = (clm_uint)index;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_get_current_index_to(const clm_id id, clm_int* index) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || index == NULL || array->index > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    *index = (clm_int)array->index;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_seal(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL) {
        return CLM_BPA_FAILURE;
    }
    /* A sealed array may still change within its current reserve. */
    array->sealed = 1;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_is_sealed_to(const clm_id id, clm_byte* sealed) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || sealed == NULL) {
        return CLM_BPA_FAILURE;
    }
    *sealed = array->sealed;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_unseal(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL) {
        return CLM_BPA_FAILURE;
    }
    array->sealed = 0;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_sort_with(const clm_id id, const clm_bpaa_compare_fn compare) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || compare == NULL) {
        return CLM_BPA_FAILURE;
    }
    if (array->count < 2) {
        return CLM_BPA_SUCCESS;
    }
    qsort(array->items.blocks, array->count, array->block_size, compare);
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_sort(const clm_id id, const clm_bpaa_predicate_fn predicate) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || predicate == NULL || array->type == CLM_BPA_TYPE_STRING) {
        return CLM_BPA_FAILURE;
    }
    if (array->count < 2) {
        return CLM_BPA_SUCCESS;
    }
    const size_t block_size = array->block_size;
    unsigned char* temporary = malloc(block_size);
    if (temporary == NULL) {
        return CLM_BPA_FAILURE;
    }
    for (clm_uint i = 0; i < array->count - 1; ++i) {
        int swapped = 0;
        for (clm_uint j = 0; j < array->count - i - 1; ++j) {
            unsigned char* left = array->items.blocks + (size_t)j * block_size;
            unsigned char* right = left + block_size;
            if (predicate(left, right) > 0) {
                memcpy(temporary, left, block_size);
                memcpy(left, right, block_size);
                memcpy(right, temporary, block_size);
                swapped = 1;
            }
        }
        if (!swapped) {
            break;
        }
    }
    free(temporary);
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_reverse(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING) {
        return CLM_BPA_FAILURE;
    }
    if (array->count < 2) {
        return CLM_BPA_SUCCESS;
    }
    unsigned char* temporary = malloc(array->block_size);
    if (temporary == NULL) {
        return CLM_BPA_FAILURE;
    }
    clm_uint left = 0;
    clm_uint right = array->count - 1;
    while (left < right) {
        unsigned char* left_item = array->items.blocks + (size_t)left * array->block_size;
        unsigned char* right_item = array->items.blocks + (size_t)right * array->block_size;
        memcpy(temporary, left_item, array->block_size);
        memcpy(left_item, right_item, array->block_size);
        memcpy(right_item, temporary, array->block_size);
        ++left;
        --right;
    }
    free(temporary);
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_rotate(clm_bpaa_array_node* array, const clm_uint index, const clm_uint positions, const int right) {
    if (array->type == CLM_BPA_TYPE_STRING || index >= array->count) {
        return CLM_BPA_FAILURE;
    }
    const clm_uint length = array->count - index;
    const clm_uint shift = positions % length;
    if (shift == 0) {
        return CLM_BPA_SUCCESS;
    }
    if (array->block_size == 0 || (size_t)shift > (size_t)-1 / (size_t)array->block_size) {
        return CLM_BPA_FAILURE;
    }
    unsigned char* temporary = malloc((size_t)shift * array->block_size);
    if (temporary == NULL) {
        return CLM_BPA_FAILURE;
    }
    unsigned char* suffix = array->items.blocks + (size_t)index * array->block_size;
    if (right) {
        memcpy(temporary, suffix + (size_t)(length - shift) * array->block_size,
               (size_t)shift * array->block_size);
        memmove(suffix + (size_t)shift * array->block_size, suffix,
                (size_t)(length - shift) * array->block_size);
        memcpy(suffix, temporary, (size_t)shift * array->block_size);
    }
    else {
        memcpy(temporary, suffix, (size_t)shift * array->block_size);
        memmove(suffix, suffix + (size_t)shift * array->block_size,
                (size_t)(length - shift) * array->block_size);
        memcpy(suffix + (size_t)(length - shift) * array->block_size,
               temporary, (size_t)shift * array->block_size);
    }
    free(temporary);
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_shift_left(const clm_id id, const clm_uint index, const clm_uint positions) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_rotate(array, index, positions, 0);
}

int clm_bpaa_shift_right(const clm_id id, const clm_uint index, const clm_uint positions) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_rotate(array, index, positions, 1);
}

int clm_bpaa_swap(const clm_id id, const clm_int index1, const clm_int index2) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index1 < 0 ||
        index2 < 0 || (clm_uint)index1 >= array->count ||
        (clm_uint)index2 >= array->count) {
        return CLM_BPA_FAILURE;
    }
    if (index1 == index2) {
        return CLM_BPA_SUCCESS;
    }
    unsigned char* temporary = malloc(array->block_size);
    if (temporary == NULL) {
        return CLM_BPA_FAILURE;
    }
    unsigned char* first = array->items.blocks + (size_t)index1 * array->block_size;
    unsigned char* second = array->items.blocks + (size_t)index2 * array->block_size;
    memcpy(temporary, first, array->block_size);
    memcpy(first, second, array->block_size);
    memcpy(second, temporary, array->block_size);
    free(temporary);
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_copy(clm_id id, const clm_int source_index, const clm_int dest_index) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || source_index < 0 ||
        dest_index < 0 || (clm_uint)source_index >= array->count ||
        (clm_uint)dest_index >= array->count) {
        return CLM_BPA_FAILURE;
    }
    memmove(array->items.blocks + (size_t)dest_index * array->block_size,
            array->items.blocks + (size_t)source_index * array->block_size,
            array->block_size);
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_shuffle(const clm_id id) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || array->count > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    /* Shuffle the stored items themselves; the deck is the active prefix
     * [0, deck_size), consumed by draw_at/draw without replacement. */
    for (clm_uint index = array->count; index > 1; --index) {
        const clm_uint other = (clm_uint)((unsigned long)rand() % index); // NOLINT(*-msc50-cpp)
        if (clm_bpaa_swap(id, (clm_int)(index - 1), (clm_int)other) != CLM_BPA_SUCCESS) {
            return CLM_BPA_FAILURE;
        }
    }
    array->deck_size = array->count;
    array->index = 0;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_draw_at_raw(const clm_id id, const clm_int index, void* buffer, const clm_uint buffer_size) {
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || buffer == NULL ||
        buffer_size < array->block_size || index < 0 ||
        (clm_uint)index >= array->deck_size) {
        return CLM_BPA_FAILURE;
    }
    /* Swap the selected entry to the deck tail, then shrink the active deck.
     * This makes each draw O(1) without moving the remaining entries. */
    const clm_uint last = array->deck_size - 1;
    if ((clm_uint)index != last) {
        unsigned char* selected = array->items.blocks + (size_t)index * array->block_size;
        unsigned char* tail = array->items.blocks + (size_t)last * array->block_size;
        unsigned char* temporary = malloc(array->block_size);
        if (temporary == NULL) {
            return CLM_BPA_FAILURE;
        }
        memcpy(temporary, selected, array->block_size);
        memcpy(selected, tail, array->block_size);
        memcpy(tail, temporary, array->block_size);
        free(temporary);
    }
    memcpy(buffer, array->items.blocks + (size_t)last * array->block_size, array->block_size);
    --array->deck_size;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_draw_raw(const clm_id id, void* buffer, const clm_uint buffer_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || buffer == NULL || buffer_size < array->block_size) {
        return CLM_BPA_FAILURE;
    }
    if (array->deck_size == 0) {
        /* Starting another draw cycle reshuffles the items in place. */
        if (array->count == 0 || clm_bpaa_shuffle(id) != CLM_BPA_SUCCESS) {
            return CLM_BPA_FAILURE;
        }
    }
    return clm_bpaa_draw_at_raw(id, 0, buffer, buffer_size);
}

int clm_bpa_borrow(const clm_id id, void** address, size_t* size) {
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || address == NULL || size == NULL) {
        return CLM_BPA_FAILURE;
    }
    *address = block->data;
    *size = block->type == CLM_BPA_TYPE_STRING
                ? (size_t)block->length + 1
                : block->block_size;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpa_copy_block(const clm_id source_id, const clm_id destination_id) {
    clm_bpa_block_node* source = clm_bpa_find_block(source_id);
    clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    if (source == NULL || destination == NULL || source->type != destination->type) {
        return CLM_BPA_FAILURE;
    }
    if (source->type == CLM_BPA_TYPE_STRING) {
        if (source->data == NULL) {
            return CLM_BPA_FAILURE;
        }
        char* fresh = clm_bpa_pool_alloc((size_t)source->length + 1);
        if (fresh == NULL) {
            return CLM_BPA_FAILURE;
        }
        memcpy(fresh, source->data, (size_t)source->length + 1);
        clm_bpa_pool_free(destination->data, (size_t)destination->length + 1);
        destination->data = fresh;
        destination->length = source->length;
        return CLM_BPA_SUCCESS;
    }
    if (source->block_size != destination->block_size) {
        return CLM_BPA_FAILURE;
    }
    memmove(destination->data, source->data, source->block_size);
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_get(const clm_id array_id, const clm_int index, const clm_id block_id, const size_t expected_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count ||
        (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        array->block_size != block->block_size || expected_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }
    memcpy(block->data, array->items.blocks + (size_t)index * array->block_size, array->block_size);
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_set(const clm_id array_id, const clm_int index, const clm_id block_id, const size_t expected_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count ||
        (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        array->block_size != block->block_size || expected_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_set_raw(array_id, index, block->data, (clm_uint)expected_size);
}

int clm_bpaa_insert(const clm_id array_id, const clm_int index, const clm_id block_id, const size_t expected_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING ||
        (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        array->block_size != block->block_size || expected_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_insert_raw(array_id, index, block->data, (clm_uint)expected_size);
}

int clm_bpaa_append(const clm_id array_id, const clm_id block_id, const size_t expected_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    if (array == NULL || array->count > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_insert(array_id, (clm_int)array->count, block_id, expected_size);
}

int clm_bpaa_get_string(const clm_id array_id, const clm_int index, const clm_id block_id) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type != CLM_BPA_TYPE_STRING ||
        block->type != CLM_BPA_TYPE_STRING || index < 0 || (clm_uint)index >= array->count) {
        return CLM_BPA_FAILURE;
    }
    const size_t len = strlen(array->items.strings[index]);
    if (len > (size_t)INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    char* fresh = clm_bpa_pool_alloc(len + 1);
    if (fresh == NULL) {
        return CLM_BPA_FAILURE;
    }
    memcpy(fresh, array->items.strings[index], len + 1);
    clm_bpa_pool_free(block->data, (size_t)block->length + 1);
    block->data = fresh;
    block->length = (clm_uint)len;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_set_string(const clm_id array_id, const clm_int index, const clm_id block_id) {
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || block->data == NULL) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_set_string_raw(array_id, index, block->data, block->length + 1) < 0
               ? CLM_BPA_FAILURE
               : CLM_BPA_SET_SUCCESS;
}

int clm_bpaa_insert_string(const clm_id array_id, const clm_int index, const clm_id block_id) {
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || block->data == NULL) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_insert_raw(array_id, index, block->data, block->length + 1);
}

int clm_bpaa_append_string(const clm_id array_id, const clm_id block_id) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    if (array == NULL || array->count > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_insert_string(array_id, (clm_int)array->count, block_id);
}

int clm_bpaa_draw(const clm_id array_id, const clm_id block_id, const size_t expected_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING || (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        block->block_size != array->block_size ||
        expected_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_draw_raw(array_id, block->data, block->block_size);
}

int clm_bpaa_draw_at(const clm_id array_id, const clm_int index, const clm_id block_id, const size_t expected_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING || (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        block->block_size != array->block_size ||
        expected_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_draw_at_raw(array_id, index, block->data, block->block_size);
}

int clm_bpa_get_to(const clm_id id, void* destination, const clm_uint size) {
    return clm_bpa_get(id, destination, size);
}

int clm_bpa_set_from(const clm_id id, const void* source, const clm_uint size) {
    return clm_bpa_set(id, source, size);
}

int clm_bpa_get_string_to(const clm_id id, void* destination, const clm_uint size) {
    return clm_bpa_get_string(id, destination, size);
}

int clm_bpa_set_string_from(const clm_id id, const void* source, const clm_uint size) {
    return clm_bpa_set_string(id, source, size);
}

int clm_bpaa_get_to(const clm_id id, const clm_int index, void* destination, const clm_uint size) {
    return clm_bpaa_get_raw(id, index, destination, size);
}

int clm_bpaa_get_first_to(const clm_id id, void* destination, const clm_uint size) {
    return clm_bpaa_get_first_raw(id, destination, size);
}

int clm_bpaa_get_last_to(const clm_id id, void* destination, const clm_uint size) {
    return clm_bpaa_get_last_raw(id, destination, size);
}

int clm_bpaa_get_current_to(const clm_id id, void* destination, const clm_uint size) {
    return clm_bpaa_get_current_raw(id, destination, size);
}

int clm_bpaa_get_string_to(const clm_id id, const clm_int index, void* destination, const clm_uint size) {
    return clm_bpaa_get_string_raw(id, index, destination, size);
}

int clm_bpaa_set_from(const clm_id id, const clm_int index, const void* source, const clm_uint size) {
    return clm_bpaa_set_raw(id, index, source, size);
}

int clm_bpaa_set_string_from(const clm_id id, const clm_int index, const void* source, const clm_uint size) {
    return clm_bpaa_set_string_raw(id, index, source, size);
}

int clm_bpaa_insert_from(const clm_id id, const clm_int index, const void* source, const clm_uint size) {
    return clm_bpaa_insert_raw(id, index, source, size);
}

int clm_bpaa_append_from(const clm_id id, const void* source, const clm_uint size) {
    return clm_bpaa_append_raw(id, source, size);
}

int clm_bpaa_draw_to(const clm_id id, void* destination, const clm_uint size) {
    return clm_bpaa_draw_raw(id, destination, size);
}

int clm_bpaa_draw_at_to(const clm_id id, const clm_int index, void* destination, const clm_uint size) {
    return clm_bpaa_draw_at_raw(id, index, destination, size);
}

int clm_bpaa_write_u32(const clm_id destination_id, const clm_uint value) {
    const clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    if (destination == NULL || destination->type != CLM_BPA_TYPE_UNSIGNED_INT ||
        destination->block_size != sizeof(value) || destination->data == NULL) {
        return CLM_BPA_FAILURE;
    }
    memcpy(destination->data, &value, sizeof(value));
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_write_i32(const clm_id destination_id, const clm_int value) {
    const clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    if (destination == NULL || destination->type != CLM_BPA_TYPE_INT ||
        destination->block_size != sizeof(value) || destination->data == NULL) {
        return CLM_BPA_FAILURE;
    }
    memcpy(destination->data, &value, sizeof(value));
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_write_byte(const clm_id destination_id, const clm_byte value) {
    const clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    if (destination == NULL || destination->type != CLM_BPA_TYPE_BYTE ||
        destination->block_size != sizeof(value) || destination->data == NULL) {
        return CLM_BPA_FAILURE;
    }
    memcpy(destination->data, &value, sizeof(value));
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_get_count(const clm_id array_id, const clm_id destination_id) {
    clm_uint value;
    return clm_bpaa_get_count_to(array_id, &value) == CLM_BPA_GET_SUCCESS
               ? clm_bpaa_write_u32(destination_id, value)
               : CLM_BPA_FAILURE;
}

int clm_bpaa_get_capacity(const clm_id array_id, const clm_id destination_id) {
    clm_uint value;
    return clm_bpaa_get_capacity_to(array_id, &value) == CLM_BPA_GET_SUCCESS
               ? clm_bpaa_write_u32(destination_id, value)
               : CLM_BPA_FAILURE;
}

int clm_bpaa_get_length(const clm_id array_id, const clm_id destination_id) {
    clm_uint value;
    return clm_bpaa_get_length_to(array_id, &value) == CLM_BPA_GET_SUCCESS
               ? clm_bpaa_write_u32(destination_id, value)
               : CLM_BPA_FAILURE;
}

int clm_bpaa_get_current_index(const clm_id array_id, const clm_id destination_id) {
    clm_int value;
    return clm_bpaa_get_current_index_to(array_id, &value) == CLM_BPA_GET_SUCCESS
               ? clm_bpaa_write_i32(destination_id, value)
               : CLM_BPA_FAILURE;
}

int clm_bpaa_is_sealed(const clm_id array_id, const clm_id destination_id) {
    clm_byte value;
    return clm_bpaa_is_sealed_to(array_id, &value) == CLM_BPA_GET_SUCCESS
               ? clm_bpaa_write_byte(destination_id, value)
               : CLM_BPA_FAILURE;
}

int clm_bpaa_get_first(const clm_id array_id, const clm_id destination_id, const size_t expected_size) {
    return clm_bpaa_get(array_id, 0, destination_id, expected_size);
}

int clm_bpaa_get_last(const clm_id array_id, const clm_id destination_id, const size_t expected_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    if (array == NULL || array->count == 0 || array->count - 1 > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_get(array_id, (clm_int)(array->count - 1), destination_id, expected_size);
}

int clm_bpaa_get_current(const clm_id array_id, const clm_id destination_id, const size_t expected_size) {
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    if (array == NULL || array->index >= array->count || array->index > INT_MAX) {
        return CLM_BPA_FAILURE;
    }
    return clm_bpaa_get(array_id, (clm_int)array->index, destination_id, expected_size);
}

int clm_bpa_shutdown(void) {
    size_t i;

    /* Release the arrays first, including their individually pooled strings. */
    for (i = 0; i < clm_bpa_global_arrays_capacity; ++i) {
        const clm_bpaa_array_node* array = clm_bpa_global_arrays[i];
        if (array != NULL && array != CLM_BPA_ARRAY_TOMBSTONE) {
            (void)clm_bpa_remove_array(array->id);
        }
    }
    free(clm_bpa_global_arrays);
    clm_bpa_global_arrays = NULL;
    clm_bpa_global_arrays_capacity = 0;
    clm_bpa_global_arrays_count = 0;
    clm_bpa_global_arrays_tombstones = 0;

    for (i = 0; i < clm_bpa_global_blocks_capacity; ++i) {
        const clm_bpa_block_node* block = clm_bpa_global_blocks[i];
        if (block != NULL && block != CLM_BPA_BLOCK_TOMBSTONE) {
            (void)clm_bpa_remove_block(block->id);
        }
    }
    free(clm_bpa_global_blocks);
    clm_bpa_global_blocks = NULL;
    clm_bpa_global_blocks_capacity = 0;
    clm_bpa_global_blocks_count = 0;
    clm_bpa_global_blocks_tombstones = 0;

    const int status = clm_bpa_pool_cleanup();
    return status;
}