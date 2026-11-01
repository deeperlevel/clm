#pragma once

#include "test.h"
#include "../clm/arena.h"

static int clm_arena_tests(void)
{
    clm_test_result result = {0};
    clm_test_begin("CLM - Arena");

    const clm_id arena_id = 91800;
    size_t first_offset = SIZE_MAX;
    size_t second_offset = SIZE_MAX;
    size_t used = 0;
    size_t capacity = 0;
    CLM_ASSERT(clm_arena_create(arena_id, 64) == CLM_ARENA_SUCCESS,
               "create an ID arena");
    CLM_ASSERT(clm_arena_create(arena_id, 64) == CLM_ARENA_ALREADY_EXISTS,
               "reject a duplicate arena ID");
    CLM_ASSERT(clm_arena_allocate(arena_id, 13, 1, &first_offset) == CLM_ARENA_SUCCESS &&
                   first_offset == 0,
               "allocate the first arena region");
    CLM_ASSERT(clm_arena_allocate(arena_id, 7, _Alignof(max_align_t), &second_offset) ==
                       CLM_ARENA_SUCCESS && second_offset % _Alignof(max_align_t) == 0 &&
                   second_offset >= 13,
               "align a later arena region");
    CLM_ASSERT(clm_arena_get_info(arena_id, &used, &capacity) == CLM_ARENA_SUCCESS &&
                   used == second_offset + 7 && capacity >= 64,
               "report arena size and capacity");

    clm_arena_borrowed borrowed = {0};
    CLM_ASSERT(clm_arena_borrow(arena_id, &borrowed) == CLM_ARENA_SUCCESS &&
                   borrowed.id == arena_id && borrowed.size == used &&
                   borrowed.capacity == capacity && borrowed.data != NULL &&
                   ((unsigned char*)borrowed.data)[first_offset] == 0 &&
                   clm_arena_allocate(arena_id, 1, 0, &used) == CLM_ARENA_BUSY &&
                   clm_arena_reserve(arena_id, capacity * 2) == CLM_ARENA_BUSY &&
                   clm_arena_reset(arena_id) == CLM_ARENA_BUSY &&
                   clm_arena_rewind(arena_id, 0) == CLM_ARENA_BUSY &&
                   clm_arena_destroy(arena_id) == CLM_ARENA_BUSY,
               "borrow direct storage and prevent invalidating mutations");

    memset((unsigned char*)borrowed.data + first_offset, 0x31, 13);
    memset((unsigned char*)borrowed.data + second_offset, 0x52, 7);
    clm_arena_borrowed copied_borrow = borrowed;
    CLM_ASSERT(clm_arena_return(&borrowed) == CLM_ARENA_SUCCESS &&
                   borrowed.data == NULL && clm_arena_return(&copied_borrow) == CLM_ARENA_INVALID_ARGUMENT &&
                   clm_arena_reserve(arena_id, capacity * 2) == CLM_ARENA_SUCCESS &&
                   clm_arena_get_info(arena_id, &used, &capacity) == CLM_ARENA_SUCCESS &&
                   capacity >= 128,
               "release a lease once and grow arena capacity");

    CLM_ASSERT(clm_arena_borrow(arena_id, &borrowed) == CLM_ARENA_SUCCESS &&
                   ((unsigned char*)borrowed.data)[first_offset] == 0x31 &&
                   ((unsigned char*)borrowed.data)[second_offset + 6] == 0x52 &&
                   clm_arena_return(&borrowed) == CLM_ARENA_SUCCESS &&
                   clm_arena_rewind(arena_id, second_offset) == CLM_ARENA_SUCCESS &&
                   clm_arena_get_info(arena_id, &used, &capacity) == CLM_ARENA_SUCCESS &&
                   used == second_offset && clm_arena_rewind(arena_id, used + 1) ==
                       CLM_ARENA_OUT_OF_RANGE &&
                   clm_arena_reset(arena_id) == CLM_ARENA_SUCCESS &&
                   clm_arena_get_info(arena_id, &used, &capacity) == CLM_ARENA_SUCCESS && used == 0,
               "preserve data across growth and support rewind and reset");

    CLM_ASSERT(clm_arena_allocate(arena_id, 4, 3, &first_offset) == CLM_ARENA_INVALID_ARGUMENT &&
                   clm_arena_allocate(arena_id, 4, _Alignof(max_align_t) * 2, &first_offset) ==
                       CLM_ARENA_INVALID_ARGUMENT &&
                   clm_arena_allocate(arena_id, 0, 0, &first_offset) == CLM_ARENA_INVALID_ARGUMENT &&
                   clm_arena_get_info(91801, &used, &capacity) == CLM_ARENA_NOT_FOUND &&
                   clm_arena_destroy(arena_id) == CLM_ARENA_SUCCESS &&
                   clm_arena_destroy(arena_id) == CLM_ARENA_NOT_FOUND,
               "reject invalid allocation parameters and cleanly destroy IDs");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0;
}