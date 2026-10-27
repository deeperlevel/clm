#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#include "base.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CLM_BPA_FAILURE (-1)
#define CLM_BPA_SUCCESS 0
#define CLM_BPA_GET_SUCCESS 1
#define CLM_BPA_SET_SUCCESS 2

#define CLM_BPA_TYPE_STRUCT 0
#define CLM_BPA_TYPE_SIGNED_BYTE 1
#define CLM_BPA_TYPE_BYTE 2
#define CLM_BPA_TYPE_SHORT 3
#define CLM_BPA_TYPE_UNSIGNED_SHORT 4
#define CLM_BPA_TYPE_INT 5
#define CLM_BPA_TYPE_UNSIGNED_INT 6
#define CLM_BPA_TYPE_LONG 7
#define CLM_BPA_TYPE_UNSIGNED_LONG 8
#define CLM_BPA_TYPE_FLOAT 9
#define CLM_BPA_TYPE_DOUBLE 10
#define CLM_BPA_TYPE_STRING 11
#define CLM_BPA_TYPE_UUID 12
#define CLM_BPA_TYPE_DATETIME 13

typedef struct {
    clm_byte type;
    clm_ushort refcount;
    clm_uint block_size;
    clm_byte encoding;
    clm_uint length;
    uintptr_t pointer;
} clm_block_allocation;

typedef struct {
    clm_byte type;
    clm_ushort refcount;
    clm_uint count;
    clm_uint capacity;
    clm_uint block_size;
    clm_byte encoding;
    clm_uint index;
    clm_byte sealed;
    clm_uint deck_size;
    uintptr_t pointer;
} clm_block_array_allocation;

typedef struct clm_bpa_block_node {
    clm_id id;
    clm_byte type;
    clm_ushort refcount;
    clm_uint block_size;
    clm_byte encoding;
    clm_uint length;
    void* data;
} clm_bpa_block_node;

typedef struct clm_bpaa_array_node {
    clm_id id;
    clm_byte type;
    clm_ushort refcount;
    clm_uint count;
    clm_uint capacity;
    clm_uint block_size;
    clm_byte encoding;
    clm_uint index;
    clm_byte sealed;
    clm_uint deck_size;
    union {
        unsigned char* blocks;
        char** strings;
    } items;
} clm_bpaa_array_node;

extern clm_bpa_block_node clm_bpa_global_block_tombstone;
extern clm_bpa_block_node** clm_bpa_global_blocks;
extern size_t clm_bpa_global_blocks_capacity;
extern size_t clm_bpa_global_blocks_count;
extern size_t clm_bpa_global_blocks_tombstones;
extern clm_bpaa_array_node clm_bpa_global_array_tombstone;
extern clm_bpaa_array_node** clm_bpa_global_arrays;
extern size_t clm_bpa_global_arrays_capacity;
extern size_t clm_bpa_global_arrays_count;
extern size_t clm_bpa_global_arrays_tombstones;

#include <stdbool.h>

typedef int (*clm_bpaa_compare_fn)(
    const void* left,
    const void* right
);

typedef bool (*clm_bpaa_predicate_fn)(
    const void* left,
    const void* right
);

/*
 * Define CLM_BPA_IMPLEMENTATION in exactly one source file before including
 * this header to provide the shared global registries.
 */

#define CLM_BPA_INITIAL_CAPACITY 16u
#define CLM_BPA_BLOCK_TOMBSTONE (&clm_bpa_global_block_tombstone)
#define CLM_BPA_ARRAY_TOMBSTONE (&clm_bpa_global_array_tombstone)


/* A bounded set of recyclable size-class arenas. Each arena obtains slabs of
 * CLM_BPA_POOL_SLAB_SLOTS aligned slots, then reuses released slots. Large
 * allocations fall back to malloc/free. All objects in a slab stay allocated
 * until clm_bpa_pool_cleanup() is called after releasing live resources. */
#define CLM_BPA_POOL_SLAB_SLOTS 32u
#define CLM_BPA_POOL_MAX_SLOT 4096u
#define CLM_BPA_POOL_CLASSES 10u /* 8, 16, 32, ... 4096 */

typedef struct clm_bpa_pool_slab {
    struct clm_bpa_pool_slab* next;
    void* memory;
} clm_bpa_pool_slab;

typedef struct {
    size_t slot_size;
    void* free_list;
    clm_bpa_pool_slab* slabs;
    size_t active;
    size_t capacity;
} clm_bpa_pool_class;

extern clm_bpa_pool_class clm_bpa_global_pools[CLM_BPA_POOL_CLASSES];

/** Selects the smallest pooled slot class that can hold the requested size.
 * @param bytes Requested allocation size; zero and oversized values use no class.
 * @return Pool-class index, or CLM_BPA_POOL_CLASSES for the heap fallback.
 */
static inline size_t clm_bpa_pool_class_for(const size_t bytes)
{
    size_t index = 0;
    size_t size = sizeof(void*) > 8 ? sizeof(void*) : 8;
    if (bytes == 0 || bytes > CLM_BPA_POOL_MAX_SLOT)
        return CLM_BPA_POOL_CLASSES;
    while (size < bytes && index + 1 < CLM_BPA_POOL_CLASSES) {
        size *= 2;
        ++index;
    }
    return size >= bytes ? index : CLM_BPA_POOL_CLASSES;
}

/** Allocates zero-initialized storage from a slab pool or the heap fallback.
 * @param bytes Number of bytes requested; zero returns NULL.
 * @return A suitably aligned allocation, or NULL if allocation fails.
 */
static inline void* clm_bpa_pool_alloc(const size_t bytes)
{
    const size_t idx = clm_bpa_pool_class_for(bytes);
    if (idx == CLM_BPA_POOL_CLASSES)
        return bytes ? calloc(1, bytes) : NULL;
    clm_bpa_pool_class* pool = &clm_bpa_global_pools[idx];
    if (pool->slot_size == 0) {
        size_t size = sizeof(void*) > 8 ? sizeof(void*) : 8;
        for (size_t i = 0; i < idx; ++i) size *= 2;
        /* malloc alignment is sufficient for fundamental C object types. */
        pool->slot_size = size;
    }
    if (pool->free_list == NULL) {
        const size_t stride = pool->slot_size;
        if (stride > ((size_t)-1 / CLM_BPA_POOL_SLAB_SLOTS))
            return NULL;
        clm_bpa_pool_slab* slab = malloc(sizeof(*slab));
        if (slab == NULL) return NULL;
        unsigned char* memory = malloc(stride * CLM_BPA_POOL_SLAB_SLOTS);
        if (memory == NULL) { free(slab); return NULL; }
        slab->memory = memory;
        slab->next = pool->slabs;
        pool->slabs = slab;
        for (size_t i = 0; i < CLM_BPA_POOL_SLAB_SLOTS; ++i) {
            void* slot = memory + i * stride;
            *(void**)slot = pool->free_list;
            pool->free_list = slot;
        }
        pool->capacity += CLM_BPA_POOL_SLAB_SLOTS;
    }
    void* result = pool->free_list;
    pool->free_list = *(void**)result;
    ++pool->active;
    memset(result, 0, bytes);
    return result;
}

/** Returns a pooled allocation to its free list, or frees a heap fallback.
 * @param pointer Allocation to release; NULL is ignored.
 * @param bytes Original allocation size, used to select the matching class.
 */
static inline void clm_bpa_pool_free(void* pointer, const size_t bytes)
{
    if (pointer == NULL) return;
    const size_t idx = clm_bpa_pool_class_for(bytes);
    if (idx == CLM_BPA_POOL_CLASSES) { free(pointer); return; }
    clm_bpa_pool_class* pool = &clm_bpa_global_pools[idx];
    *(void**)pointer = pool->free_list;
    pool->free_list = pointer;
    --pool->active;
}

/* Call only when all BPA blocks are released. Fails rather than invalidating
 * live allocations. Array allocations are independent of these slab pools. */
/** Releases all pool slabs after verifying that no pooled objects remain live.
 * @return CLM_BPA_SUCCESS when cleaned up; CLM_BPA_FAILURE if a class is active.
 */
static inline int clm_bpa_pool_cleanup(void)
{
    for (size_t i = 0; i < CLM_BPA_POOL_CLASSES; ++i)
        if (clm_bpa_global_pools[i].active != 0)
            return CLM_BPA_FAILURE;
    for (size_t i = 0; i < CLM_BPA_POOL_CLASSES; ++i) {
        clm_bpa_pool_class* pool = &clm_bpa_global_pools[i];
        clm_bpa_pool_slab* slab = pool->slabs;
        while (slab != NULL) {
            clm_bpa_pool_slab* next = slab->next;
            free(slab->memory);
            free(slab);
            slab = next;
        }
        memset(pool, 0, sizeof(*pool));
    }
    return CLM_BPA_SUCCESS;
}

/** Reports usage for the pool class selected by a requested object size.
 * @param bytes Size used to select a pooled class.
 * @param active Receives the number of currently checked-out slots.
 * @param capacity Receives the total slots reserved by that class.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid size or output.
 */
static inline int clm_bpa_pool_stats(const size_t bytes, size_t* active, size_t* capacity)
{
    const size_t idx = clm_bpa_pool_class_for(bytes);
    if (idx == CLM_BPA_POOL_CLASSES || active == NULL || capacity == NULL)
        return CLM_BPA_FAILURE;
    *active = clm_bpa_global_pools[idx].active;
    *capacity = clm_bpa_global_pools[idx].capacity;
    return CLM_BPA_SUCCESS;
}

/** Mixes a 64-bit ID into a hash suitable for the global open-addressed tables.
 * @param id Identifier to hash.
 * @return Mixed hash value; callers mask it to the table capacity.
 */
static inline size_t clm_bpa_hash_id(clm_id id)
{
    id ^= id >> 30;
    id *= UINT64_C(0xbf58476d1ce4e5b9);
    id ^= id >> 27;
    id *= UINT64_C(0x94d049bb133111eb);
    id ^= id >> 31;
    return id;
}

/** Rebuilds the block registry at a new power-of-two capacity.
 * @param capacity New slot count; must be a nonzero power of two.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if allocation fails.
 */
static inline int clm_bpa_rehash_blocks(const size_t capacity)
{
    clm_bpa_block_node** slots =
        calloc(capacity, sizeof(*slots));
    if (slots == NULL)
        return CLM_BPA_FAILURE;
    for (size_t index = 0; index < clm_bpa_global_blocks_capacity; ++index) {
        clm_bpa_block_node* block = clm_bpa_global_blocks[index];
        if (block != NULL && block != CLM_BPA_BLOCK_TOMBSTONE) {
            size_t slot = clm_bpa_hash_id(block->id) & (capacity - 1);
            while (slots[slot] != NULL)
                slot = slot + 1 & capacity - 1;
            slots[slot] = block;
        }
    }
    free(clm_bpa_global_blocks);
    clm_bpa_global_blocks = slots;
    clm_bpa_global_blocks_capacity = capacity;
    clm_bpa_global_blocks_tombstones = 0;
    return CLM_BPA_SUCCESS;
}

