/*
 * =============================================================================
 * CLM — Cross Language Modules
 * Public Domain — CC0 1.0 Universal
 * =============================================================================
 */

#pragma once

#include "test.hpp"
#include "../clm/storage.hpp"

#include <cstring>

static int test_clm_storage()
{
    clm_test_result result = {0};
    using namespace clm::storage;

    clm_int value = 0;
    clm_int replacement = 72;
    clm_storage_info info{};
    CLM_ASSERT(create_2d(100, CLM_BPA_TYPE_INT, 0, 4, 3) == CLM_BPA_SUCCESS,
               "create a 2D grid");
    CLM_ASSERT(create_2d(100, CLM_BPA_TYPE_INT, 0, 4, 3) == CLM_BPA_FAILURE,
               "reject duplicate storage IDs");
    CLM_ASSERT(get_to(100, 0, 2, 3, &value, sizeof(value)) == CLM_BPA_GET_SUCCESS &&
               value == 0,
               "read zero-initialized cells");
    CLM_ASSERT(set_from(100, 0, 1, 2, &replacement, sizeof(replacement)) ==
                   CLM_BPA_SET_SUCCESS,
               "write a cell from a sized buffer");
    CLM_ASSERT(get_to(100, 0, 1, 2, &value, sizeof(value)) ==
                   CLM_BPA_GET_SUCCESS && value == 72,
               "read back an updated cell");
    struct oversized_cell_buffer {
        clm_int cell;
        clm_int guard;
    } extra_cell_bytes{0, 1234};
    CLM_ASSERT(get_to(100, 0, 1, 2, &extra_cell_bytes,
                      sizeof(extra_cell_bytes)) == CLM_BPA_GET_SUCCESS &&
               extra_cell_bytes.cell == 72 && extra_cell_bytes.guard == 1234,
               "accept oversized read buffers without overwriting trailing bytes");
    CLM_ASSERT(get_to(100, 1, 1, 2, &value, sizeof(value)) == CLM_BPA_FAILURE,
               "reject an invalid layer");
    CLM_ASSERT(get_to(100, 0, 3, 2, &value, sizeof(value)) == CLM_BPA_FAILURE,
               "reject an invalid row");
    CLM_ASSERT(set_from(100, 0, 1, 2, &replacement, 2) == CLM_BPA_FAILURE,
               "reject an incorrect element size");
    CLM_ASSERT(get_to(100, 0, 1, 2, nullptr, sizeof(value)) == CLM_BPA_FAILURE,
               "reject a null read buffer");
    CLM_ASSERT(set_from(100, 0, 1, 2, nullptr, sizeof(replacement)) ==
                   CLM_BPA_FAILURE,
               "reject a null write buffer");
    CLM_ASSERT(get_info(100, &info) == CLM_BPA_GET_SUCCESS &&
               info.columns == 4 && info.rows == 3 && info.layers == 1 &&
               info.element_size == sizeof(clm_int) &&
               info.type == CLM_BPA_TYPE_INT && info.atlas == 0 &&
               info.data_id != 100,
               "report dimensions and element metadata");
    CLM_ASSERT(get_info(100, nullptr) == CLM_BPA_FAILURE,
               "reject a null metadata output");
    CLM_ASSERT(get_info(999, &info) == CLM_BPA_FAILURE,
               "reject metadata queries for unknown storage IDs");
    CLM_ASSERT(get_to(999, 0, 0, 0, &value, sizeof(value)) == CLM_BPA_FAILURE,
               "reject cell reads for unknown storage IDs");
    CLM_ASSERT(slice(100, 0, 1, 1, 1, 2, 2, 101) == CLM_BPA_SUCCESS,
               "copy a bounded storage slice");
    CLM_ASSERT(get_to(101, 0, 0, 1, &value, sizeof(value)) ==
                   CLM_BPA_GET_SUCCESS && value == 72,
               "preserve slice coordinates and cell values");
    CLM_ASSERT(slice(100, 0, 2, 2, 1, 2, 2, 102) == CLM_BPA_FAILURE,
               "reject a slice that exceeds source bounds");
    clm_storage_info slice_info{};
    CLM_ASSERT(get_info(101, &slice_info) == CLM_BPA_GET_SUCCESS &&
               slice_info.columns == 2 && slice_info.rows == 2 &&
               slice_info.layers == 1,
               "report dimensions of a copied slice");
    CLM_ASSERT(slice(100, 0, 1, 1, 1, 2, 2, 101) == CLM_BPA_FAILURE,
               "reject an already-used slice destination ID");
    CLM_ASSERT(slice(100, 0, 1, 1, 0, 2, 2, 102) == CLM_BPA_FAILURE,
               "reject a slice with zero layers");
    replacement = 21;
    CLM_ASSERT(set_from(101, 0, 0, 1, &replacement, sizeof(replacement)) == CLM_BPA_SET_SUCCESS &&
               get_to(100, 0, 1, 2, &value, sizeof(value)) == CLM_BPA_GET_SUCCESS && value == 72,
               "keep a slice independent from its source");
    CLM_ASSERT(erase(101) == CLM_BPA_SUCCESS &&
               erase(100) == CLM_BPA_SUCCESS &&
               get_info(100, &info) == CLM_BPA_FAILURE,
               "delete storage resources");

    constexpr clm_byte byte_value = 9;
    clm_byte read_byte = 0;
    CLM_ASSERT(create(110, CLM_BPA_TYPE_BYTE, 0, 2, 3, 2) == CLM_BPA_SUCCESS &&
               set_from(110, 1, 2, 1, &byte_value, 1) == CLM_BPA_SET_SUCCESS &&
               get_to(110, 0, 2, 1, &read_byte, 1) == CLM_BPA_GET_SUCCESS && read_byte == 0 &&
               set_from(110, 2, 2, 1, &byte_value, 1) == CLM_BPA_FAILURE &&
               get_to(110, 2, 2, 1, &read_byte, 1) == CLM_BPA_FAILURE &&
               get_to(110, 1, 2, 1, &read_byte, 1) == CLM_BPA_GET_SUCCESS && read_byte == 9 &&
               get_to(110, 1, 3, 1, &read_byte, 1) == CLM_BPA_FAILURE &&
               get_to(110, 1, 2, 2, &read_byte, 1) == CLM_BPA_FAILURE &&
               slice(110, 1, 1, 0, 1, 2, 2, 111) == CLM_BPA_SUCCESS &&
               get_to(111, 0, 1, 1, &read_byte, 1) == CLM_BPA_GET_SUCCESS && read_byte == 9,
               "address valid 3D cells, reject bounds, and slice one layer");
    CLM_ASSERT(erase(111) == CLM_BPA_SUCCESS &&
               erase(110) == CLM_BPA_SUCCESS,
               "release 3D resources");

    constexpr clm_byte frame[4] = {1, 2, 3, 4};
    clm_byte copied_frame[4] = {};
    CLM_ASSERT(atlas_create(120, CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2) == CLM_BPA_SUCCESS &&
               atlas_set_frame_from(120, 0, 1, 1, frame, 3) == CLM_BPA_FAILURE &&
               atlas_set_frame_from(120, 0, 1, 1, nullptr, sizeof(frame)) == CLM_BPA_FAILURE &&
               atlas_set_frame_from(120, 0, 1, 1, frame, 4) == CLM_BPA_SET_SUCCESS &&
               atlas_get_frame_to(120, 0, 1, 1, copied_frame, 4) == CLM_BPA_GET_SUCCESS &&
               std::memcmp(frame, copied_frame, sizeof(frame)) == 0,
               "validate input buffers and copy tightly packed atlas frames");
    CLM_ASSERT(atlas_get_frame_to(120, 0, 0, 0, copied_frame, 4) == CLM_BPA_GET_SUCCESS &&
               copied_frame[0] == 0 && copied_frame[3] == 0 &&
               atlas_get_frame_to(120, 0, 2, 0, copied_frame, 4) == CLM_BPA_FAILURE &&
               atlas_get_frame_to(120, 0, 1, 1, copied_frame, 3) == CLM_BPA_FAILURE,
               "zero-initialize atlas frames and validate frame bounds");
    clm_byte oversized_frame[6] = {0, 0, 0, 0, 0xaa, 0xbb};
    CLM_ASSERT(atlas_get_frame_to(120, 0, 1, 1, oversized_frame,
                                  sizeof(oversized_frame)) == CLM_BPA_GET_SUCCESS &&
               std::memcmp(frame, oversized_frame, sizeof(frame)) == 0 &&
               oversized_frame[4] == 0xaa && oversized_frame[5] == 0xbb,
               "accept oversized frame buffers without overwriting trailing bytes");
    CLM_ASSERT(get_info(120, &info) == CLM_BPA_GET_SUCCESS &&
               info.atlas == 1 && info.frame_columns == 2 &&
               info.frame_rows == 2 && info.columns == 4 &&
               info.rows == 4 && info.layers == 1,
               "report atlas dimensions and frame metadata");
    CLM_ASSERT(atlas_slice_frame(120, 0, 1, 1, 121) == CLM_BPA_SUCCESS &&
               get_to(121, 0, 1, 1, &read_byte, 1) == CLM_BPA_GET_SUCCESS && read_byte == 4 &&
               atlas_slice_frame(120, 0, 2, 1, 122) == CLM_BPA_FAILURE &&
               atlas_slice_frame(120, 0, 1, 1, 121) == CLM_BPA_FAILURE &&
               erase(121) == CLM_BPA_SUCCESS &&
               erase(120) == CLM_BPA_SUCCESS,
               "copy atlas frames and reject invalid or duplicate destinations");
    CLM_ASSERT(atlas_create(122, CLM_BPA_TYPE_BYTE, 0, 5, 4, 2, 2) == CLM_BPA_FAILURE &&
               create_2d(123, CLM_BPA_TYPE_STRING, 0, 2, 2) == CLM_BPA_FAILURE &&
               create_2d(123, CLM_BPA_TYPE_INT, 0, 0, 2) == CLM_BPA_FAILURE &&
               create_2d(123, CLM_BPA_TYPE_INT, sizeof(clm_int) + 1, 2, 2) == CLM_BPA_FAILURE &&
               create_2d(123, CLM_BPA_TYPE_STRUCT, 0, 2, 2) == CLM_BPA_FAILURE &&
               create(123, CLM_BPA_TYPE_INT, 0, 0xffffffffu, 0xffffffffu, 2) == CLM_BPA_FAILURE,
               "reject invalid types, element sizes, and dimensions");
    CLM_ASSERT(atlas_create_3d(130, CLM_BPA_TYPE_BYTE, 0, 4, 4, 2, 2, 2) == CLM_BPA_SUCCESS &&
               atlas_set_frame_from(130, 1, 0, 1, frame, sizeof(frame)) == CLM_BPA_SET_SUCCESS &&
               atlas_get_frame_to(130, 1, 0, 1, copied_frame, sizeof(copied_frame)) == CLM_BPA_GET_SUCCESS &&
               std::memcmp(frame, copied_frame, sizeof(frame)) == 0 &&
               atlas_get_frame_to(130, 2, 0, 1, copied_frame, sizeof(copied_frame)) == CLM_BPA_FAILURE &&
               atlas_get_frame_to(130, 1, 2, 0, copied_frame, sizeof(copied_frame)) == CLM_BPA_FAILURE &&
               get_to(130, 0, 0, 2, &read_byte, 1) == CLM_BPA_GET_SUCCESS && read_byte == 0 &&
               erase(130) == CLM_BPA_SUCCESS,
               "validate 3D atlas layer and frame bounds");

    value = 5;
    CLM_ASSERT(create_2d(140, CLM_BPA_TYPE_INT, 0, 2, 2) == CLM_BPA_SUCCESS &&
               fill_from(140, &value, 2) == CLM_BPA_FAILURE &&
               fill_from(140, &value, sizeof(value)) == CLM_BPA_SUCCESS &&
               get_to(140, 0, 1, 1, &replacement, sizeof(replacement)) == CLM_BPA_GET_SUCCESS && replacement == 5 &&
               get_to(140, 0, 0, 0, &replacement, sizeof(replacement)) == CLM_BPA_GET_SUCCESS && replacement == 5 &&
               erase(140) == CLM_BPA_SUCCESS,
               "reject invalid fill sizes and fill every cell with a fixed-width value");
    value = 83;
    CLM_ASSERT(create_2d(141, CLM_BPA_TYPE_INT, 0, 2, 2) == CLM_BPA_SUCCESS &&
               set(141, 0, 0, 0, &value) == CLM_BPA_SET_SUCCESS &&
               get(141, 0, 0, 0, &replacement) == CLM_BPA_GET_SUCCESS &&
               replacement == 83,
               "copy a cell through element-sized set and get overloads");
    value = 84;
    CLM_ASSERT(set(141, 0, 1, 1, &value, sizeof(value)) == CLM_BPA_SET_SUCCESS &&
               get(141, 0, 1, 1, &replacement, sizeof(replacement)) == CLM_BPA_GET_SUCCESS &&
               replacement == value &&
               set(141, 0, 1, 0, &value, 2) == CLM_BPA_FAILURE,
               "copy a cell through sized set and get overloads and validate width");
    CLM_ASSERT(get(141, 0, 1, 0, &replacement, sizeof(replacement)) == CLM_BPA_GET_SUCCESS &&
               replacement == 0 &&
               erase(141) == CLM_BPA_SUCCESS,
               "leave cells unchanged after rejected writes");

    clm_test_summary(result.passed, result.failed);
    return result.failed == 0 ? 0 : 1;
}
