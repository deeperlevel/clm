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
#include "arena.h"

#include <limits.h>

/*
 * Block Pool Allocation (BPA) is CLM's ID-addressed storage layer. A caller
 * creates typed blocks or arrays under unique clm_id values and accesses them
 * through this registry instead of owning each payload allocation directly.
 * Fixed-size payloads and string bytes use recyclable size-class pools;
 * registries and growable array backing stores use ordinary heap allocations.
 *
 * BPA is process-global and not internally synchronized. Callers must
 * serialize access when using it from multiple threads. Pointers returned by
 * borrow/advanced APIs are temporary views, not independent ownership.
 */

/** Result codes distinguish general operations from get/set operations. */
#define CLM_BPA_FAILURE (-1)
#define CLM_BPA_SUCCESS 0
#define CLM_BPA_GET_SUCCESS 1
#define CLM_BPA_SET_SUCCESS 2

/** Runtime type tags stored with blocks and arrays. */
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
#define CLM_BPA_TYPE_ANY 99

/** Compact serialized block metadata; pointer is an encoded address value. */
typedef struct {
    clm_byte type;
    clm_ushort refcount;
    clm_uint block_size;
    clm_byte encoding;
    clm_uint length;
    uintptr_t pointer;
} clm_block_allocation;

/** Compact serialized array metadata; payload layout depends on its type. */
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

/** Registry node for one ID-addressed value.
 * Fixed-size values have block_size bytes; strings have length UTF-8 bytes
 * followed by a NUL terminator. String encoding is metadata for consumers.
 */
typedef struct clm_bpa_block_node {
    clm_id id;
    clm_byte type;
    clm_ushort refcount;
    clm_uint block_size;
    clm_byte encoding;
    clm_uint length;
    void* data;
} clm_bpa_block_node;

/** Registry node for an ID-addressed growable array (BPAA).
 * count is the number of initialized elements; capacity is allocated slots.
 * Fixed-width items are contiguous, while string arrays own separate strings.
 * index is the traversal cursor; deck_size tracks remaining shuffled draws.
 */
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

/* A bounded set of recyclable size-class pools. Each slab is backed by a
 * clm_arena allocation and remains borrowed until pool cleanup; released slots
 * are reused in-band. Large allocations fall back to calloc/free. The pool is
 * intentionally unsynchronized, like the BPA registries. */
#define CLM_BPA_POOL_SLAB_SLOTS 32u
#define CLM_BPA_POOL_MAX_SLOT 4096u
#define CLM_BPA_POOL_CLASSES 10u /* 8, 16, 32, ... 4096 */