/** Rebuilds the array registry at a new power-of-two capacity.
 * @param capacity New slot count; must be a nonzero power of two.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if allocation fails.
 */
static inline int clm_bpa_rehash_arrays(const size_t capacity)
{
    clm_bpaa_array_node** slots = calloc(capacity, sizeof(*slots));
    if (slots == NULL)
        return CLM_BPA_FAILURE;
    for (size_t index = 0; index < clm_bpa_global_arrays_capacity; ++index) {
        clm_bpaa_array_node* array = clm_bpa_global_arrays[index];
        if (array != NULL && array != CLM_BPA_ARRAY_TOMBSTONE) {
            size_t slot = clm_bpa_hash_id(array->id) & (capacity - 1);
            while (slots[slot] != NULL)
                slot = slot + 1 & capacity - 1;
            slots[slot] = array;
        }
    }
    free(clm_bpa_global_arrays);
    clm_bpa_global_arrays = slots;
    clm_bpa_global_arrays_capacity = capacity;
    clm_bpa_global_arrays_tombstones = 0;
    return CLM_BPA_SUCCESS;
}

/** Ensures the block registry has room and compacts tombstones if needed.
 * @return CLM_BPA_SUCCESS when insertion can proceed; otherwise failure.
 */
static inline int clm_bpa_prepare_blocks(void)
{
    const size_t capacity = clm_bpa_global_blocks_capacity;
    if (capacity == 0)
        return clm_bpa_rehash_blocks(CLM_BPA_INITIAL_CAPACITY);
    if (clm_bpa_global_blocks_count + clm_bpa_global_blocks_tombstones + 1
        >= capacity - capacity / 4) {
        if (clm_bpa_global_blocks_count < capacity / 4 &&
            clm_bpa_global_blocks_tombstones != 0)
            return clm_bpa_rehash_blocks(capacity);
        if (capacity > (size_t)-1 / 2)
            return CLM_BPA_FAILURE;
        return clm_bpa_rehash_blocks(capacity * 2);
    }
    return CLM_BPA_SUCCESS;
}

/** Ensures the array registry has room and compacts tombstones if needed.
 * @return CLM_BPA_SUCCESS when insertion can proceed; otherwise failure.
 */
static inline int clm_bpa_prepare_arrays(void)
{
    size_t capacity = clm_bpa_global_arrays_capacity;
    if (capacity == 0)
        return clm_bpa_rehash_arrays(CLM_BPA_INITIAL_CAPACITY);
    if (clm_bpa_global_arrays_count + clm_bpa_global_arrays_tombstones + 1 >= capacity - capacity / 4) {
        if (clm_bpa_global_arrays_count < capacity / 4 &&
            clm_bpa_global_arrays_tombstones != 0)
            return clm_bpa_rehash_arrays(capacity);
        if (capacity > (size_t)-1 / 2)
            return CLM_BPA_FAILURE;
        return clm_bpa_rehash_arrays(capacity * 2);
    }
    return CLM_BPA_SUCCESS;
}

/** Resolves the byte size of a built-in fixed-size BPA type.
 * @param type BPA type tag to resolve.
 * @return Type size in bytes, or zero for variable-sized/unsupported types.
 */
static inline clm_uint clm_bpa_type_size(const clm_byte type)
{
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

/** Looks up a block by ID in the global block registry.
 * @param id ID of the block to find.
 * @return The block node, or NULL when no such block exists.
 */
static inline clm_bpa_block_node* clm_bpa_find_block(const clm_id id)
{
    if (clm_bpa_global_blocks_capacity == 0)
        return NULL;
    size_t slot = clm_bpa_hash_id(id) & clm_bpa_global_blocks_capacity - 1;
    for (size_t probes = 0; probes < clm_bpa_global_blocks_capacity; ++probes) {
        clm_bpa_block_node* block = clm_bpa_global_blocks[slot];
        if (block == NULL)
            return NULL;
        if (block != CLM_BPA_BLOCK_TOMBSTONE && block->id == id)
            return block;
        slot = slot + 1 & clm_bpa_global_blocks_capacity - 1;
    }
    return NULL;
}

/** Checks whether a block with the given ID exists in the global registry.
 * @param id ID of the block to check.
 * @return true if the block exists; otherwise false.
 */
static inline bool clm_bpa_is_existing(const clm_id id)
{
    return clm_bpa_find_block(id) != NULL;
}

/** Looks up an array by ID in the global array registry.
 * @param id ID of the array to find.
 * @return The array node, or NULL when no such array exists.
 */
static inline clm_bpaa_array_node* clm_bpaa_find_array(const clm_id id)
{
    if (clm_bpa_global_arrays_capacity == 0)
        return NULL;
    size_t slot = clm_bpa_hash_id(id) & clm_bpa_global_arrays_capacity - 1;
    for (size_t probes = 0; probes < clm_bpa_global_arrays_capacity; ++probes) {
        clm_bpaa_array_node* array = clm_bpa_global_arrays[slot];
        if (array == NULL)
            return NULL;
        if (array != CLM_BPA_ARRAY_TOMBSTONE && array->id == id)
            return array;
        slot = slot + 1 & clm_bpa_global_arrays_capacity - 1;
    }
    return NULL;
}

/** Checks whether an array with the given ID exists in the global registry.
 * @param id ID of the array to check.
 * @return true if the array exists; otherwise false.
 */
static inline bool clm_bpaa_is_existing(const clm_id id)
{
    return clm_bpaa_find_array(id) != NULL;
}

/** Inserts a new block node into a prepared registry slot.
 * @param block Initialized node with an ID not already present in the table.
 */
static inline void clm_bpa_insert_block(clm_bpa_block_node* block)
{
    size_t slot = clm_bpa_hash_id(block->id) & clm_bpa_global_blocks_capacity - 1;
    size_t first_tombstone = (size_t)-1;
    while (clm_bpa_global_blocks[slot] != NULL) {
        if (clm_bpa_global_blocks[slot] == CLM_BPA_BLOCK_TOMBSTONE) {
            if (first_tombstone == (size_t)-1)
                first_tombstone = slot;
        }
        slot = slot + 1 & clm_bpa_global_blocks_capacity - 1;
    }
    if (first_tombstone != (size_t)-1) {
        slot = first_tombstone;
        --clm_bpa_global_blocks_tombstones;
    }
    clm_bpa_global_blocks[slot] = block;
    ++clm_bpa_global_blocks_count;
}

/** Inserts a new array node into a prepared registry slot.
 * @param array Initialized node with an ID not already present in the table.
 */
static inline void clm_bpa_insert_array(clm_bpaa_array_node* array)
{
    size_t slot = clm_bpa_hash_id(array->id) & clm_bpa_global_arrays_capacity - 1;
    size_t first_tombstone = (size_t)-1;
    while (clm_bpa_global_arrays[slot] != NULL) {
        if (clm_bpa_global_arrays[slot] == CLM_BPA_ARRAY_TOMBSTONE) {
            if (first_tombstone == (size_t)-1)
                first_tombstone = slot;
        }
        slot = slot + 1 & clm_bpa_global_arrays_capacity - 1;
    }
    if (first_tombstone != (size_t)-1) {
        slot = first_tombstone;
        --clm_bpa_global_arrays_tombstones;
    }
    clm_bpa_global_arrays[slot] = array;
    ++clm_bpa_global_arrays_count;
}

/** Removes a block from the registry and returns its storage to the pool.
 * @param id ID of the block to release.
 * @return CLM_BPA_SUCCESS if removed; otherwise CLM_BPA_FAILURE.
 */
static inline int clm_bpa_remove_block(const clm_id id)
{
    if (clm_bpa_global_blocks_capacity == 0)
        return CLM_BPA_FAILURE;
    size_t slot = clm_bpa_hash_id(id) & clm_bpa_global_blocks_capacity - 1;
    for (size_t probes = 0; probes < clm_bpa_global_blocks_capacity; ++probes) {
        clm_bpa_block_node* block = clm_bpa_global_blocks[slot];
        if (block == NULL)
            return CLM_BPA_FAILURE;
        if (block != CLM_BPA_BLOCK_TOMBSTONE && block->id == id) {
            clm_bpa_global_blocks[slot] = CLM_BPA_BLOCK_TOMBSTONE;
            --clm_bpa_global_blocks_count;
            ++clm_bpa_global_blocks_tombstones;
            clm_bpa_pool_free(block->data, block->type == CLM_BPA_TYPE_STRING
                ? (size_t)block->length + 1 : (size_t)block->block_size);
            free(block);
            return CLM_BPA_SUCCESS;
        }
        slot = slot + 1 & clm_bpa_global_blocks_capacity - 1;
    }
    return CLM_BPA_FAILURE;
}

/** Removes an array and releases its items and backing storage.
 * @param id ID of the array to release.
 * @return CLM_BPA_SUCCESS if removed; otherwise CLM_BPA_FAILURE.
 */
static inline int clm_bpa_remove_array(const clm_id id)
{
    if (clm_bpa_global_arrays_capacity == 0)
        return CLM_BPA_FAILURE;
    size_t slot = clm_bpa_hash_id(id) & (clm_bpa_global_arrays_capacity - 1);
    for (size_t probes = 0; probes < clm_bpa_global_arrays_capacity; ++probes) {
        clm_bpaa_array_node* array = clm_bpa_global_arrays[slot];
        if (array == NULL)
            return CLM_BPA_FAILURE;
        if (array != CLM_BPA_ARRAY_TOMBSTONE && array->id == id) {
            clm_bpa_global_arrays[slot] = CLM_BPA_ARRAY_TOMBSTONE;
            --clm_bpa_global_arrays_count;
            ++clm_bpa_global_arrays_tombstones;
            if (array->type == CLM_BPA_TYPE_STRING) {
                for (clm_uint index = 0; index < array->count; ++index)
                    clm_bpa_pool_free(array->items.strings[index], strlen(array->items.strings[index]) + 1);
                free(array->items.strings);
            } else {
                free(array->items.blocks);
            }
            free(array);
            return CLM_BPA_SUCCESS;
        }
        slot = slot + 1 & clm_bpa_global_arrays_capacity - 1;
    }
    return CLM_BPA_FAILURE;
}

/** Copies a bounded string into pool storage and records its byte length.
 * @param buffer Source bytes; a NUL within buffer_size ends the string.
 * @param buffer_size Number of readable source bytes.
 * @param value Receives the allocated, NUL-terminated copy.
 * @param length Receives the copied length excluding the terminator.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
static inline int clm_bpa_copy_string(const void* buffer, const clm_uint buffer_size, char** value, clm_uint* length)
{
    const char* bytes = buffer;

    if (buffer == NULL || buffer_size == 0 || value == NULL || length == NULL)
        return CLM_BPA_FAILURE;

    const char* terminator = memchr(bytes, '\0', buffer_size);
    const size_t string_length = terminator == NULL ? (size_t)buffer_size : (size_t)(terminator - bytes);
    if ((size_t)(clm_uint)string_length != string_length || string_length == (size_t)-1)
        return CLM_BPA_FAILURE;

    char* copy = clm_bpa_pool_alloc(string_length + 1);
    if (copy == NULL)
        return CLM_BPA_FAILURE;
    memcpy(copy, bytes, string_length);
    copy[string_length] = '\0';

    *value = copy;
    *length = (clm_uint)string_length;
    return CLM_BPA_SUCCESS;
}

/** Creates a zero-initialized block with an explicit size for custom structs.
 * @param id Unique global ID; type selects a built-in, string, or struct block.
 * @param type Block type; use CLM_BPA_TYPE_STRUCT for custom-sized blocks.
 * @param block_size Required byte size for STRUCT; pass zero for built-in types.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/duplicate ID.
 */
static inline int clm_bpa_create_sized(const clm_id id, const clm_byte type, const clm_uint block_size)
{
    clm_uint resolved_size = clm_bpa_type_size(type);

    if (clm_bpa_find_block(id) != NULL || clm_bpa_prepare_blocks() != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

    if (type == CLM_BPA_TYPE_STRING) {
        if (block_size != 0)
            return CLM_BPA_FAILURE;
    } else if (type == CLM_BPA_TYPE_STRUCT) {
        if (block_size == 0)
            return CLM_BPA_FAILURE;
        resolved_size = block_size;
    } else if (resolved_size == 0 || (block_size != 0 && block_size != resolved_size)) {
        return CLM_BPA_FAILURE;
    }

    clm_bpa_block_node* block = calloc(1, sizeof(*block));
    if (block == NULL)
        return CLM_BPA_FAILURE;
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

/** Creates a zero-initialized block of a built-in type.
 * @param id Unique global ID to assign to the block.
 * @param type Supported fixed-size or string BPA type; STRUCT requires create_sized.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if creation is not possible.
 */
static inline int clm_bpa_create(const clm_id id, const clm_byte type)
{
    if (type == CLM_BPA_TYPE_STRUCT)
        return CLM_BPA_FAILURE;
    return clm_bpa_create_sized(id, type, 0);
}

/** Immediately releases a block regardless of its reference count.
 * @param id ID of the block to delete.
 * @return CLM_BPA_SUCCESS if deleted; otherwise CLM_BPA_FAILURE.
 */
static inline int clm_bpa_delete(const clm_id id)
{
    return clm_bpa_remove_block(id);
}

/** Adds one reference to an existing block.
 * @param id ID of the block to retain.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing/at the refcount limit.
 */
static inline int clm_bpa_ref(const clm_id id)
{
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->refcount == USHRT_MAX)
        return CLM_BPA_FAILURE;
    ++block->refcount;
    return CLM_BPA_SUCCESS;
}

/** Releases one reference and deletes the block when its count reaches zero.
 * @param id ID of the block to release.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing or already unreferenced.
 */
static inline int clm_bpa_unref(const clm_id id)
{
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->refcount == 0)
        return CLM_BPA_FAILURE;
    if (--block->refcount != 0)
        return CLM_BPA_SUCCESS;
    return clm_bpa_remove_block(id);
}

/** Copies a fixed-size block into a caller-provided output buffer.
 * @param id ID of a non-string block.
 * @param buffer Receives the copied block contents.
 * @param buffer_size Destination capacity in bytes; must cover the block size.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
 */
static inline int clm_bpa_get(const clm_id id, void* buffer, const clm_uint buffer_size)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type == CLM_BPA_TYPE_STRING || buffer == NULL ||
        buffer_size < block->block_size)
        return CLM_BPA_FAILURE;
    memcpy(buffer, block->data, block->block_size);
    return CLM_BPA_GET_SUCCESS;
}

