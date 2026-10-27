/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 *
 * Public Domain — CC0 1.0 Universal
 * =============================================================================
 */

#pragma once

#include "dictionary.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <vector>

namespace clm::storage {

inline constexpr int CLM_BPA_FAILURE = dictionary::CLM_BPA_FAILURE;
inline constexpr int CLM_BPA_SUCCESS = dictionary::CLM_BPA_SUCCESS;
inline constexpr int CLM_BPA_GET_SUCCESS = dictionary::CLM_BPA_GET_SUCCESS;
inline constexpr int CLM_BPA_SET_SUCCESS = dictionary::CLM_BPA_SET_SUCCESS;
inline constexpr clm_byte CLM_BPA_TYPE_STRUCT = dictionary::CLM_BPA_TYPE_STRUCT;
inline constexpr clm_byte CLM_BPA_TYPE_SIGNED_BYTE = dictionary::CLM_BPA_TYPE_SIGNED_BYTE;
inline constexpr clm_byte CLM_BPA_TYPE_BYTE = dictionary::CLM_BPA_TYPE_BYTE;
inline constexpr clm_byte CLM_BPA_TYPE_SHORT = dictionary::CLM_BPA_TYPE_SHORT;
inline constexpr clm_byte CLM_BPA_TYPE_UNSIGNED_SHORT = dictionary::CLM_BPA_TYPE_UNSIGNED_SHORT;
inline constexpr clm_byte CLM_BPA_TYPE_INT = dictionary::CLM_BPA_TYPE_INT;
inline constexpr clm_byte CLM_BPA_TYPE_UNSIGNED_INT = dictionary::CLM_BPA_TYPE_UNSIGNED_INT;
inline constexpr clm_byte CLM_BPA_TYPE_LONG = dictionary::CLM_BPA_TYPE_LONG;
inline constexpr clm_byte CLM_BPA_TYPE_UNSIGNED_LONG = dictionary::CLM_BPA_TYPE_UNSIGNED_LONG;
inline constexpr clm_byte CLM_BPA_TYPE_FLOAT = dictionary::CLM_BPA_TYPE_FLOAT;
inline constexpr clm_byte CLM_BPA_TYPE_DOUBLE = dictionary::CLM_BPA_TYPE_DOUBLE;
inline constexpr clm_byte CLM_BPA_TYPE_STRING = dictionary::CLM_BPA_TYPE_STRING;
inline constexpr clm_byte CLM_BPA_TYPE_UUID = dictionary::CLM_BPA_TYPE_UUID;
inline constexpr clm_byte CLM_BPA_TYPE_DATETIME = dictionary::CLM_BPA_TYPE_DATETIME;

struct clm_storage_info {
    clm_uint columns = 0;
    clm_uint rows = 0;
    clm_uint layers = 0;
    clm_uint frame_columns = 0;
    clm_uint frame_rows = 0;
    clm_uint element_size = 0;
    clm_byte type = 0;
    clm_byte atlas = 0;
    clm_id data_id = 0; // Opaque backing-storage token; not a BPAA resource ID.
};

namespace detail {
struct storage_data {
    clm_storage_info info;
    std::vector<unsigned char> bytes;
};

inline std::unordered_map<clm_id, storage_data>& storages()
{
    static std::unordered_map<clm_id, storage_data> value;
    return value;
}

inline std::size_t type_size(const clm_byte type)
{
    return dictionary::detail::type_size(type);
}

inline bool cell_index(const clm_storage_info& info, const clm_uint layer,
                       const clm_uint row, const clm_uint column,
                       std::size_t& index)
{
    if (layer >= info.layers || row >= info.rows || column >= info.columns)
        return false;
    index = (static_cast<std::size_t>(layer) * info.rows + row) *
        info.columns + column;
    return true;
}

inline int create(const clm_id id, const clm_byte type, const std::size_t struct_size,
                  const clm_uint columns, const clm_uint rows, const clm_uint layers,
                  const clm_uint frame_columns, const clm_uint frame_rows,
                  const bool atlas)
{
    auto& resources = storages();
    if (resources.contains(id) || columns == 0 || rows == 0 ||
        layers == 0 || type == CLM_BPA_TYPE_STRING ||
        (atlas && (frame_columns == 0 || frame_rows == 0 ||
                   columns % frame_columns != 0 || rows % frame_rows != 0)) ||
        (!atlas && (frame_columns != 0 || frame_rows != 0)))
        return CLM_BPA_FAILURE;
    std::size_t width = type_size(type);
    if (type == CLM_BPA_TYPE_STRUCT) {
        if (struct_size == 0 ||
            struct_size > std::numeric_limits<clm_uint>::max())
            return CLM_BPA_FAILURE;
        width = struct_size;
    } else if (width == 0 || (struct_size != 0 && struct_size != width)) {
        return CLM_BPA_FAILURE;
    }

    const std::uint64_t plane = static_cast<std::uint64_t>(columns) * rows;
    if (plane > static_cast<std::uint64_t>(INT_MAX) / layers)
        return CLM_BPA_FAILURE;
    const std::uint64_t total = plane * layers;
    if (total == 0 ||
        total > std::numeric_limits<std::size_t>::max() / width)
        return CLM_BPA_FAILURE;

    clm_id internal_id = std::numeric_limits<clm_id>::max();
    bool internal_id_used = true;
    while (internal_id_used) {
        internal_id_used = internal_id == id ||
            resources.contains(internal_id);
        for (const auto& [info, bytes] : resources | std::views::values)
            internal_id_used = internal_id_used ||
                info.data_id == internal_id;
        if (internal_id_used)
            --internal_id;
    }
    try {
        storage_data data;
        data.info = {columns, rows, layers, frame_columns, frame_rows,
                     static_cast<clm_uint>(width), type,
                     static_cast<clm_byte>(atlas), internal_id};
        data.bytes.resize(static_cast<std::size_t>(total) * width, 0);
        resources.emplace(id, std::move(data));
    } catch (...) {
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SUCCESS;
}
}

inline int create(const clm_id id, const clm_byte type,
                           const std::size_t struct_size, const clm_uint columns,
                           const clm_uint rows, const clm_uint layers)
{
    return detail::create(id, type, struct_size, columns, rows, layers, 0, 0, false);
}

inline int create_2d(const clm_id id, const clm_byte type,
                              const std::size_t struct_size, const clm_uint columns,
                              const clm_uint rows)
{
    return create(id, type, struct_size, columns, rows, 1);
}

inline int atlas_create(const clm_id id, const clm_byte type,
                                 const std::size_t struct_size,
                                 const clm_uint columns, const clm_uint rows,
                                 const clm_uint frame_columns,
                                 const clm_uint frame_rows)
{
    return detail::create(id, type, struct_size, columns, rows, 1,
                          frame_columns, frame_rows, true);
}

inline int atlas_create_3d(const clm_id id, const clm_byte type,
                                    const std::size_t struct_size,
                                    const clm_uint columns, const clm_uint rows,
                                    const clm_uint layers,
                                    const clm_uint frame_columns,
                                    const clm_uint frame_rows)
{
    return detail::create(id, type, struct_size, columns, rows, layers,
                          frame_columns, frame_rows, true);
}

inline int get_info(const clm_id id, clm_storage_info* info)
{
    const auto found = detail::storages().find(id);
    if (found == detail::storages().end() || info == nullptr)
        return CLM_BPA_FAILURE;
    *info = found->second.info;
    return CLM_BPA_GET_SUCCESS;
}

inline int erase(const clm_id id)
{
    return detail::storages().erase(id) == 1
        ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

inline int get_to(const clm_id id, const clm_uint layer,
                              const clm_uint row, const clm_uint column,
                              void* buffer, const std::size_t buffer_size)
{
    const auto found = detail::storages().find(id);
    std::size_t index;
    if (found == detail::storages().end() || buffer == nullptr ||
        !detail::cell_index(found->second.info, layer, row, column, index) ||
        buffer_size < found->second.info.element_size)
        return CLM_BPA_FAILURE;
    std::memcpy(buffer, found->second.bytes.data() +
                index * found->second.info.element_size,
                found->second.info.element_size);
    return CLM_BPA_GET_SUCCESS;
}

inline int set_from(const clm_id id, const clm_uint layer,
                                const clm_uint row, const clm_uint column,
                                const void* value, const std::size_t value_size)
{
    const auto found = detail::storages().find(id);
    std::size_t index;
    if (found == detail::storages().end() || value == nullptr ||
        !detail::cell_index(found->second.info, layer, row, column, index) ||
        value_size != found->second.info.element_size)
        return CLM_BPA_FAILURE;
    std::memcpy(found->second.bytes.data() +
                index * found->second.info.element_size,
                value, value_size);
    return CLM_BPA_SET_SUCCESS;
}

inline int get(const clm_id id, const clm_uint layer,
                           const clm_uint row, const clm_uint column,
                           void* buffer, const std::size_t buffer_size)
{
    return get_to(id, layer, row, column, buffer, buffer_size);
}

inline int get(const clm_id id, const clm_uint layer,
                           const clm_uint row, const clm_uint column,
                           void* buffer)
{
    const auto found = detail::storages().find(id);
    if (found == detail::storages().end())
        return CLM_BPA_FAILURE;
    return get_to(id, layer, row, column, buffer,
                              found->second.info.element_size);
}

inline int set(const clm_id id, const clm_uint layer,
                           const clm_uint row, const clm_uint column,
                           const void* value, const std::size_t value_size)
{
    return set_from(id, layer, row, column, value, value_size);
}

inline int set(const clm_id id, const clm_uint layer,
                           const clm_uint row, const clm_uint column,
                           const void* value)
{
    const auto found = detail::storages().find(id);
    if (found == detail::storages().end())
        return CLM_BPA_FAILURE;
    return set_from(id, layer, row, column, value,
                                found->second.info.element_size);
}

inline int fill_from(const clm_id id, const void* value,
                                 const std::size_t value_size)
{
    const auto found = detail::storages().find(id);
    if (found == detail::storages().end() || value == nullptr ||
        value_size != found->second.info.element_size)
        return CLM_BPA_FAILURE;
    std::vector<unsigned char> snapshot;
    try {
        snapshot.resize(value_size);
    } catch (...) {
        return CLM_BPA_FAILURE;
    }
    std::memcpy(snapshot.data(), value, value_size);
    for (std::size_t offset = 0; offset < found->second.bytes.size();
         offset += value_size)
        std::memcpy(found->second.bytes.data() + offset, snapshot.data(), value_size);
    return CLM_BPA_SUCCESS;
}

inline int slice(const clm_id source_id, const clm_uint layer,
                             const clm_uint row, const clm_uint column,
                             const clm_uint layers, const clm_uint rows,
                             const clm_uint columns, const clm_id destination_id)
{
    auto source = detail::storages().find(source_id);
    if (source == detail::storages().end() || layers == 0 || rows == 0 ||
        columns == 0 || layer >= source->second.info.layers ||
        row >= source->second.info.rows || column >= source->second.info.columns ||
        layers > source->second.info.layers - layer ||
        rows > source->second.info.rows - row ||
        columns > source->second.info.columns - column)
        return CLM_BPA_FAILURE;
    const clm_storage_info source_info = source->second.info;
    // Re-find the source after destination insertion: unordered_map may rehash.
    if (create(destination_id, source_info.type,
                        source_info.element_size, columns, rows, layers) !=
        CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    auto destination = detail::storages().find(destination_id);
    const auto& source_bytes = detail::storages().at(source_id).bytes;
    const std::size_t width = source_info.element_size;
    for (clm_uint z = 0; z < layers; ++z) {
        for (clm_uint y = 0; y < rows; ++y) {
            std::size_t source_index;
            std::size_t destination_index;
            detail::cell_index(source_info, layer + z, row + y,
                               column, source_index);
            detail::cell_index(destination->second.info, z, y, 0,
                               destination_index);
            std::memcpy(destination->second.bytes.data() + destination_index * width,
                        source_bytes.data() + source_index * width,
                        static_cast<std::size_t>(columns) * width);
        }
    }
    return CLM_BPA_SUCCESS;
}

inline int atlas_get_frame_to(const clm_id id, const clm_uint layer,
                                         const clm_uint frame_row,
                                         const clm_uint frame_column,
                                         void* buffer,
                                         const std::size_t buffer_size)
{
    const auto found = detail::storages().find(id);
    if (found == detail::storages().end() || !found->second.info.atlas ||
        buffer == nullptr ||
        buffer_size < static_cast<std::size_t>(found->second.info.frame_columns) *
            found->second.info.frame_rows * found->second.info.element_size)
        return CLM_BPA_FAILURE;
    const auto& info = found->second.info;
    const clm_uint frame_count_x = info.columns / info.frame_columns;
    const clm_uint frame_count_y = info.rows / info.frame_rows;
    if (layer >= info.layers || frame_row >= frame_count_y ||
        frame_column >= frame_count_x)
        return CLM_BPA_FAILURE;
    auto* out = static_cast<unsigned char*>(buffer);
    const std::size_t row_size =
        static_cast<std::size_t>(info.frame_columns) * info.element_size;
    std::vector<unsigned char> snapshot;
    const std::size_t needed = row_size * info.frame_rows;
    try {
        snapshot.resize(needed);
    } catch (...) {
        return CLM_BPA_FAILURE;
    }
    for (clm_uint y = 0; y < info.frame_rows; ++y) {
        const clm_uint cell_row = frame_row * info.frame_rows + y;
        const clm_uint cell_column = frame_column * info.frame_columns;
        std::size_t index;
        detail::cell_index(info, layer, cell_row, cell_column, index);
        std::memcpy(snapshot.data() + static_cast<std::size_t>(y) * row_size,
                    found->second.bytes.data() + index * info.element_size,
                    row_size);
    }
    std::memcpy(out, snapshot.data(), needed);
    return CLM_BPA_GET_SUCCESS;
}

inline int atlas_set_frame_from(const clm_id id, const clm_uint layer,
                                            const clm_uint frame_row,
                                            const clm_uint frame_column,
                                            const void* value,
                                            const std::size_t value_size)
{
    const auto found = detail::storages().find(id);
    if (found == detail::storages().end() || !found->second.info.atlas ||
        value == nullptr ||
        value_size != static_cast<std::size_t>(found->second.info.frame_columns) *
            found->second.info.frame_rows * found->second.info.element_size)
        return CLM_BPA_FAILURE;
    const auto& info = found->second.info;
    const clm_uint frame_count_x = info.columns / info.frame_columns;
    const clm_uint frame_count_y = info.rows / info.frame_rows;
    if (layer >= info.layers || frame_row >= frame_count_y ||
        frame_column >= frame_count_x)
        return CLM_BPA_FAILURE;
    const auto* in = static_cast<const unsigned char*>(value);
    const std::size_t row_size =
        static_cast<std::size_t>(info.frame_columns) * info.element_size;
    std::vector<unsigned char> snapshot;
    try {
        snapshot.resize(value_size);
    } catch (...) {
        return CLM_BPA_FAILURE;
    }
    std::memcpy(snapshot.data(), in, value_size);
    for (clm_uint y = 0; y < info.frame_rows; ++y) {
        const clm_uint cell_row = frame_row * info.frame_rows + y;
        const clm_uint cell_column = frame_column * info.frame_columns;
        std::size_t index;
        detail::cell_index(info, layer, cell_row, cell_column, index);
        std::memcpy(found->second.bytes.data() + index * info.element_size,
                    snapshot.data() + static_cast<std::size_t>(y) * row_size,
                    row_size);
    }
    return CLM_BPA_SET_SUCCESS;
}

inline int atlas_slice_frame(const clm_id id, const clm_uint layer,
                                         const clm_uint frame_row,
                                         const clm_uint frame_column,
                                         const clm_id destination_id)
{
    const auto found = detail::storages().find(id);
    if (found == detail::storages().end() || !found->second.info.atlas)
        return CLM_BPA_FAILURE;
    const auto info = found->second.info;
    const clm_uint frame_count_x = info.columns / info.frame_columns;
    const clm_uint frame_count_y = info.rows / info.frame_rows;
    if (layer >= info.layers || frame_row >= frame_count_y ||
        frame_column >= frame_count_x ||
        create_2d(destination_id, info.type, info.element_size,
                           info.frame_columns, info.frame_rows) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    auto destination = detail::storages().find(destination_id);
    const auto& source_bytes = detail::storages().at(id).bytes;
    const std::size_t width = info.element_size;
    for (clm_uint y = 0; y < info.frame_rows; ++y) {
        std::size_t source_index;
        std::size_t destination_index;
        detail::cell_index(info, layer, frame_row * info.frame_rows + y,
                           frame_column * info.frame_columns, source_index);
        detail::cell_index(destination->second.info, 0, y, 0, destination_index);
        std::memcpy(destination->second.bytes.data() + destination_index * width,
                    source_bytes.data() + source_index * width,
                    static_cast<std::size_t>(info.frame_columns) * width);
    }
    return CLM_BPA_SUCCESS;
}

}