typedef struct clm_bpa_pool_slab {
    struct clm_bpa_pool_slab* next;
    void* memory;
    /* Private arena backing; its borrow pins the slab address until cleanup. */
    clm_id arena_id;
    clm_arena_borrowed borrowed;
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
size_t clm_bpa_pool_class_for(size_t bytes);

/** Allocates zero-initialized storage from a slab pool or the heap fallback.
 * @param bytes Number of bytes requested; zero returns NULL.
 * @return A suitably aligned allocation, or NULL if allocation fails.
 */
void* clm_bpa_pool_alloc(size_t bytes);

/** Returns a pooled allocation to its free list, or frees a heap fallback.
 * @param pointer Allocation to release; NULL is ignored.
 * @param bytes Original allocation size, used to select the matching class.
 */
void clm_bpa_pool_free(void* pointer, const size_t bytes);

/* Call only when all BPA blocks are released. Fails rather than invalidating
 * live allocations. Array allocations are independent of these slab pools. */
/** Releases all pool slabs after verifying that no pooled objects remain live.
 * @return CLM_BPA_SUCCESS when cleaned up; CLM_BPA_FAILURE if a class is active.
 */
int clm_bpa_pool_cleanup(void);

/** Reports usage for the pool class selected by a requested object size.
 * @param bytes Size used to select a pooled class.
 * @param active Receives the number of currently checked-out slots.
 * @param capacity Receives the total slots reserved by that class.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid size or output.
 */
int clm_bpa_pool_stats(size_t bytes, size_t* active, size_t* capacity);

/** Mixes a 64-bit ID into a hash suitable for the global open-addressed tables.
 * @param id Identifier to hash.
 * @return Mixed hash value; callers mask it to the table capacity.
 */
size_t clm_bpa_hash_id(clm_id id);

/** Rebuilds the block registry at a new power-of-two capacity.
 * @param capacity New slot count; must be a nonzero power of two.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if allocation fails.
 */
int clm_bpa_rehash_blocks(size_t capacity);

/** Rebuilds the array registry at a new power-of-two capacity.
 * @param capacity New slot count; must be a nonzero power of two.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if allocation fails.
 */
int clm_bpa_rehash_arrays(size_t capacity);

/** Ensures the block registry has room and compacts tombstones if needed.
 * @return CLM_BPA_SUCCESS when insertion can proceed; otherwise failure.
 */
int clm_bpa_prepare_blocks(void);

/** Ensures the array registry has room and compacts tombstones if needed.
 * @return CLM_BPA_SUCCESS when insertion can proceed; otherwise failure.
 */
int clm_bpa_prepare_arrays(void);

/** Resolves the byte size of a built-in fixed-size BPA type.
 * @param type BPA type tag to resolve.
 * @return Type size in bytes, or zero for variable-sized/unsupported types.
 */
clm_uint clm_bpa_type_size(clm_byte type);

/** Looks up a block by ID in the global block registry.
 * @param id ID of the block to find.
 * @return The block node, or NULL when no such block exists.
 */
clm_bpa_block_node* clm_bpa_find_block(clm_id id);

/** Checks whether a block with the given ID exists in the global registry.
 * @param id ID of the block to check.
 * @return true if the block exists; otherwise false.
 */
bool clm_bpa_is_existing(clm_id id);

/** Looks up an array by ID in the global array registry.
 * @param id ID of the array to find.
 * @return The array node, or NULL when no such array exists.
 */
clm_bpaa_array_node* clm_bpaa_find_array(clm_id id);

/** Checks whether an array with the given ID exists in the global registry.
 * @param id ID of the array to check.
 * @return true if the array exists; otherwise false.
 */
bool clm_bpaa_is_existing(clm_id id);

/** Inserts a new block node into a prepared registry slot.
 * @param block Initialized node with an ID not already present in the table.
 */
void clm_bpa_insert_block(clm_bpa_block_node* block);

/** Inserts a new array node into a prepared registry slot.
 * @param array Initialized node with an ID not already present in the table.
 */
void clm_bpa_insert_array(clm_bpaa_array_node* array);

/** Removes a block from the registry and returns its storage to the pool.
 * @param id ID of the block to release.
 * @return CLM_BPA_SUCCESS if removed; otherwise CLM_BPA_FAILURE.
 */
int clm_bpa_remove_block(clm_id id);

/** Removes an array and releases its items and backing storage.
 * @param id ID of the array to release.
 * @return CLM_BPA_SUCCESS if removed; otherwise CLM_BPA_FAILURE.
 */
int clm_bpa_remove_array(clm_id id);

/** Copies a bounded string into pool storage and records its byte length.
 * @param buffer Source bytes; a NUL within buffer_size ends the string.
 * @param buffer_size Number of readable source bytes.
 * @param value Receives the allocated, NUL-terminated copy.
 * @param length Receives the copied length excluding the terminator.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
int clm_bpa_copy_string(const void* buffer, clm_uint buffer_size, char** value, clm_uint* length);

/** Creates a zero-initialized block with an explicit size for custom structs.
 * @param id Unique global ID; type selects a built-in, string, or struct block.
 * @param type Block type; use CLM_BPA_TYPE_STRUCT for custom-sized blocks.
 * @param block_size Required byte size for STRUCT; pass zero for built-in types.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/duplicate ID.
 */
int clm_bpa_create_sized(clm_id id, clm_byte type, clm_uint block_size);

/** Creates a zero-initialized block of a built-in type.
 * @param id Unique global ID to assign to the block.
 * @param type Supported fixed-size or string BPA type; STRUCT requires create_sized.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if creation is not possible.
 */
int clm_bpa_create(clm_id id, clm_byte type);

/** Immediately releases a block regardless of its reference count.
 * @param id ID of the block to delete.
 * @return CLM_BPA_SUCCESS if deleted; otherwise CLM_BPA_FAILURE.
 */
int clm_bpa_delete(clm_id id);

/** Adds one reference to an existing block.
 * @param id ID of the block to retain.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing/at the refcount limit.
 */
int clm_bpa_ref(clm_id id);

/** Releases one reference and deletes the block when its count reaches zero.
 * @param id ID of the block to release.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing or already unreferenced.
 */
int clm_bpa_unref(clm_id id);

/** Copies a fixed-size block into a caller-provided output buffer.
 * @param id ID of a non-string block.
 * @param buffer Receives the copied block contents.
 * @param buffer_size Destination capacity in bytes; must cover the block size.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
 */
int clm_bpa_get(clm_id id, void* buffer, clm_uint buffer_size);

/** Replaces the contents of a fixed-size block with caller-provided bytes.
 * @param id ID of a non-string block.
 * @param buffer Source bytes to copy into the block.
 * @param buffer_size Source size in bytes; must exactly match the block size.
 * @return CLM_BPA_SET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
 */
int clm_bpa_set(clm_id id, const void* buffer, clm_uint buffer_size);

/** Copies a string block, including its NUL terminator, into the output buffer.
 * @param id ID of a string block.
 * @param buffer Receives the string and its NUL terminator.
 * @param buffer_size Destination capacity in bytes, including room for the terminator.
 * @return String byte length excluding NUL, or CLM_BPA_FAILURE if it does not fit.
 */
int clm_bpa_get_string(clm_id id, void* buffer, clm_uint buffer_size);

/** Replaces a string block with a bounded copy of the supplied string bytes.
 * @param id ID of a string block.
 * @param buffer Source string bytes; a NUL within the bound terminates the string.
 * @param buffer_size Number of readable source bytes.
 * @return String byte length excluding NUL, or CLM_BPA_FAILURE on error.
 */
int clm_bpa_set_string(clm_id id, const void* buffer, clm_uint buffer_size);

/** Replaces the content of an existing block.
 * @param id ID of the block to update.
 * @param buffer Source content; string data is copied into the internal pool.
 * @param buffer_size Number of readable source bytes.
 * @return CLM_BPA_SET_SUCCESS for fixed-size blocks, string byte length for
 *         string blocks, or CLM_BPA_FAILURE on invalid input.
 */
int clm_bpa_replace(
    clm_id id,
    const void* buffer,
    clm_uint buffer_size);

/* BPAA stores a typed sequence under its own ID. Fixed-width items occupy one
 * contiguous backing buffer; string arrays own a pointer table and a pooled
 * copy of each string. count and capacity are deliberately separate. */
/** Creates an array and reserves initial slots, with an explicit struct item size.
 * @param id Unique global array ID; reserved is the initial item capacity.
 * @param type Item type; block_size is required only for STRUCT items.
 * @param reserved Initial number of reserved item slots; must be nonnegative.
 * @param block_size Item byte width for STRUCT; pass zero for built-in/string types.
 * @param encoding Encoding metadata retained for string arrays.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/duplicate ID.
 */
int clm_bpaa_create_sized(clm_id id, clm_byte type, clm_int reserved, clm_uint block_size, clm_byte encoding);

/** Creates an array of a built-in type with the requested reserved capacity.
 * @param id Unique global array ID; reserved is the initial slot capacity.
 * @param type Supported built-in or string type; STRUCT requires create_sized.
 * @param reserved Initial number of reserved item slots; must be nonnegative.
 * @param encoding Encoding metadata for string arrays.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE when creation cannot proceed.
 */
int clm_bpaa_create(clm_id id, clm_byte type, clm_int reserved, clm_byte encoding);

/** Immediately deletes an array and releases its items and reserved storage.
 * @param id ID of the array to delete.
 * @return CLM_BPA_SUCCESS if deleted; otherwise CLM_BPA_FAILURE.
 */
int clm_bpaa_delete(clm_id id);

/** Adds one reference to an existing array.
 * @param id ID of the array to retain.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing/at the refcount limit.
 */
int clm_bpaa_ref(clm_id id);

/** Releases one array reference and deletes the array when its count reaches zero.
 * @param id ID of the array to release.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if missing/unreferenced.
 */
int clm_bpaa_unref(clm_id id);


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
int clm_bpaa_slice(clm_id source_id, clm_int start, clm_int length, clm_id destination_id);

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
int clm_bpaa_slice_borrow(clm_id source_id, clm_int start, clm_int length, const void** buffer, size_t* buffer_size);

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
int clm_bpaa_slice_to(clm_id source_id, clm_int start, clm_int length, void* buffer, size_t buffer_size);

/** Copies a fixed-size array item into a caller-provided buffer.
 * @param id Array ID of a non-string fixed-width array.
 * @param index Zero-based item position to copy.
 * @param buffer Receives the item bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
 */
int clm_bpaa_get_raw(clm_id id, clm_int index, void* buffer, clm_uint buffer_size);

/** Copies one string item, including its NUL terminator, to the output buffer.
 * @param id Array ID of a string array.
 * @param index Zero-based string position to copy.
 * @param buffer Receives the string and its NUL terminator.
 * @param buffer_size Destination capacity in bytes, including room for NUL.
 * @return String byte length excluding NUL, or CLM_BPA_FAILURE if it does not fit.
 */
int clm_bpaa_get_string_raw(clm_id id, clm_int index, void* buffer, clm_uint buffer_size);

/** Replaces a string array item with a bounded copy of caller-provided bytes.
 * @param id Array ID of a string array.
 * @param index Zero-based position of the existing string to replace.
 * @param buffer Source string bytes; a NUL within the bound terminates the string.
 * @param buffer_size Number of readable source bytes.
 * @return String byte length excluding NUL, or CLM_BPA_FAILURE on error.
 */
int clm_bpaa_set_string_raw(clm_id id, clm_int index, const void* buffer, clm_uint buffer_size);

/** Copies the first fixed-size array item to the caller's buffer.
 * @param id Array ID of a non-string fixed-width array.
 * @param buffer Receives the first item's bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE for empty/invalid input.
 */
int clm_bpaa_get_first_raw(clm_id id, void* buffer, clm_uint buffer_size);

/** Copies the last fixed-size array item to the caller's buffer.
 * @param id Array ID of a non-string fixed-width array.
 * @param buffer Receives the last item's bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE for empty/invalid input.
 */
int clm_bpaa_get_last_raw(clm_id id, void* buffer, clm_uint buffer_size);

/** Retrieves the number of initialized items in an array.
 * @param id Array ID to query.
 * @param count Receives the number of initialized items.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for a missing array/null output.
 */
int clm_bpaa_get_count_to(clm_id id, clm_uint* count);

/** Retrieves the current item's string length, excluding its NUL terminator.
 * @param id String-array ID to query.
 * @param length Receives the current string's byte length, excluding NUL.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid/current-empty state.
 */
int clm_bpaa_get_length_to(clm_id id, clm_uint* length);

/** Retrieves the number of item slots currently reserved by an array.
 * @param id Array ID to query.
 * @param capacity Receives the number of reserved item slots.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for a missing array/null output.
 */
int clm_bpaa_get_capacity_to(clm_id id, clm_uint* capacity);

/** Resizes an array's backing storage while preserving its existing items.
 * @param array Internal array node to resize; its capacity is updated on success.
 * @param capacity New slot count, which must be at least the live item count.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if size arithmetic/allocation fails.
 */
int clm_bpaa_resize(clm_bpaa_array_node* array, clm_uint capacity);

/** Grows storage geometrically when needed, unless the array is sealed.
 * @param array Internal array node to grow.
 * @param needed Minimum slot count required by the pending insertion.
 * @return CLM_BPA_SUCCESS if capacity is sufficient; otherwise failure.
 */
int clm_bpaa_ensure_capacity(clm_bpaa_array_node* array, clm_uint needed);

/** Adds an explicit number of reserved slots to an unsealed array.
 * @param id Array ID to extend.
 * @param additional_capacity Number of additional slots to reserve; must be positive.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for sealed/invalid/overflow cases.
 */
int clm_bpaa_extend(clm_id id, clm_uint additional_capacity);

/** Reduces reserved slots without discarding live items on an unsealed array.
 * @param id Array ID to shrink.
 * @param new_capacity New slot capacity; must be below current capacity and at least count.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE when shrinking is not permitted.
 */
int clm_bpaa_shrink(clm_id id, clm_uint new_capacity);

/** Copies the item at the array's current cursor into the caller's buffer.
 * @param id Array ID of a non-string fixed-width array.
 * @param buffer Receives the current item's bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS on copy, or CLM_BPA_FAILURE for invalid state.
 */
int clm_bpaa_get_current_raw(clm_id id, void* buffer, clm_uint buffer_size);

/** Replaces a fixed-size item without changing array count or capacity.
 * @param id Array ID of a non-string fixed-width array.
 * @param index Zero-based position of the existing item to replace.
 * @param buffer Source bytes to copy into the item.
 * @param buffer_size Source size in bytes; must exactly match the item size.
 * @return CLM_BPA_SET_SUCCESS on copy, or CLM_BPA_FAILURE on invalid input.
 */
int clm_bpaa_set_raw(clm_id id, clm_int index, const void* buffer, clm_uint buffer_size);

/** Inserts one item at an index, growing storage only if the reserve is full.
 * @param id Array ID to modify.
 * @param index Zero-based insertion position, from zero through the current count.
 * @param buffer Fixed-width item bytes or source string bytes.
 * @param buffer_size Item byte size, or readable source bound for a string.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
int clm_bpaa_insert_raw(clm_id id, clm_int index, const void* buffer, clm_uint buffer_size);

/** Appends one fixed-size item or bounded string to an array.
 * @param id Array ID to modify.
 * @param buffer Fixed-width item bytes or source string bytes.
 * @param buffer_size Item byte size, or readable source bound for a string.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/growth failure.
 */
int clm_bpaa_append_raw(clm_id id, const void* buffer, clm_uint buffer_size);

/** Removes an item, compacts following items, and retains reserved capacity.
 * @param id Array ID to modify.
 * @param index Zero-based position of the initialized item to remove.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array/index is invalid.
 */
int clm_bpaa_remove(clm_id id, clm_int index);

/** Removes the first item while retaining the array's reserve.
 * @param id Array ID; the array must contain at least one item.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if empty or missing.
 */
int clm_bpaa_remove_first(clm_id id);

/** Removes the last item while retaining the array's reserve.
 * @param id Array ID; the array must contain at least one item.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if empty or missing.
 */
int clm_bpaa_remove_last(clm_id id);

/** Removes all items and resets the cursor/deck without releasing capacity.
 * @param id Array ID to clear.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array does not exist.
 */
int clm_bpaa_clear(clm_id id);

/* Advanced borrowed-array access. The caller must follow the lifetime contract:
 * explicit delete/shutdown invalidates the borrowed pointer immediately. */
/** Borrows array metadata and retains one reference against ordinary unref.
 * @param id Array ID to borrow.
 * @param out_array Receives the internal metadata pointer; initialized to NULL.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/missing array.
 * @note This does not prevent explicit delete or shutdown from invalidating it.
 */
int clm_bpaa_advanced_borrow(clm_id id, clm_bpaa_array_node** out_array);

/** Releases an advanced borrow and nulls its pointer.
 * @param borrowed Address of a live borrowed pointer.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid/released input.
 * @note Do not call after explicit deletion or shutdown invalidated the pointer.
 */
int clm_bpaa_advanced_release(clm_bpaa_array_node** borrowed);

/** Returns the contiguous fixed-width payload, or NULL for string arrays.
 * @param array A live pointer returned by clm_bpaa_advanced_borrow.
 * @return Mutable payload address; invalidated by resize, delete, or shutdown.
 */
void* clm_bpaa_advanced_data(const clm_bpaa_array_node* array);

/** Retrieves a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for a non-string array.
 * @param index Zero-based position of the item to retrieve.
 * @param buffer Receives the item bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return Operation status or value described in the summary above.
 */
int clm_bpaa_advanced_get_raw(const clm_bpaa_array_node* array, clm_int index, void* buffer, clm_uint buffer_size);

/** Replaces a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for a non-string array.
 * @param index Zero-based position of the existing item to replace.
 * @param buffer Source bytes to copy into the item.
 * @param buffer_size Source size in bytes; must exactly match the item size.
 * @return Operation status or value described in the summary above.
 */
int clm_bpaa_advanced_set_raw(clm_bpaa_array_node* array, clm_int index, const void* buffer, clm_uint buffer_size);

/** Inserts a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for an unsealed non-string array.
 * @param index Zero-based insertion position, from zero through the current count.
 * @param buffer Source bytes for the new item.
 * @param buffer_size Source size in bytes; must exactly match the item size.
 * @return Operation status or value described in the summary above.
 */
int clm_bpaa_advanced_insert_raw(
    clm_bpaa_array_node* array,
    const clm_int index,
    const void* buffer,
    const clm_uint buffer_size);

/** Appends a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for an unsealed non-string array.
 * @param buffer Source bytes for the new item.
 * @param buffer_size Source size in bytes; must exactly match the item size.
 * @return Operation status or value described in the summary above.
 */
int clm_bpaa_advanced_append_raw(clm_bpaa_array_node* array, const void* buffer, clm_uint buffer_size);

/** Removes a fixed-width element without performing an ID lookup.
 * @param array Live borrowed metadata for a non-string array.
 * @param index Zero-based position of the item to remove.
 * @return Operation status or value described in the summary above.
 */
int clm_bpaa_advanced_remove(clm_bpaa_array_node* array, clm_int index);

/** Clears a fixed-width array without releasing its reserved capacity.
 * @param array Live borrowed metadata for a non-string array.
 * @return Operation status or value described in the summary above.
 */
int clm_bpaa_advanced_clear(clm_bpaa_array_node* array);

/** Reserves additional slots without performing an ID lookup.
 * @param array Live borrowed metadata for an unsealed array.
 * @param additional_capacity Number of additional slots to reserve; must be positive.
 * @return Operation status or value described in the summary above.
 */
int clm_bpaa_advanced_extend(clm_bpaa_array_node* array, clm_uint additional_capacity);

/** Shrinks reserved slots without performing an ID lookup.
 * @param array Live borrowed metadata for an unsealed array.
 * @param new_capacity New capacity; must be below current capacity and at least count.
 * @return Operation status or value described in the summary above.
 */
int clm_bpaa_advanced_shrink(clm_bpaa_array_node* array, clm_uint new_capacity);

/** Moves the array cursor to index zero.
 * @param id Array ID whose cursor should be reset.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
 */
int clm_bpaa_reset(clm_id id);

/** Moves the array cursor to its first item.
 * @param id Array ID whose cursor should move.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
 */
int clm_bpaa_move_first(const clm_id id);

/** Moves the array cursor one item toward the beginning.
 * @param id Array ID; the cursor must currently be greater than zero.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE at the beginning/missing array.
 */
int clm_bpaa_move_previous(clm_id id);

/** Moves the array cursor one item toward the end.
 * @param id Array ID; a next item must exist.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE at the end/missing array.
 */
int clm_bpaa_move_next(clm_id id);

/** Moves the array cursor to the final initialized item.
 * @param id Array ID; the array must not be empty.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if empty or missing.
 */
int clm_bpaa_move_last(clm_id id);

/** Moves the array cursor to a specified initialized item.
 * @param id Array ID whose cursor should move.
 * @param index Zero-based position of an initialized item.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid index/array.
 */
int clm_bpaa_move_to(clm_id id, clm_int index);

/** Retrieves the current array cursor position.
 * @param id Array ID to query.
 * @param index Receives the zero-based cursor position.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid output/state.
 */
int clm_bpaa_get_current_index_to(clm_id id, clm_int* index);

/** Seals an array against capacity changes and automatic capacity growth.
 * @param id Array ID to seal; in-capacity item mutations remain allowed.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
 */
int clm_bpaa_seal(clm_id id);

/** Retrieves an array's sealed state.
 * @param id Array ID to query.
 * @param sealed Receives zero when unsealed or nonzero when sealed.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid output/array.
 */
int clm_bpaa_is_sealed_to(clm_id id, clm_byte* sealed);

/** Allows capacity changes and growth for a previously sealed array.
 * @param id Array ID to unseal.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the array is missing.
 */
int clm_bpaa_unseal(clm_id id);

/** Sorts fixed-size items with a qsort-style three-way comparison callback.
 * @param id ID of a non-string fixed-width array to sort.
 * @param compare Comparison callback; returns negative, zero, or positive.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid array/callback.
 */
int clm_bpaa_sort_with(clm_id id, clm_bpaa_compare_fn compare);

/** Sorts fixed-size items using a predicate that reports out-of-order pairs.
 * @param id ID of a non-string fixed-width array to sort.
 * @param predicate Callback that reports whether adjacent items are out of order.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid array/callback.
 */
int clm_bpaa_sort(clm_id id, clm_bpaa_predicate_fn predicate);


/** Reverses the order of all fixed-size items in an array.
 * @param id ID of a non-string array to reverse.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for a missing/string array or OOM.
 */
int clm_bpaa_reverse(clm_id id);

/** Rotates a fixed-size array suffix left or right in place.
 * @param array Live fixed-width array node to modify.
 * @param index Zero-based start of the suffix to rotate.
 * @param positions Number of positions to rotate, reduced modulo the suffix length.
 * @param right Nonzero rotates right; zero rotates left.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid suffix/type/OOM.
 */
int clm_bpaa_rotate(clm_bpaa_array_node* array, clm_uint index, clm_uint positions, int right);

/** Rotates the suffix beginning at index left by positions.
 * @param id ID of a non-string fixed-width array.
 * @param index Zero-based start of the suffix to rotate.
 * @param positions Number of positions to rotate the suffix left.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
int clm_bpaa_shift_left(clm_id id, clm_uint index, clm_uint positions);

/** Rotates the suffix beginning at index right by positions.
 * @param id ID of a non-string fixed-width array.
 * @param index Zero-based start of the suffix to rotate.
 * @param positions Number of positions to rotate the suffix right.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
 */
int clm_bpaa_shift_right(clm_id id, clm_uint index, clm_uint positions);

/** Exchanges two fixed-size items without changing the item count.
 * @param id ID of a non-string fixed-width array.
 * @param index1 Zero-based position of the first item.
 * @param index2 Zero-based position of the second item.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid indices/type/OOM.
 */
int clm_bpaa_swap(clm_id id, clm_int index1, clm_int index2);

/** Copies one fixed-size item over another within the same array.
 * @param id ID of a non-string fixed-width array.
 * @param source_index Zero-based position of the item to copy.
 * @param dest_index Zero-based destination position to overwrite.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for an invalid array/index.
 */
int clm_bpaa_copy(clm_id id, clm_int source_index, clm_int dest_index);

/** Randomly shuffles fixed-size array items and resets the draw deck.
 * @param id ID of a non-string array; uses the C library rand() state.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for a missing/string array.
 */
int clm_bpaa_shuffle(clm_id id);

/** Draws a selected item from the active deck and removes it from that deck.
 * @param id Array ID of a non-string fixed-width array.
 * @param index Zero-based position within the active draw deck.
 * @param buffer Receives the drawn item bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if invalid/exhausted.
 */
int clm_bpaa_draw_at_raw(clm_id id, clm_int index, void* buffer, clm_uint buffer_size);

/** Draws the next item, reshuffling the array when its deck is exhausted.
 * @param id ID of a nonempty fixed-size array.
 * @param buffer Receives the drawn item bytes.
 * @param buffer_size Destination capacity in bytes; must fit one item.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid/empty input.
 */
int clm_bpaa_draw_raw(clm_id id, void* buffer, clm_uint buffer_size);


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
int clm_bpa_borrow(clm_id id, void** address, size_t* size);

/* Copy between two pre-created BPA blocks; no buffer pointer escapes. */
/** Copies one block's value into another pre-created block of a compatible type.
 * @param source_id ID of the block to copy from.
 * @param destination_id ID of the compatible block to overwrite.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if types/sizes are incompatible.
 */
int clm_bpa_copy_block(clm_id source_id, clm_id destination_id);

/* Non-string BPAA: transfer an element between an array and a BPA block.
 * Both resources must exist; the block must have an exact matching size. */
/** Copies a fixed-size array item into a compatible BPA block.
 * @param array_id Source array; index selects the item to copy.
 * @param block_id Destination block; expected_size must match the item size.
 * @param index Zero-based position of the item to copy.
 * @param expected_size Expected byte size of the array item and destination block.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for incompatible resources.
 */
int clm_bpaa_get(clm_id array_id, clm_int index, clm_id block_id, size_t expected_size);

/** Copies a compatible BPA block's value over an existing fixed-size array item.
 * @param array_id Destination array; index selects the item to overwrite.
 * @param block_id Source block; expected_size must match both payload sizes.
 * @param index Zero-based position of the existing item to overwrite.
 * @param expected_size Expected byte size of the array item and source block.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for incompatible resources.
 */
int clm_bpaa_set(clm_id array_id, clm_int index, clm_id block_id, size_t expected_size);

/** Inserts a compatible BPA block's value into a fixed-size array.
 * @param array_id Destination array; index may equal its current item count.
 * @param block_id Source block; expected_size must match the item size.
 * @param index Zero-based insertion position, from zero through the current count.
 * @param expected_size Expected byte size of the array item and source block.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for incompatible input/growth.
 */
int clm_bpaa_insert(clm_id array_id, clm_int index, clm_id block_id, size_t expected_size);

/** Appends a compatible BPA block's value to a fixed-size array.
 * @param array_id Destination array; block_id identifies the source block.
 * @param block_id Source block whose value is appended.
 * @param expected_size Expected byte size of the array item and source block.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for incompatible input/growth.
 */
int clm_bpaa_append(clm_id array_id, clm_id block_id, size_t expected_size);

/* String arrays: buffers are string BPA objects, copied independently. */
/** Copies a string array item into a pre-created string BPA block.
 * @param array_id Source string array; index selects the string.
 * @param block_id Destination string block whose current value is replaced.
 * @param index Zero-based position of the string to copy.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid resource types.
 */
int clm_bpaa_get_string(clm_id array_id, clm_int index, clm_id block_id);

/** Replaces an existing string array item with a string BPA block's value.
 * @param array_id Destination string array; index selects the item to replace.
 * @param block_id Source string block.
 * @param index Zero-based position of the existing string to replace.
 * @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid resource/index.
 */
int clm_bpaa_set_string(clm_id array_id, clm_int index, clm_id block_id);

/** Inserts a string BPA block's value at an index in a string array.
 * @param array_id Destination string array; index may equal its item count.
 * @param block_id Source string block; its value is copied independently.
 * @param index Zero-based insertion position, from zero through the current count.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/growth.
 */
int clm_bpaa_insert_string(clm_id array_id, clm_int index, clm_id block_id);

/** Appends a string BPA block's value to a string array.
 * @param array_id Destination string array.
 * @param block_id Source string block; its value is copied independently.
 * @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE for invalid input/growth.
 */
int clm_bpaa_append_string(clm_id array_id, clm_id block_id);

/** Draws a fixed-size item into a compatible BPA block.
 * @param array_id Non-string array; block_id is the destination block.
 * @param block_id Destination block that receives the drawn item.
 * @param expected_size Expected byte size of the array item and destination block.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if empty/incompatible.
 */
int clm_bpaa_draw(clm_id array_id, clm_id block_id, size_t expected_size);

/** Draws a selected active-deck item into a compatible BPA block.
 * @param array_id Non-string array; index selects a position in the current deck.
 * @param block_id Destination block; expected_size must match both item sizes.
 * @param index Zero-based position within the active draw deck.
 * @param expected_size Expected byte size of the array item and destination block.
 * @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE for invalid/exhausted input.
 */
int clm_bpaa_draw_at(clm_id array_id, clm_int index, clm_id block_id, size_t expected_size);


/* External memory access. Suffixes indicate transfer direction. */
int clm_bpa_get_to(clm_id id, void* destination, clm_uint size);
int clm_bpa_set_from(clm_id id, const void* source, clm_uint size);
int clm_bpa_get_string_to(clm_id id, void* destination, clm_uint size);
int clm_bpa_set_string_from(clm_id id, const void* source, clm_uint size);
int clm_bpaa_get_to(clm_id id, clm_int index, void* destination, clm_uint size);
int clm_bpaa_get_first_to(clm_id id, void* destination, clm_uint size);
int clm_bpaa_get_last_to(clm_id id, void* destination, clm_uint size);
int clm_bpaa_get_current_to(clm_id id, void* destination, clm_uint size);
int clm_bpaa_get_string_to(clm_id id, clm_int index, void* destination, clm_uint size);
int clm_bpaa_set_from(clm_id id, clm_int index, const void* source, clm_uint size);
int clm_bpaa_set_string_from(clm_id id, clm_int index, const void* source, clm_uint size);
int clm_bpaa_insert_from(clm_id id, clm_int index, const void* source, clm_uint size);
int clm_bpaa_append_from(clm_id id, const void* source, clm_uint size);
int clm_bpaa_draw_to(clm_id id, void* destination, clm_uint size);
int clm_bpaa_draw_at_to(clm_id id, clm_int index, void* destination, clm_uint size);

/* Managed metadata transfers validate destination types and sizes. */
int clm_bpaa_write_u32(clm_id destination_id, clm_uint value);
int clm_bpaa_write_i32(clm_id destination_id, clm_int value);
int clm_bpaa_write_byte(clm_id destination_id, clm_byte value);
int clm_bpaa_get_count(clm_id array_id, clm_id destination_id);
int clm_bpaa_get_capacity(clm_id array_id, clm_id destination_id);
int clm_bpaa_get_length(clm_id array_id, clm_id destination_id);
int clm_bpaa_get_current_index(clm_id array_id, clm_id destination_id);
int clm_bpaa_is_sealed(clm_id array_id, clm_id destination_id);
int clm_bpaa_get_first(clm_id array_id, clm_id destination_id, size_t expected_size);
int clm_bpaa_get_last(clm_id array_id, clm_id destination_id, size_t expected_size);
int clm_bpaa_get_current(clm_id array_id, clm_id destination_id, size_t expected_size);

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
int clm_bpa_shutdown(void);