/** Replaces the contents of a fixed-size block with caller-provided bytes.
 * @param id ID of a non-string block.
 * @param buffer Source bytes to copy into the block.
 * @param buffer_size Source size in bytes; must exactly match the block size.
 * @return CLM_BPA_SET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
 */
static inline int clm_bpa_set(const clm_id id, const void* buffer, const clm_uint buffer_size)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type == CLM_BPA_TYPE_STRING || buffer == NULL ||
        buffer_size != block->block_size)
        return CLM_BPA_FAILURE;
    memmove(block->data, buffer, block->block_size);
    return CLM_BPA_SET_SUCCESS;
}

/** Copies a string block, including its NUL terminator, into the output buffer.
 * @param id ID of a string block.
 * @param buffer Receives the string and its NUL terminator.
 * @param buffer_size Destination capacity in bytes, including room for the terminator.
 * @return String byte length excluding NUL, or CLM_BPA_FAILURE if it does not fit.
 */
static inline int clm_bpa_get_string(const clm_id id, void* buffer, const clm_uint buffer_size)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || buffer == NULL)
        return CLM_BPA_FAILURE;
    const size_t required = (size_t)block->length + 1;
    if ((size_t)buffer_size < required || block->length > INT_MAX)
        return CLM_BPA_FAILURE;
    if (block->length != 0)
        memcpy(buffer, block->data, block->length);
    ((char*)buffer)[block->length] = '\0';
    return (int)block->length;
}

/** Replaces a string block with a bounded copy of the supplied string bytes.
 * @param id ID of a string block.
 * @param buffer Source string bytes; a NUL within the bound terminates the string.
 * @param buffer_size Number of readable source bytes.
 * @return String byte length excluding NUL, or CLM_BPA_FAILURE on error.
 */
static inline int clm_bpa_set_string(const clm_id id, const void* buffer, const clm_uint buffer_size)
{
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    char* copy;
    clm_uint length;
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING ||
        clm_bpa_copy_string(buffer, buffer_size, &copy, &length) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (length > INT_MAX) {
        clm_bpa_pool_free(copy, (size_t)length + 1);
        return CLM_BPA_FAILURE;
    }
    clm_bpa_pool_free(block->data, (size_t)block->length + 1);
    block->data = copy;
    block->length = length;
    return (int)length;
}

/** Replaces the content of an existing block.
 * @param id ID of the block to update.
 * @param buffer Source content; string data is copied into the internal pool.
 * @param buffer_size Number of readable source bytes.
 * @return CLM_BPA_SET_SUCCESS for fixed-size blocks, string byte length for
 *         string blocks, or CLM_BPA_FAILURE on invalid input.
 */
static inline int clm_bpa_replace(
    const clm_id id,
    const void* buffer,
    const clm_uint buffer_size)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL)
        return CLM_BPA_FAILURE;
    return block->type == CLM_BPA_TYPE_STRING
        ? clm_bpa_set_string(id, buffer, buffer_size)
        : clm_bpa_set(id, buffer, buffer_size);
}

/** Creates an array and reserves initial slots, with an explicit struct item size.
 * @param id Unique global array ID; reserved is the initial item capacity.
 * @param type Item type; block_size is required only for STRUCT items.
 * @param reserved Initial number of reserved item slots; must be nonnegative.
 * @param block_size Item byte width for STRUCT; pass zero for built-in/string types.
 * @param encoding Encoding metadata retained for string arrays.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/duplicate ID.
 */
static inline int clm_bpaa_create_sized(const clm_id id, const clm_byte type, const clm_int reserved, const clm_uint block_size, const clm_byte encoding)
{
    clm_uint resolved_size = clm_bpa_type_size(type);
    if (clm_bpaa_find_array(id) != NULL || clm_bpa_prepare_arrays() != CLM_BPA_SUCCESS ||
        reserved < 0)
        return CLM_BPA_FAILURE;

    if (type == CLM_BPA_TYPE_STRING) {
        if (block_size != 0)
            return CLM_BPA_FAILURE;
    } else if (type == CLM_BPA_TYPE_STRUCT) {
        if (block_size == 0)
            return CLM_BPA_FAILURE;
        resolved_size = block_size;
    } else if (resolved_size == 0 || (block_size != 0 && block_size != resolved_size)) {
        return CLM_BPA_FAILURE;
    }

    clm_bpaa_array_node* array = calloc(1, sizeof(*array));
    if (array == NULL)
        return CLM_BPA_FAILURE;
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
        } else {
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

/** Creates an array of a built-in type with the requested reserved capacity.
 * @param id Unique global array ID; reserved is the initial slot capacity.
 * @param type Supported built-in or string type; STRUCT requires create_sized.
 * @param reserved Initial number of reserved item slots; must be nonnegative.
 * @param encoding Encoding metadata for string arrays.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE when creation cannot proceed.
 */
static inline int clm_bpaa_create(const clm_id id, const clm_byte type, const clm_int reserved, const clm_byte encoding)
{
    if (type == CLM_BPA_TYPE_STRUCT)
        return CLM_BPA_FAILURE;
    return clm_bpaa_create_sized(id, type, reserved, 0, encoding);
}

/** Immediately deletes an array and releases its items and reserved storage.
 * @param id ID of the array to delete.
 * @return CLM_BPA_SUCCESS if deleted; otherwise CLM_BPA_FAILURE.
 */
static inline int clm_bpaa_delete(const clm_id id)
{
    return clm_bpa_remove_array(id);
}

/** Adds one reference to an existing array.
 * @param id ID of the array to retain.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing/at the refcount limit.
 */
static inline int clm_bpaa_ref(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->refcount == USHRT_MAX)
        return CLM_BPA_FAILURE;
    ++array->refcount;
    return CLM_BPA_SUCCESS;
}

/** Releases one array reference and deletes the array when its count reaches zero.
 * @param id ID of the array to release.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing/unreferenced.
 */
static inline int clm_bpaa_unref(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->refcount == 0)
        return CLM_BPA_FAILURE;
    if (--array->refcount != 0)
        return CLM_BPA_SUCCESS;
    return clm_bpaa_delete(id);
}


/* Copy-based slicing. The destination must be a previously unused array ID.
 * The source remains unchanged, and the destination owns its copies. */
