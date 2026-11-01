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

/* ID-addressed growable bump arena. Allocations are identified by stable byte
 * offsets; temporary borrowed views expose the contiguous backing storage.
 * The registry is thread-safe, but callers must synchronize writes through a
 * borrowed view themselves. Mutating/resizing/destroying a borrowed arena
 * returns BUSY so the view cannot be invalidated unexpectedly.
 */

#include "base.h"

#include <stddef.h>
#include <stdint.h>

#define CLM_ARENA_SUCCESS ((clm_int)0)
#define CLM_ARENA_FAILURE ((clm_int)-1)
#define CLM_ARENA_INVALID_ARGUMENT ((clm_int)-2)
#define CLM_ARENA_NOT_FOUND ((clm_int)-3)
#define CLM_ARENA_ALREADY_EXISTS ((clm_int)-4)
#define CLM_ARENA_NO_MEMORY ((clm_int)-5)
#define CLM_ARENA_BUSY ((clm_int)-6)
#define CLM_ARENA_OUT_OF_RANGE ((clm_int)-7)

#define CLM_ARENA_DEFAULT_CAPACITY ((size_t)4096)

typedef struct {
    clm_id id;
    size_t size;
    size_t capacity;
    uint64_t generation;
    void* data;
    void* _owner;
    uint64_t _token;
} clm_arena_borrowed;

/** Creates an empty arena. initial_capacity zero selects the default. */
clm_int clm_arena_create(clm_id id, size_t initial_capacity);

/** Destroys an arena; returns BUSY while any borrow is outstanding. */
clm_int clm_arena_destroy(clm_id id);

/** Allocates zeroed bytes and returns their stable offset from the arena base.
 * alignment zero selects max_align_t alignment; other alignments must be
 * powers of two no greater than max_align_t alignment. New allocations may
 * grow the arena unless it is borrowed.
 */
clm_int clm_arena_allocate(
    clm_id id, size_t size, size_t alignment, size_t* out_offset);

/** Grows capacity to at least requested_capacity without changing used size. */
clm_int clm_arena_reserve(clm_id id, size_t requested_capacity);

/** Resets used size to zero. Capacity is retained. */
clm_int clm_arena_reset(clm_id id);

/** Rewinds used size to offset, discarding later allocations. */
clm_int clm_arena_rewind(clm_id id, size_t offset);

/** Reads the current used size and capacity. Outputs are unchanged on failure. */
clm_int clm_arena_get_info(
    clm_id id, size_t* out_size, size_t* out_capacity);

/** Borrows the arena's contiguous storage. Do not copy the lease object.
 * The pointer remains valid until clm_arena_return; arena mutations and destroy
 * return BUSY during that interval. */
clm_int clm_arena_borrow(clm_id id, clm_arena_borrowed* out_borrow);

/** Releases a live lease and zeros it. A lease must be returned exactly once. */
clm_int clm_arena_return(clm_arena_borrowed* borrowed);