/** Creates a new BPAA containing [start, start + length) from source.
 * Fixed-width elements are bulk-copied; string elements are deep-copied.
 * The destination keeps the source type, item width and encoding metadata.
 * @param source_id ID of the array to slice.
 * @param start Zero-based first element to include.
 * @param length Number of elements to copy.
 * @param destination_id Unused ID for the new independent array.
 * @return CLM_BPA_SUCCESS or CLM_BPA_FAILURE; no destination remains on failure.
 */
static inline int clm_bpaa_slice(const clm_id source_id, const clm_int start,
                                 const clm_int length, const clm_id destination_id)
{
    const clm_bpaa_array_node* source = clm_bpaa_find_array(source_id);
    if (source == NULL || start < 0 || length < 0 ||
        (clm_uint)start > source->count ||
        (clm_uint)length > source->count - (clm_uint)start ||
        clm_bpaa_is_existing(destination_id))
        return CLM_BPA_FAILURE;

    if (clm_bpaa_create_sized(destination_id, source->type, length,
                              source->type == CLM_BPA_TYPE_STRUCT ? source->block_size : 0,
                              source->encoding) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;

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
    } else if (length != 0) {
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

/** Borrows a contiguous fixed-width slice without copying or retaining the array.
 * The returned pointer is invalidated by array deletion/shutdown or any capacity
 * change. Keep the source array alive and do not mutate it while using the view.
 * @param source_id ID of a non-string fixed-width array.
 * @param start Zero-based first element to include.
 * @param length Number of elements to borrow.
 * @param buffer Receives the first sliced byte; NULL for an empty slice.
 * @param buffer_size Receives the byte size of the slice.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/range.
 */
static inline int clm_bpaa_slice_borrow(const clm_id source_id,
                                        const clm_int start,
                                        const clm_int length,
                                        const void** buffer,
                                        size_t* buffer_size)
{
    const clm_bpaa_array_node* source = clm_bpaa_find_array(source_id);
    if (buffer == NULL || buffer_size == NULL)
        return CLM_BPA_FAILURE;
    *buffer = NULL;
    *buffer_size = 0;
    if (source == NULL || source->type == CLM_BPA_TYPE_STRING ||
        start < 0 || length < 0 || (clm_uint)start > source->count ||
        (clm_uint)length > source->count - (clm_uint)start ||
        source->block_size == 0 ||
        (length != 0 && (size_t)length > ((size_t)-1) / source->block_size))
        return CLM_BPA_FAILURE;
    if (length != 0) {
        *buffer = source->items.blocks + (size_t)start * source->block_size;
        *buffer_size = (size_t)length * source->block_size;
    }
    return CLM_BPA_SUCCESS;
}

/** Copies a fixed-width BPAA slice to caller storage (no NUL/string pointers).
 * buffer_size is in BYTES; start/length are in ELEMENTS. A zero-length slice
 * succeeds without touching buffer. Caller destination may overlap source.
 * @param source_id ID of a non-string fixed-width array.
 * @param start Zero-based first element to copy.
 * @param length Number of elements to copy.
 * @param buffer Destination for the copied bytes; may be NULL for an empty slice.
 * @param buffer_size Destination capacity in bytes.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/range/capacity.
 */
static inline int clm_bpaa_slice_to(const clm_id source_id, const clm_int start,
                                    const clm_int length, void* buffer,
                                    const size_t buffer_size)
{
    const clm_bpaa_array_node* source = clm_bpaa_find_array(source_id);
    if (source == NULL || source->type == CLM_BPA_TYPE_STRING ||
        start < 0 || length < 0 || (clm_uint)start > source->count ||
        (clm_uint)length > source->count - (clm_uint)start)
        return CLM_BPA_FAILURE;
    if (length == 0) return CLM_BPA_SUCCESS;
    if (source->block_size == 0 ||
        (size_t)length > (size_t)-1 / source->block_size)
        return CLM_BPA_FAILURE;
    const size_t bytes = (size_t)length * source->block_size;
    if (buffer == NULL || buffer_size < bytes) return CLM_BPA_FAILURE;
    memmove(buffer, source->items.blocks + (size_t)start * source->block_size, bytes);
    return CLM_BPA_SUCCESS;
}

/** Copies a fixed-size array item into a caller-provided buffer.
 * @param id Array ID of a non-string fixed-width array.
 * @param index Zero-based item position to copy.
 * @param buffer Receives the item bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
 */
static inline int clm_bpaa_get_raw(const clm_id id, const clm_int index, void* buffer, const clm_uint buffer_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        buffer_size < array->block_size)
        return CLM_BPA_FAILURE;
    memcpy(buffer, array->items.blocks + (size_t)index * array->block_size, array->block_size);
    return CLM_BPA_GET_SUCCESS;
}

/** Copies one string item, including its NUL terminator, to the output buffer.
 * @param id Array ID of a string array.
 * @param index Zero-based string position to copy.
 * @param buffer Receives the string and its NUL terminator.
 * @param buffer_size Destination capacity in bytes, including room for NUL.
 * @return String byte length excluding NUL, or CLM_BPA_FAILURE if it does not fit.
 */
static inline int clm_bpaa_get_string_raw(const clm_id id, const clm_int index, void* buffer, const clm_uint buffer_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type != CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL)
        return CLM_BPA_FAILURE;
    const clm_uint length = (clm_uint)strlen(array->items.strings[index]);
    const size_t required = (size_t)length + 1;
    if ((size_t)buffer_size < required || length > INT_MAX)
        return CLM_BPA_FAILURE;
    memcpy(buffer, array->items.strings[index], required);
    return (int)length;
}

/** Replaces a string array item with a bounded copy of caller-provided bytes.
 * @param id Array ID of a string array.
 * @param index Zero-based position of the existing string to replace.
 * @param buffer Source string bytes; a NUL within the bound terminates the string.
 * @param buffer_size Number of readable source bytes.
 * @return String byte length excluding NUL, or CLM_BPA_FAILURE on error.
 */
static inline int clm_bpaa_set_string_raw(const clm_id id, const clm_int index, const void* buffer, const clm_uint buffer_size)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    char* copy;
    clm_uint length;
    if (array == NULL || array->type != CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        clm_bpa_copy_string(buffer, buffer_size, &copy, &length) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (length > INT_MAX) {
        clm_bpa_pool_free(copy, (size_t)length + 1);
        return CLM_BPA_FAILURE;
    }
    clm_bpa_pool_free(array->items.strings[index], strlen(array->items.strings[index]) + 1);
    array->items.strings[index] = copy;
    array->deck_size = array->count;
    return (int)length;
}

/** Copies the first fixed-size array item to the caller's buffer.
 * @param id Array ID of a non-string fixed-width array.
 * @param buffer Receives the first item's bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE for empty/invalid input.
 */
static inline int clm_bpaa_get_first_raw(const clm_id id, void* buffer, const clm_uint buffer_size)
{
    return clm_bpaa_get_raw(id, 0, buffer, buffer_size);
}

/** Copies the last fixed-size array item to the caller's buffer.
 * @param id Array ID of a non-string fixed-width array.
 * @param buffer Receives the last item's bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE for empty/invalid input.
 */
static inline int clm_bpaa_get_last_raw(const clm_id id, void* buffer, const clm_uint buffer_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->count == 0 || array->count - 1 > INT_MAX)
        return CLM_BPA_FAILURE;
    return clm_bpaa_get_raw(id, (clm_int)(array->count - 1), buffer, buffer_size);
}

/** Retrieves the number of initialized items in an array.
 * @param id Array ID to query.
 * @param count Receives the number of initialized items.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for a missing array/null output.
 */
static inline int clm_bpaa_get_count_to(const clm_id id, clm_uint* count)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || count == NULL)
        return CLM_BPA_FAILURE;
    *count = array->count;
    return CLM_BPA_GET_SUCCESS;
}

/** Retrieves the current item's string length, excluding its NUL terminator.
 * @param id String-array ID to query.
 * @param length Receives the current string's byte length, excluding NUL.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid/current-empty state.
 */
static inline int clm_bpaa_get_length_to(const clm_id id, clm_uint* length)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || length == NULL || array->type != CLM_BPA_TYPE_STRING ||
        array->index >= array->count)
        return CLM_BPA_FAILURE;
    *length = (clm_uint)strlen(array->items.strings[array->index]);
    return CLM_BPA_GET_SUCCESS;
}

/** Retrieves the number of item slots currently reserved by an array.
 * @param id Array ID to query.
 * @param capacity Receives the number of reserved item slots.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for a missing array/null output.
 */
static inline int clm_bpaa_get_capacity_to(const clm_id id, clm_uint* capacity)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || capacity == NULL)
        return CLM_BPA_FAILURE;
    *capacity = array->capacity;
    return CLM_BPA_GET_SUCCESS;
}

/** Resizes an array's backing storage while preserving its existing items.
 * @param array Internal array node to resize; its capacity is updated on success.
 * @param capacity New slot count, which must be at least the live item count.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if size arithmetic/allocation fails.
 */
static inline int clm_bpaa_resize(clm_bpaa_array_node* array, const clm_uint capacity)
{
    void* resized;
    if (array->type == CLM_BPA_TYPE_STRING) {
        if (capacity == 0) {
            free(array->items.strings);
            array->items.strings = NULL;
        } else {
            if ((size_t)capacity > (size_t)-1 / sizeof(char*))
                return CLM_BPA_FAILURE;
            resized = realloc(array->items.strings, (size_t)capacity * sizeof(char*));
            if (resized == NULL)
                return CLM_BPA_FAILURE;
            array->items.strings = (char**)resized;
            if (capacity > array->capacity)
                memset(array->items.strings + array->capacity, 0,
                       (size_t)(capacity - array->capacity) * sizeof(char*));
        }
    } else {
        if (capacity == 0) {
            free(array->items.blocks);
            array->items.blocks = NULL;
        } else {
            if ((size_t)capacity > (size_t)-1 / (size_t)array->block_size)
                return CLM_BPA_FAILURE;
            resized = realloc(array->items.blocks,
                              (size_t)capacity * array->block_size);
            if (resized == NULL)
                return CLM_BPA_FAILURE;
            array->items.blocks = (unsigned char*)resized;
        }
    }
    array->capacity = capacity;
    return CLM_BPA_SUCCESS;
}

/** Grows storage geometrically when needed, unless the array is sealed.
 * @param array Internal array node to grow.
 * @param needed Minimum slot count required by the pending insertion.
 * @return CLM_BPA_SUCCESS if capacity is sufficient; otherwise failure.
 */
static inline int clm_bpaa_ensure_capacity(clm_bpaa_array_node* array, const clm_uint needed)
{
    if (needed <= array->capacity)
        return CLM_BPA_SUCCESS;
    if (array->sealed)
        return CLM_BPA_FAILURE;
    clm_uint capacity = array->capacity == 0 ? 4 : array->capacity;
    while (capacity < needed) {
        if (capacity > ((clm_uint)-1 >> 1)) {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }
    return clm_bpaa_resize(array, capacity);
}

/** Adds an explicit number of reserved slots to an unsealed array.
 * @param id Array ID to extend.
 * @param additional_capacity Number of additional slots to reserve; must be positive.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for sealed/invalid/overflow cases.
 */
static inline int clm_bpaa_extend(const clm_id id, const clm_uint additional_capacity)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->sealed || additional_capacity == 0 ||
        additional_capacity > (clm_uint)-1 - array->capacity)
        return CLM_BPA_FAILURE;
    return clm_bpaa_resize(array, array->capacity + additional_capacity);
}

/** Reduces reserved slots without discarding live items on an unsealed array.
 * @param id Array ID to shrink.
 * @param new_capacity New slot capacity; must be below current capacity and at least count.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE when shrinking is not permitted.
 */
static inline int clm_bpaa_shrink(const clm_id id, const clm_uint new_capacity)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->sealed || new_capacity >= array->capacity ||
        new_capacity < array->count)
        return CLM_BPA_FAILURE;
    return clm_bpaa_resize(array, new_capacity);
}

/** Copies the item at the array's current cursor into the caller's buffer.
 * @param id Array ID of a non-string fixed-width array.
 * @param buffer Receives the current item's bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE for invalid state.
 */
static inline int clm_bpaa_get_current_raw(const clm_id id, void* buffer, const clm_uint buffer_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->index >= array->count || array->index > INT_MAX)
        return CLM_BPA_FAILURE;
    return clm_bpaa_get_raw(id, (clm_int)array->index, buffer, buffer_size);
}

/** Replaces a fixed-size item without changing array count or capacity.
 * @param id Array ID of a non-string fixed-width array.
 * @param index Zero-based position of the existing item to replace.
 * @param buffer Source bytes to copy into the item.
 * @param buffer_size Source size in bytes; must exactly match the item size.
 * @return CLM_BPA_SET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
 */
static inline int clm_bpaa_set_raw(const clm_id id, const clm_int index, const void* buffer, const clm_uint buffer_size)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        buffer_size != array->block_size)
        return CLM_BPA_FAILURE;
    memmove(array->items.blocks + (size_t)index * array->block_size, buffer,
            array->block_size);
    array->deck_size = array->count;
    return CLM_BPA_SET_SUCCESS;
}

/** Inserts one item at an index, growing storage only if the reserve is full.
 * @param id Array ID to modify.
 * @param index Zero-based insertion position, from zero through the current count.
 * @param buffer Fixed-width item bytes or source string bytes.
 * @param buffer_size Item byte size, or readable source bound for a string.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
static inline int clm_bpaa_insert_raw(const clm_id id, const clm_int index, const void* buffer, const clm_uint buffer_size)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    char* string_copy = NULL;
    clm_uint string_length = 0;
    if (array == NULL || index < 0 ||
        (clm_uint)index > array->count || array->count == (clm_uint)-1)
        return CLM_BPA_FAILURE;

    if (array->type == CLM_BPA_TYPE_STRING) {
        if (clm_bpa_copy_string(buffer, buffer_size, &string_copy, &string_length) !=
            CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
    } else if (buffer == NULL || buffer_size != array->block_size) {
        return CLM_BPA_FAILURE;
    }

    /* Snapshot the source before realloc/memmove: it may alias this array. */
    void* value_copy = NULL;
    if (array->type != CLM_BPA_TYPE_STRING) {
        value_copy = malloc(array->block_size);
        if (value_copy == NULL) return CLM_BPA_FAILURE;
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
    } else {
        unsigned char* destination =
            array->items.blocks + (size_t)index * array->block_size;
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

/** Appends one fixed-size item or bounded string to an array.
 * @param id Array ID to modify.
 * @param buffer Fixed-width item bytes or source string bytes.
 * @param buffer_size Item byte size, or readable source bound for a string.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/growth failure.
 */
static inline int clm_bpaa_append_raw(const clm_id id, const void* buffer, const clm_uint buffer_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->count > INT_MAX)
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_raw(id, (clm_int)array->count, buffer, buffer_size);
}

/** Removes an item, compacts following items, and retains reserved capacity.
 * @param id Array ID to modify.
 * @param index Zero-based position of the initialized item to remove.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array/index is invalid.
 */
static inline int clm_bpaa_remove(const clm_id id, const clm_int index)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || index < 0 ||
        (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    if (array->type == CLM_BPA_TYPE_STRING) {
        clm_bpa_pool_free(array->items.strings[index], strlen(array->items.strings[index]) + 1);
        memmove(array->items.strings + index, array->items.strings + index + 1,
                (size_t)(array->count - (clm_uint)index - 1) * sizeof(char*));
        array->items.strings[array->count - 1] = NULL;
    } else {
        unsigned char* item = array->items.blocks + (size_t)index * array->block_size;
        memmove(item, item + array->block_size,
                (size_t)(array->count - (clm_uint)index - 1) * array->block_size);
    }
    --array->count;
    if (array->count == 0)
        array->index = 0;
    else if (array->index > (clm_uint)index)
        --array->index;
    else if (array->index >= array->count)
        array->index = array->count - 1;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

/** Removes the first item while retaining the array's reserve.
 * @param id Array ID; the array must contain at least one item.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if empty or missing.
 */
static inline int clm_bpaa_remove_first(const clm_id id)
{
    return clm_bpaa_remove(id, 0);
}

/** Removes the last item while retaining the array's reserve.
 * @param id Array ID; the array must contain at least one item.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if empty or missing.
 */
static inline int clm_bpaa_remove_last(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->count == 0 || array->count - 1 > INT_MAX)
        return CLM_BPA_FAILURE;
    return clm_bpaa_remove(id, (clm_int)(array->count - 1));
}

/** Removes all items and resets the cursor/deck without releasing capacity.
 * @param id Array ID to clear.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array does not exist.
 */
static inline int clm_bpaa_clear(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL)
        return CLM_BPA_FAILURE;
    if (array->type == CLM_BPA_TYPE_STRING)
    {
        for (clm_uint index = 0; index < array->count; ++index) {
            clm_bpa_pool_free(array->items.strings[index], strlen(array->items.strings[index]) + 1);
            array->items.strings[index] = NULL;
        }
    } else if (array->capacity != 0) {
        memset(array->items.blocks, 0, (size_t)array->capacity * array->block_size);
    }
    array->count = 0;
    array->index = 0;
    array->deck_size = 0;
    return CLM_BPA_SUCCESS;
}

/* Advanced borrowed-array access. The caller must follow the lifetime contract:
 * explicit delete/shutdown invalidates the borrowed pointer immediately. */
/** Borrows array metadata and retains one reference against ordinary unref.
 * @param id Array ID to borrow.
 * @param out_array Receives the internal metadata pointer; initialized to NULL.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/missing array.
 * @note This does not prevent explicit delete or shutdown from invalidating it.
 */
static inline int clm_bpaa_advanced_borrow(
    const clm_id id,
    clm_bpaa_array_node** out_array)
{
    if (out_array == NULL)
        return CLM_BPA_FAILURE;
    *out_array = NULL;
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->refcount == USHRT_MAX)
        return CLM_BPA_FAILURE;
    ++array->refcount;
    *out_array = array;
    return CLM_BPA_SUCCESS;
}

/** Releases an advanced borrow and nulls its pointer.
 * @param borrowed Address of a live borrowed pointer.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid/released input.
 * @note Do not call after explicit deletion or shutdown invalidated the pointer.
 */
static inline int clm_bpaa_advanced_release(clm_bpaa_array_node** borrowed)
{
    if (borrowed == NULL || *borrowed == NULL)
        return CLM_BPA_FAILURE;
    clm_bpaa_array_node* array = *borrowed;
    if (array->refcount == 0)
        return CLM_BPA_FAILURE;
    clm_id id = array->id;
    *borrowed = NULL;
    if (--array->refcount != 0)
        return CLM_BPA_SUCCESS;
    return clm_bpa_remove_array(id);
}

/** Returns the contiguous fixed-width payload, or NULL for string arrays.
 * @param array A live pointer returned by clm_bpaa_advanced_borrow.
 * @return Mutable payload address; invalidated by resize, delete, or shutdown.
 */
static inline void* clm_bpaa_advanced_data(const clm_bpaa_array_node* array)
{
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING)
        return NULL;
    return array->items.blocks;
}

/** Retrieves a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for a non-string array.
 * @param index Zero-based position of the item to retrieve.
 * @param buffer Receives the item bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 */
static inline int clm_bpaa_advanced_get_raw(
    const clm_bpaa_array_node* array,
    const clm_int index,
    void* buffer,
    const clm_uint buffer_size)
{
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        buffer_size < array->block_size)
        return CLM_BPA_FAILURE;
    memcpy(buffer, array->items.blocks + (size_t)index * array->block_size,
           array->block_size);
    return CLM_BPA_GET_SUCCESS;
}

/** Replaces a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for a non-string array.
 * @param index Zero-based position of the existing item to replace.
 * @param buffer Source bytes to copy into the item.
 * @param buffer_size Source size in bytes; must exactly match the item size.
 */
static inline int clm_bpaa_advanced_set_raw(
    clm_bpaa_array_node* array,
    const clm_int index,
    const void* buffer,
    const clm_uint buffer_size)
{
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count || buffer == NULL ||
        buffer_size != array->block_size)
        return CLM_BPA_FAILURE;
    memmove(array->items.blocks + (size_t)index * array->block_size, buffer,
            array->block_size);
    array->deck_size = array->count;
    return CLM_BPA_SET_SUCCESS;
}

/** Inserts a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for an unsealed non-string array.
 * @param index Zero-based insertion position, from zero through the current count.
 * @param buffer Source bytes for the new item.
 * @param buffer_size Source size in bytes; must exactly match the item size.
 */
static inline int clm_bpaa_advanced_insert_raw(
    clm_bpaa_array_node* array,
    const clm_int index,
    const void* buffer,
    const clm_uint buffer_size)
{
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index > array->count || array->count >= INT_MAX ||
        buffer == NULL || buffer_size != array->block_size)
        return CLM_BPA_FAILURE;
    void* value_copy = malloc(array->block_size);
    if (value_copy == NULL)
        return CLM_BPA_FAILURE;
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

/** Appends a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for an unsealed non-string array.
 * @param buffer Source bytes for the new item.
 * @param buffer_size Source size in bytes; must exactly match the item size.
 */
static inline int clm_bpaa_advanced_append_raw(
    clm_bpaa_array_node* array,
    const void* buffer,
    const clm_uint buffer_size)
{
    if (array == NULL || array->count > INT_MAX)
        return CLM_BPA_FAILURE;
    return clm_bpaa_advanced_insert_raw(
        array, (clm_int)array->count, buffer, buffer_size);
}

/** Removes a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for a non-string array.
 * @param index Zero-based position of the item to remove.
 */
static inline int clm_bpaa_advanced_remove(
    clm_bpaa_array_node* array,
    const clm_int index)
{
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    unsigned char* item = array->items.blocks + (size_t)index * array->block_size;
    memmove(item, item + array->block_size,
            (size_t)(array->count - (clm_uint)index - 1) * array->block_size);
    --array->count;
    if (array->count == 0)
        array->index = 0;
    else if (array->index > (clm_uint)index)
        --array->index;
    else if (array->index >= array->count)
        array->index = array->count - 1;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

/** Clears a fixed-width array without releasing its reserved capacity.
 * @param array Live borrowed metadata for a non-string array.
 */
static inline int clm_bpaa_advanced_clear(clm_bpaa_array_node* array)
{
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_FAILURE;
    if (array->capacity != 0)
        memset(array->items.blocks, 0,
               (size_t)array->capacity * array->block_size);
    array->count = 0;
    array->index = 0;
    array->deck_size = 0;
    return CLM_BPA_SUCCESS;
}

/** Reserves additional slots without performing an ID lookup.
 * @param array Live borrowed metadata for an unsealed array.
 * @param additional_capacity Number of additional slots to reserve; must be positive.
 */
static inline int clm_bpaa_advanced_extend(
    clm_bpaa_array_node* array,
    const clm_uint additional_capacity)
{
    if (array == NULL || array->sealed || additional_capacity == 0 ||
        additional_capacity > (clm_uint)-1 - array->capacity)
        return CLM_BPA_FAILURE;
    return clm_bpaa_resize(array, array->capacity + additional_capacity);
}

/** Shrinks reserved slots without performing an ID lookup.
 * @param array Live borrowed metadata for an unsealed array.
 * @param new_capacity New capacity; must be below current capacity and at least count.
 */
static inline int clm_bpaa_advanced_shrink(
    clm_bpaa_array_node* array,
    const clm_uint new_capacity)
{
    if (array == NULL || array->sealed || new_capacity >= array->capacity ||
        new_capacity < array->count)
        return CLM_BPA_FAILURE;
    return clm_bpaa_resize(array, new_capacity);
}

/** Moves the array cursor to index zero.
 * @param id Array ID whose cursor should be reset.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
 */
static inline int clm_bpaa_reset(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL)
        return CLM_BPA_FAILURE;
    array->index = 0;
    return CLM_BPA_SUCCESS;
}

/** Moves the array cursor to its first item.
 * @param id Array ID whose cursor should move.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
 */
static inline int clm_bpaa_move_first(const clm_id id)
{
    return clm_bpaa_reset(id);
}

/** Moves the array cursor one item toward the beginning.
 * @param id Array ID; the cursor must currently be greater than zero.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE at the beginning/missing array.
 */
static inline int clm_bpaa_move_previous(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->index == 0)
        return CLM_BPA_FAILURE;
    --array->index;
    return CLM_BPA_SUCCESS;
}

/** Moves the array cursor one item toward the end.
 * @param id Array ID; a next item must exist.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE at the end/missing array.
 */
static inline int clm_bpaa_move_next(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->index + 1 >= array->count)
        return CLM_BPA_FAILURE;
    ++array->index;
    return CLM_BPA_SUCCESS;
}

/** Moves the array cursor to the final initialized item.
 * @param id Array ID; the array must not be empty.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if empty or missing.
 */
static inline int clm_bpaa_move_last(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->count == 0)
        return CLM_BPA_FAILURE;
    array->index = array->count - 1;
    return CLM_BPA_SUCCESS;
}

/** Moves the array cursor to a specified initialized item.
 * @param id Array ID whose cursor should move.
 * @param index Zero-based position of an initialized item.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid index/array.
 */
static inline int clm_bpaa_move_to(const clm_id id, const clm_int index)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || index < 0 || (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    array->index = (clm_uint)index;
    return CLM_BPA_SUCCESS;
}

/** Retrieves the current array cursor position.
 * @param id Array ID to query.
 * @param index Receives the zero-based cursor position.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid output/state.
 */
static inline int clm_bpaa_get_current_index_to(const clm_id id, clm_int* index)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || index == NULL || array->index > INT_MAX)
        return CLM_BPA_FAILURE;
    *index = (clm_int)array->index;
    return CLM_BPA_GET_SUCCESS;
}

/** Seals an array against capacity changes and automatic capacity growth.
 * @param id Array ID to seal; in-capacity item mutations remain allowed.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
 */
static inline int clm_bpaa_seal(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL)
        return CLM_BPA_FAILURE;
    array->sealed = 1;
    return CLM_BPA_SUCCESS;
}

/** Retrieves an array's sealed state.
 * @param id Array ID to query.
 * @param sealed Receives zero when unsealed or nonzero when sealed.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid output/array.
 */
static inline int clm_bpaa_is_sealed_to(const clm_id id, clm_byte* sealed)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || sealed == NULL)
        return CLM_BPA_FAILURE;
    *sealed = array->sealed;
    return CLM_BPA_GET_SUCCESS;
}

/** Allows capacity changes and growth for a previously sealed array.
 * @param id Array ID to unseal.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
 */
static inline int clm_bpaa_unseal(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL)
        return CLM_BPA_FAILURE;
    array->sealed = 0;
    return CLM_BPA_SUCCESS;
}

/** Sorts fixed-size items with a qsort-style three-way comparison callback.
 * @param id ID of a non-string fixed-width array to sort.
 * @param compare Comparison callback; returns negative, zero, or positive.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid array/callback.
 */
static inline int clm_bpaa_sort_with(const clm_id id, const clm_bpaa_compare_fn compare)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || compare == NULL)
        return CLM_BPA_FAILURE;
    if (array->count < 2)
        return CLM_BPA_SUCCESS;
    qsort(array->items.blocks, array->count, array->block_size, compare);
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

/** Sorts fixed-size items using a predicate that reports out-of-order pairs.
 * @param id ID of a non-string fixed-width array to sort.
 * @param predicate Callback that reports whether adjacent items are out of order.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid array/callback.
 */
static inline int clm_bpaa_sort(const clm_id id, const clm_bpaa_predicate_fn predicate)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || predicate == NULL ||
        array->type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_FAILURE;
    if (array->count < 2)
        return CLM_BPA_SUCCESS;
    const size_t block_size = array->block_size;
    unsigned char* temporary = malloc(block_size);
    if (temporary == NULL)
        return CLM_BPA_FAILURE;
    for (clm_uint i = 0; i < array->count - 1; ++i) {
        int swapped = 0;
        for (clm_uint j = 0; j < array->count - i - 1; ++j) {
            unsigned char* left =
                array->items.blocks + (size_t)j * block_size;
            unsigned char* right = left + block_size;
            if (predicate(left, right) > 0) {
                memcpy(temporary, left, block_size);
                memcpy(left, right, block_size);
                memcpy(right, temporary, block_size);
                swapped = 1;
            }
        }
        if (!swapped)
            break;
    }
    free(temporary);
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}


/** Reverses the order of all fixed-size items in an array.
 * @param id ID of a non-string array to reverse.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for a missing/string array or OOM.
 */
static inline int clm_bpaa_reverse(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_FAILURE;
    if (array->count < 2)
        return CLM_BPA_SUCCESS;
    unsigned char* temporary = malloc(array->block_size);
    if (temporary == NULL)
        return CLM_BPA_FAILURE;
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

/** Rotates a fixed-size array suffix left or right in place.
 * @param array Live fixed-width array node to modify.
 * @param index Zero-based start of the suffix to rotate.
 * @param positions Number of positions to rotate, reduced modulo the suffix length.
 * @param right Nonzero rotates right; zero rotates left.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid suffix/type/OOM.
 */
static inline int clm_bpaa_rotate(clm_bpaa_array_node* array, const clm_uint index, const clm_uint positions, const int right)
{
    if (array->type == CLM_BPA_TYPE_STRING || index >= array->count)
        return CLM_BPA_FAILURE;
    const clm_uint length = array->count - index;
    const clm_uint shift = positions % length;
    if (shift == 0)
        return CLM_BPA_SUCCESS;
    if (array->block_size == 0 ||
        (size_t)shift > ((size_t)-1) / (size_t)array->block_size)
        return CLM_BPA_FAILURE;
    unsigned char* temporary = malloc((size_t)shift * array->block_size);
    if (temporary == NULL)
        return CLM_BPA_FAILURE;
    {
        unsigned char* suffix = array->items.blocks + (size_t)index * array->block_size;
        if (right) {
            memcpy(temporary, suffix + (size_t)(length - shift) * array->block_size,
                   (size_t)shift * array->block_size);
            memmove(suffix + (size_t)shift * array->block_size, suffix,
                    (size_t)(length - shift) * array->block_size);
            memcpy(suffix, temporary, (size_t)shift * array->block_size);
        } else {
            memcpy(temporary, suffix, (size_t)shift * array->block_size);
            memmove(suffix, suffix + (size_t)shift * array->block_size,
                    (size_t)(length - shift) * array->block_size);
            memcpy(suffix + (size_t)(length - shift) * array->block_size,
                   temporary, (size_t)shift * array->block_size);
        }
    }
    free(temporary);
    array->index = 0;
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

/** Rotates the suffix beginning at index left by positions.
 * @param id ID of a non-string fixed-width array.
 * @param index Zero-based start of the suffix to rotate.
 * @param positions Number of positions to rotate the suffix left.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
static inline int clm_bpaa_shift_left(const clm_id id, const clm_uint index, const clm_uint positions)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL)
        return CLM_BPA_FAILURE;
    return clm_bpaa_rotate(array, index, positions, 0);
}

/** Rotates the suffix beginning at index right by positions.
 * @param id ID of a non-string fixed-width array.
 * @param index Zero-based start of the suffix to rotate.
 * @param positions Number of positions to rotate the suffix right.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
static inline int clm_bpaa_shift_right(const clm_id id, const clm_uint index, const clm_uint positions)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL)
        return CLM_BPA_FAILURE;
    return clm_bpaa_rotate(array, index, positions, 1);
}

/** Exchanges two fixed-size items without changing the item count.
 * @param id ID of a non-string fixed-width array.
 * @param index1 Zero-based position of the first item.
 * @param index2 Zero-based position of the second item.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid indices/type/OOM.
 */
static inline int clm_bpaa_swap(const clm_id id, const clm_int index1, const clm_int index2)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || index1 < 0 ||
        index2 < 0 || (clm_uint)index1 >= array->count ||
        (clm_uint)index2 >= array->count)
        return CLM_BPA_FAILURE;
    if (index1 == index2)
        return CLM_BPA_SUCCESS;
    unsigned char* temporary = malloc(array->block_size);
    if (temporary == NULL)
        return CLM_BPA_FAILURE;
    unsigned char* first = array->items.blocks + (size_t)index1 * array->block_size;
    unsigned char* second = array->items.blocks + (size_t)index2 * array->block_size;
    memcpy(temporary, first, array->block_size);
    memcpy(first, second, array->block_size);
    memcpy(second, temporary, array->block_size);
    free(temporary);
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

/** Copies one fixed-size item over another within the same array.
 * @param id ID of a non-string fixed-width array.
 * @param source_index Zero-based position of the item to copy.
 * @param dest_index Zero-based destination position to overwrite.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid array/index.
 */
static inline int clm_bpaa_copy(clm_id id, const clm_int source_index, const clm_int dest_index)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || source_index < 0 ||
        dest_index < 0 || (clm_uint)source_index >= array->count ||
        (clm_uint)dest_index >= array->count)
        return CLM_BPA_FAILURE;
    memmove(array->items.blocks + (size_t)dest_index * array->block_size,
            array->items.blocks + (size_t)source_index * array->block_size,
            array->block_size);
    array->deck_size = array->count;
    return CLM_BPA_SUCCESS;
}

/** Randomly shuffles fixed-size array items and resets the draw deck.
 * @param id ID of a non-string array; uses the C library rand() state.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for a missing/string array.
 */
static inline int clm_bpaa_shuffle(const clm_id id)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || array->count > INT_MAX)
        return CLM_BPA_FAILURE;
    for (clm_uint index = array->count; index > 1; --index) {
        const clm_uint other = (clm_uint)((unsigned long)rand() % index); // NOLINT(*-msc50-cpp)
        if (clm_bpaa_swap(id, (clm_int)(index - 1), (clm_int)other) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
    }
    array->deck_size = array->count;
    array->index = 0;
    return CLM_BPA_SUCCESS;
}

/** Draws a selected item from the active deck and removes it from that deck.
 * @param id Array ID of a non-string fixed-width array.
 * @param index Zero-based position within the active draw deck.
 * @param buffer Receives the drawn item bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if invalid/exhausted.
 */
static inline int clm_bpaa_draw_at_raw(const clm_id id, const clm_int index, void* buffer, const clm_uint buffer_size)
{
    clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || buffer == NULL ||
        buffer_size < array->block_size || index < 0 ||
        (clm_uint)index >= array->deck_size)
        return CLM_BPA_FAILURE;
    clm_uint last = array->deck_size - 1;
    if ((clm_uint)index != last) {
        unsigned char* selected = array->items.blocks + (size_t)index * array->block_size;
        unsigned char* tail = array->items.blocks + (size_t)last * array->block_size;
        unsigned char* temporary = malloc(array->block_size);
        if (temporary == NULL)
            return CLM_BPA_FAILURE;
        memcpy(temporary, selected, array->block_size);
        memcpy(selected, tail, array->block_size);
        memcpy(tail, temporary, array->block_size);
        free(temporary);
    }
    memcpy(buffer, array->items.blocks + (size_t)last * array->block_size,
           array->block_size);
    --array->deck_size;
    return CLM_BPA_GET_SUCCESS;
}

/** Draws the next item, reshuffling the array when its deck is exhausted.
 * @param id ID of a nonempty fixed-size array.
 * @param buffer Receives the drawn item bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid/empty input.
 */
static inline int clm_bpaa_draw_raw(const clm_id id, void* buffer, const clm_uint buffer_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(id);
    if (array == NULL || array->type == CLM_BPA_TYPE_STRING || buffer == NULL ||
        buffer_size < array->block_size)
        return CLM_BPA_FAILURE;
    if (array->deck_size == 0) {
        if (array->count == 0 || clm_bpaa_shuffle(id) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
    }
    return clm_bpaa_draw_at_raw(id, 0, buffer, buffer_size);
}


/* Borrowed access is optional: the pointer becomes invalid after delete,
 * string replacement, or any operation that relocates its allocation.
 * Pooling does NOT make use-after-delete safe. Never retain across release. */
/** Returns a temporary direct pointer to a block's payload and its byte size.
 * @param id ID of the block whose payload to borrow.
 * @param address Receives the borrowed payload pointer.
 * @param size Receives the payload size in bytes, including NUL for strings.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid input.
 * @note Do not retain the pointer across deletion or string replacement.
 */
static inline int clm_bpa_borrow(const clm_id id, void** address, size_t* size)
{
    clm_bpa_block_node* block = clm_bpa_find_block(id);
    if (block == NULL || address == NULL || size == NULL) return CLM_BPA_FAILURE;
    *address = block->data;
    *size = block->type == CLM_BPA_TYPE_STRING
        ? (size_t)block->length + 1 : block->block_size;
    return CLM_BPA_GET_SUCCESS;
}

/* Copy between two pre-created BPA blocks; no buffer pointer escapes. */
/** Copies one block's value into another pre-created block of a compatible type.
 * @param source_id ID of the block to copy from.
 * @param destination_id ID of the compatible block to overwrite.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if types/sizes are incompatible.
 */
static inline int clm_bpa_copy_block(const clm_id source_id, const clm_id destination_id)
{
    clm_bpa_block_node* source = clm_bpa_find_block(source_id);
    clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    if (source == NULL || destination == NULL || source->type != destination->type)
        return CLM_BPA_FAILURE;
    if (source->type == CLM_BPA_TYPE_STRING) {
        if (source->data == NULL) return CLM_BPA_FAILURE;
        char* fresh = clm_bpa_pool_alloc((size_t)source->length + 1);
        if (fresh == NULL) return CLM_BPA_FAILURE;
        memcpy(fresh, source->data, (size_t)source->length + 1);
        clm_bpa_pool_free(destination->data, (size_t)destination->length + 1);
        destination->data = fresh;
        destination->length = source->length;
        return CLM_BPA_SUCCESS;
    }
    if (source->block_size != destination->block_size)
        return CLM_BPA_FAILURE;
    memmove(destination->data, source->data, source->block_size);
    return CLM_BPA_SUCCESS;
}

/* Non-string BPAA: transfer an element between an array and a BPA block.
 * Both resources must exist; the block must have an exact matching size. */
/** Copies a fixed-size array item into a compatible BPA block.
 * @param array_id Source array; index selects the item to copy.
 * @param block_id Destination block; expected_size must match the item size.
 * @param index Zero-based position of the item to copy.
 * @param expected_size Expected byte size of the array item and destination block.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for incompatible resources.
 */
static inline int clm_bpaa_get(const clm_id array_id, const clm_int index, const clm_id block_id, const size_t expected_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count ||
        (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        array->block_size != block->block_size || expected_size != array->block_size)
        return CLM_BPA_FAILURE;
    memcpy(block->data, array->items.blocks + (size_t)index * array->block_size,
           array->block_size);
    return CLM_BPA_GET_SUCCESS;
}

/** Copies a compatible BPA block's value over an existing fixed-size array item.
 * @param array_id Destination array; index selects the item to overwrite.
 * @param block_id Source block; expected_size must match both payload sizes.
 * @param index Zero-based position of the existing item to overwrite.
 * @param expected_size Expected byte size of the array item and source block.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for incompatible resources.
 */
static inline int clm_bpaa_set(const clm_id array_id, const clm_int index, const clm_id block_id, const size_t expected_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING || index < 0 ||
        (clm_uint)index >= array->count ||
        (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        array->block_size != block->block_size || expected_size != array->block_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_raw(array_id, index, block->data, (clm_uint)expected_size);
}

/** Inserts a compatible BPA block's value into a fixed-size array.
 * @param array_id Destination array; index may equal its current item count.
 * @param block_id Source block; expected_size must match the item size.
 * @param index Zero-based insertion position, from zero through the current count.
 * @param expected_size Expected byte size of the array item and source block.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for incompatible input/growth.
 */
static inline int clm_bpaa_insert(const clm_id array_id, const clm_int index, const clm_id block_id, const size_t expected_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING ||
        (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        array->block_size != block->block_size || expected_size != array->block_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_raw(array_id, index, block->data, (clm_uint)expected_size);
}

/** Appends a compatible BPA block's value to a fixed-size array.
 * @param array_id Destination array; block_id identifies the source block.
 * @param block_id Source block whose value is appended.
 * @param expected_size Expected byte size of the array item and source block.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for incompatible input/growth.
 */
static inline int clm_bpaa_append(const clm_id array_id, const clm_id block_id, const size_t expected_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    if (array == NULL || array->count > INT_MAX) return CLM_BPA_FAILURE;
    return clm_bpaa_insert(array_id, (clm_int)array->count, block_id, expected_size);
}

/* String arrays: buffers are string BPA objects, copied independently. */
/** Copies a string array item into a pre-created string BPA block.
 * @param array_id Source string array; index selects the string.
 * @param block_id Destination string block whose current value is replaced.
 * @param index Zero-based position of the string to copy.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid resource types.
 */
static inline int clm_bpaa_get_string(const clm_id array_id, const clm_int index, const clm_id block_id)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type != CLM_BPA_TYPE_STRING ||
        block->type != CLM_BPA_TYPE_STRING || index < 0 || (clm_uint)index >= array->count)
        return CLM_BPA_FAILURE;
    const size_t len = strlen(array->items.strings[index]);
    if (len > (size_t)INT_MAX) return CLM_BPA_FAILURE;
    char* fresh = clm_bpa_pool_alloc(len + 1);
    if (fresh == NULL) return CLM_BPA_FAILURE;
    memcpy(fresh, array->items.strings[index], len + 1);
    clm_bpa_pool_free(block->data, (size_t)block->length + 1);
    block->data = fresh;
    block->length = (clm_uint)len;
    return CLM_BPA_GET_SUCCESS;
}

/** Replaces an existing string array item with a string BPA block's value.
 * @param array_id Destination string array; index selects the item to replace.
 * @param block_id Source string block.
 * @param index Zero-based position of the existing string to replace.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid resource/index.
 */
static inline int clm_bpaa_set_string(const clm_id array_id, const clm_int index, const clm_id block_id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || block->data == NULL)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_string_raw(array_id, index, block->data, block->length + 1) < 0
        ? CLM_BPA_FAILURE : CLM_BPA_SET_SUCCESS;
}

/** Inserts a string BPA block's value at an index in a string array.
 * @param array_id Destination string array; index may equal its item count.
 * @param block_id Source string block; its value is copied independently.
 * @param index Zero-based insertion position, from zero through the current count.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/growth.
 */
static inline int clm_bpaa_insert_string(const clm_id array_id, const clm_int index, const clm_id block_id)
{
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (block == NULL || block->type != CLM_BPA_TYPE_STRING || block->data == NULL)
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_raw(array_id, index, block->data, block->length + 1);
}

/** Appends a string BPA block's value to a string array.
 * @param array_id Destination string array.
 * @param block_id Source string block; its value is copied independently.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/growth.
 */
static inline int clm_bpaa_append_string(const clm_id array_id, const clm_id block_id)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    if (array == NULL || array->count > INT_MAX) return CLM_BPA_FAILURE;
    return clm_bpaa_insert_string(array_id, (clm_int)array->count, block_id);
}

/** Draws a fixed-size item into a compatible BPA block.
 * @param array_id Non-string array; block_id is the destination block.
 * @param block_id Destination block that receives the drawn item.
 * @param expected_size Expected byte size of the array item and destination block.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if empty/incompatible.
 */
static inline int clm_bpaa_draw(const clm_id array_id, const clm_id block_id, const size_t expected_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING || (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        block->block_size != array->block_size ||
        expected_size != array->block_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_draw_raw(array_id, block->data, block->block_size);
}

/** Draws a selected active-deck item into a compatible BPA block.
 * @param array_id Non-string array; index selects a position in the current deck.
 * @param block_id Destination block; expected_size must match both item sizes.
 * @param index Zero-based position within the active draw deck.
 * @param expected_size Expected byte size of the array item and destination block.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid/exhausted input.
 */
static inline int clm_bpaa_draw_at(const clm_id array_id, const clm_int index, const clm_id block_id, const size_t expected_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    const clm_bpa_block_node* block = clm_bpa_find_block(block_id);
    if (array == NULL || block == NULL || array->type == CLM_BPA_TYPE_STRING ||
        block->type == CLM_BPA_TYPE_STRING || (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
        block->block_size != array->block_size ||
        expected_size != array->block_size)
        return CLM_BPA_FAILURE;
    return clm_bpaa_draw_at_raw(array_id, index, block->data, block->block_size);
}


/* External memory access. Suffixes indicate transfer direction. */
static inline int clm_bpa_get_to(const clm_id id, void* destination, const clm_uint size) { return clm_bpa_get(id, destination, size); }
static inline int clm_bpa_set_from(const clm_id id, const void* source, const clm_uint size) { return clm_bpa_set(id, source, size); }
static inline int clm_bpa_get_string_to(const clm_id id, void* destination, const clm_uint size) { return clm_bpa_get_string(id, destination, size); }
static inline int clm_bpa_set_string_from(const clm_id id, const void* source, const clm_uint size) { return clm_bpa_set_string(id, source, size); }
static inline int clm_bpaa_get_to(const clm_id id, const clm_int index, void* destination, const clm_uint size) { return clm_bpaa_get_raw(id, index, destination, size); }
static inline int clm_bpaa_get_first_to(const clm_id id, void* destination, const clm_uint size) { return clm_bpaa_get_first_raw(id, destination, size); }
static inline int clm_bpaa_get_last_to(const clm_id id, void* destination, const clm_uint size) { return clm_bpaa_get_last_raw(id, destination, size); }
static inline int clm_bpaa_get_current_to(const clm_id id, void* destination, const clm_uint size) { return clm_bpaa_get_current_raw(id, destination, size); }
static inline int clm_bpaa_get_string_to(const clm_id id, const clm_int index, void* destination, const clm_uint size) { return clm_bpaa_get_string_raw(id, index, destination, size); }
static inline int clm_bpaa_set_from(const clm_id id, const clm_int index, const void* source, const clm_uint size) { return clm_bpaa_set_raw(id, index, source, size); }
static inline int clm_bpaa_set_string_from(const clm_id id, const clm_int index, const void* source, const clm_uint size) { return clm_bpaa_set_string_raw(id, index, source, size); }
static inline int clm_bpaa_insert_from(const clm_id id, const clm_int index, const void* source, const clm_uint size) { return clm_bpaa_insert_raw(id, index, source, size); }
static inline int clm_bpaa_append_from(const clm_id id, const void* source, const clm_uint size) { return clm_bpaa_append_raw(id, source, size); }
static inline int clm_bpaa_draw_to(const clm_id id, void* destination, const clm_uint size) { return clm_bpaa_draw_raw(id, destination, size); }
static inline int clm_bpaa_draw_at_to(const clm_id id, const clm_int index, void* destination, const clm_uint size) { return clm_bpaa_draw_at_raw(id, index, destination, size); }

/* Managed metadata transfers validate destination types and sizes. */
static inline int clm_bpaa_write_u32(const clm_id destination_id, const clm_uint value)
{
    clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    if (destination == NULL || destination->type != CLM_BPA_TYPE_UNSIGNED_INT ||
        destination->block_size != sizeof(value) || destination->data == NULL)
        return CLM_BPA_FAILURE;
    memcpy(destination->data, &value, sizeof(value));
    return CLM_BPA_GET_SUCCESS;
}
static inline int clm_bpaa_write_i32(const clm_id destination_id, const clm_int value)
{
    clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    if (destination == NULL || destination->type != CLM_BPA_TYPE_INT ||
        destination->block_size != sizeof(value) || destination->data == NULL)
        return CLM_BPA_FAILURE;
    memcpy(destination->data, &value, sizeof(value));
    return CLM_BPA_GET_SUCCESS;
}
static inline int clm_bpaa_write_byte(const clm_id destination_id, const clm_byte value)
{
    clm_bpa_block_node* destination = clm_bpa_find_block(destination_id);
    if (destination == NULL || destination->type != CLM_BPA_TYPE_BYTE ||
        destination->block_size != sizeof(value) || destination->data == NULL)
        return CLM_BPA_FAILURE;
    memcpy(destination->data, &value, sizeof(value));
    return CLM_BPA_GET_SUCCESS;
}
static inline int clm_bpaa_get_count(const clm_id array_id, const clm_id destination_id)
{
    clm_uint value;
    return clm_bpaa_get_count_to(array_id, &value) == CLM_BPA_GET_SUCCESS ? clm_bpaa_write_u32(destination_id, value) : CLM_BPA_FAILURE;
}
static inline int clm_bpaa_get_capacity(const clm_id array_id, const clm_id destination_id)
{
    clm_uint value;
    return clm_bpaa_get_capacity_to(array_id, &value) == CLM_BPA_GET_SUCCESS ? clm_bpaa_write_u32(destination_id, value) : CLM_BPA_FAILURE;
}
static inline int clm_bpaa_get_length(const clm_id array_id, const clm_id destination_id)
{
    clm_uint value;
    return clm_bpaa_get_length_to(array_id, &value) == CLM_BPA_GET_SUCCESS ? clm_bpaa_write_u32(destination_id, value) : CLM_BPA_FAILURE;
}
static inline int clm_bpaa_get_current_index(const clm_id array_id, const clm_id destination_id)
{
    clm_int value;
    return clm_bpaa_get_current_index_to(array_id, &value) == CLM_BPA_GET_SUCCESS ? clm_bpaa_write_i32(destination_id, value) : CLM_BPA_FAILURE;
}
static inline int clm_bpaa_is_sealed(const clm_id array_id, const clm_id destination_id)
{
    clm_byte value;
    return clm_bpaa_is_sealed_to(array_id, &value) == CLM_BPA_GET_SUCCESS ? clm_bpaa_write_byte(destination_id, value) : CLM_BPA_FAILURE;
}
static inline int clm_bpaa_get_first(const clm_id array_id, const clm_id destination_id, const size_t expected_size)
{ return clm_bpaa_get(array_id, 0, destination_id, expected_size); }
static inline int clm_bpaa_get_last(const clm_id array_id, const clm_id destination_id, const size_t expected_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    if (array == NULL || array->count == 0 || array->count - 1 > INT_MAX) return CLM_BPA_FAILURE;
    return clm_bpaa_get(array_id, (clm_int)(array->count - 1), destination_id, expected_size);
}
static inline int clm_bpaa_get_current(const clm_id array_id, const clm_id destination_id, const size_t expected_size)
{
    const clm_bpaa_array_node* array = clm_bpaa_find_array(array_id);
    if (array == NULL || array->index >= array->count || array->index > INT_MAX) return CLM_BPA_FAILURE;
    return clm_bpaa_get(array_id, (clm_int)array->index, destination_id, expected_size);
}

/* Force-release all registered BPA/BPAA resources, hash registries, and
 * pooled slabs. Outstanding references are intentionally ignored, matching
 * explicit clm_bpa_delete()/clm_bpaa_delete() semantics.
 *
 * Not thread-safe; call only after all BPA consumers have stopped. Borrowed
 * pointers and handles become invalid. Idempotent; BPA may be used again
 * after shutdown and will initialize its registries/pools lazily.
 */
/** Releases all pool slabs and registry storage after all BPA resources are gone.
 * @return CLM_BPA_SUCCESS when cleanup completes; failure if pooled blocks remain.
 * @note Call at process/module shutdown; it invalidates registry state.
 */
static inline int clm_bpa_shutdown(void)
{
    size_t i;

    /* Release the arrays first, including their individually pooled strings. */
    for (i = 0; i < clm_bpa_global_arrays_capacity; ++i) {
        clm_bpaa_array_node* array = clm_bpa_global_arrays[i];
        if (array != NULL && array != CLM_BPA_ARRAY_TOMBSTONE)
            (void)clm_bpa_remove_array(array->id);
    }
    free(clm_bpa_global_arrays);
    clm_bpa_global_arrays = NULL;
    clm_bpa_global_arrays_capacity = 0;
    clm_bpa_global_arrays_count = 0;
    clm_bpa_global_arrays_tombstones = 0;

    for (i = 0; i < clm_bpa_global_blocks_capacity; ++i) {
        clm_bpa_block_node* block = clm_bpa_global_blocks[i];
        if (block != NULL && block != CLM_BPA_BLOCK_TOMBSTONE)
            (void)clm_bpa_remove_block(block->id);
    }
    free(clm_bpa_global_blocks);
    clm_bpa_global_blocks = NULL;
    clm_bpa_global_blocks_capacity = 0;
    clm_bpa_global_blocks_count = 0;
    clm_bpa_global_blocks_tombstones = 0;

    const int status = clm_bpa_pool_cleanup();
    return status;
}

#ifdef CLM_BPA_IMPLEMENTATION
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
#endif
