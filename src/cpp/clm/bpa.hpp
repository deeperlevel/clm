#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * Public Domain — CC0 1.0 Universal
 * =============================================================================
 */

#include "base.hpp"

#include <algorithm>
#include <cstddef>
#include <climits>
#include <cstdlib>
#include <functional>
#include <ranges>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define CLM_BPA_POOL_SLAB_SLOTS 32u
#define CLM_BPA_POOL_MAX_SLOT 4096u
#define CLM_BPA_POOL_CLASSES 10u

namespace clm::bpa {

inline constexpr int CLM_BPA_FAILURE = -1;
inline constexpr int CLM_BPA_SUCCESS = 0;
inline constexpr int CLM_BPA_GET_SUCCESS = 1;
inline constexpr int CLM_BPA_SET_SUCCESS = 2;

inline constexpr clm_byte CLM_BPA_TYPE_STRUCT = 0;
inline constexpr clm_byte CLM_BPA_TYPE_SIGNED_BYTE = 1;
inline constexpr clm_byte CLM_BPA_TYPE_BYTE = 2;
inline constexpr clm_byte CLM_BPA_TYPE_SHORT = 3;
inline constexpr clm_byte CLM_BPA_TYPE_UNSIGNED_SHORT = 4;
inline constexpr clm_byte CLM_BPA_TYPE_INT = 5;
inline constexpr clm_byte CLM_BPA_TYPE_UNSIGNED_INT = 6;
inline constexpr clm_byte CLM_BPA_TYPE_LONG = 7;
inline constexpr clm_byte CLM_BPA_TYPE_UNSIGNED_LONG = 8;
inline constexpr clm_byte CLM_BPA_TYPE_FLOAT = 9;
inline constexpr clm_byte CLM_BPA_TYPE_DOUBLE = 10;
inline constexpr clm_byte CLM_BPA_TYPE_STRING = 11;
inline constexpr clm_byte CLM_BPA_TYPE_UUID = 12;
inline constexpr clm_byte CLM_BPA_TYPE_DATETIME = 13;

using id = clm_id;
using byte = clm_byte;
using uint = clm_uint;
using ulong = clm_ulong;

using clm_bpaa_compare_fn = int (*)(const void*, const void*);
using clm_bpaa_predicate_fn = bool (*)(const void*, const void*);

struct clm_bpa_pool_slab { clm_bpa_pool_slab* next{}; void* memory{}; };
struct clm_bpa_pool_class {
    std::size_t slot_size{};
    void* free_list{};
    clm_bpa_pool_slab* slabs{};
    std::size_t active{};
    std::size_t capacity{};
};
struct bpa_byte_buffer : public std::vector<unsigned char> {
    using std::vector<unsigned char>::vector;
    bpa_byte_buffer() = default;
    bpa_byte_buffer(const std::vector<unsigned char>& v) : std::vector<unsigned char>(v) {}
    bpa_byte_buffer(std::vector<unsigned char>&& v) : std::vector<unsigned char>(std::move(v)) {}

    operator char*() { return reinterpret_cast<char*>(this->data()); }
    operator const char*() const { return reinterpret_cast<const char*>(this->data()); }
    operator void*() { return this->data(); }
    operator const void*() const { return this->data(); }
    operator unsigned char*() { return this->data(); }
    operator const unsigned char*() const { return this->data(); }
};

struct clm_bpa_block_node {
    clm_id id{};
    clm_byte type{};
    clm_ushort refcount{};
    clm_uint block_size{};
    clm_byte encoding{};
    clm_uint length{};
    bpa_byte_buffer data;
};
struct clm_bpaa_array_node {
    clm_id id{};
    clm_byte type{};
    clm_ushort refcount{};
    clm_uint count{};
    clm_uint capacity{};
    clm_uint block_size{};
    clm_byte encoding{};
    clm_uint index{};
    clm_byte sealed{};
    clm_uint deck_size{};
    std::vector<unsigned char> blocks;
    std::vector<std::string> strings;
};

namespace detail {
    inline std::unordered_map<clm_id, clm_bpa_block_node> blocks;
    inline std::unordered_map<clm_id, clm_bpaa_array_node> arrays;
    inline std::size_t pool_active[CLM_BPA_POOL_CLASSES]{};
    inline std::size_t pool_capacity[CLM_BPA_POOL_CLASSES]{};

    inline clm_bpa_block_node* find_block(const clm_id id)
    {
        const auto it = blocks.find(id);
        return it == blocks.end() ? nullptr : &it->second;
    }
    inline clm_bpaa_array_node* find_array(const clm_id id)
    {
        const auto it = arrays.find(id);
        return it == arrays.end() ? nullptr : &it->second;
    }

    inline std::size_t class_for(const std::size_t bytes)
    {
        if (bytes == 0 || bytes > CLM_BPA_POOL_MAX_SLOT)
            return CLM_BPA_POOL_CLASSES;
        std::size_t size = std::max(sizeof(void*), std::size_t{8});
        std::size_t index = 0;
        while (size < bytes && index + 1 < CLM_BPA_POOL_CLASSES) {
            size *= 2;
            ++index;
        }
        return size >= bytes ? index : CLM_BPA_POOL_CLASSES;
    }

    inline void track_pool_alloc(const std::size_t bytes)
    {
        const auto index = class_for(bytes);
        if (index < CLM_BPA_POOL_CLASSES) {
            ++pool_active[index];
            pool_capacity[index] = std::max(pool_capacity[index], pool_active[index]);
        }
    }

    inline void track_pool_free(const std::size_t bytes)
    {
        const auto index = class_for(bytes);
        if (index < CLM_BPA_POOL_CLASSES && pool_active[index] != 0)
            --pool_active[index];
    }

    inline bool copy_text(const void* input, const clm_uint size, std::string& result)
    {
        if (input == nullptr || size == 0)
            return false;
        const auto* text = static_cast<const char*>(input);
        const auto* end = static_cast<const char*>(std::memchr(text, '\0', size));
        result.assign(text, end == nullptr ? size : static_cast<std::size_t>(end - text));
        return result.size() <= static_cast<std::size_t>(std::numeric_limits<clm_uint>::max());
    }

    inline bool valid_index(const clm_bpaa_array_node& array, const clm_int index)
    {
        return index >= 0 && static_cast<clm_uint>(index) < array.count;
    }

    inline bool resize_capacity(clm_bpaa_array_node& array, const clm_uint capacity)
    {
        if (capacity < array.count)
            return false;
        try {
            if (array.type == CLM_BPA_TYPE_STRING)
                array.strings.reserve(capacity);
            else
                array.blocks.resize(static_cast<std::size_t>(capacity) * array.block_size);
            array.capacity = capacity;
            return true;
        } catch (...) {
            return false;
        }
    }

    inline bool append_bytes(clm_bpaa_array_node& array, const void* data)
    {
        if (data == nullptr || array.type == CLM_BPA_TYPE_STRING)
            return false;
        try {
            if (array.count == array.capacity) {
                const clm_uint next = array.capacity == 0 ? 1 :
                    (array.capacity > std::numeric_limits<clm_uint>::max() / 2
                        ? std::numeric_limits<clm_uint>::max()
                        : array.capacity * 2);
                if (next <= array.capacity || !resize_capacity(array, next))
                    return false;
            }
            std::memcpy(array.blocks.data() +
                            static_cast<std::size_t>(array.count) * array.block_size,
                        data, array.block_size);
            ++array.count;
            array.deck_size = array.count;
            return true;
        } catch (...) {
            return false;
        }
    }
    } // namespace detail

    static inline std::size_t pool_class_for(const std::size_t bytes)
    {
        return detail::class_for(bytes);
    }

    static inline void* pool_alloc(const std::size_t bytes)
    {
        if (bytes == 0)
            return nullptr;
        const auto index = detail::class_for(bytes);
        try {
            void* memory = std::calloc(1, bytes);
            if (memory != nullptr && index < CLM_BPA_POOL_CLASSES) {
                ++detail::pool_active[index];
                detail::pool_capacity[index] =
                    std::max(detail::pool_capacity[index], detail::pool_active[index]);
            }
            return memory;
        } catch (...) {
            return nullptr;
        }
    }

    static inline void pool_free(void* pointer, const std::size_t bytes)
    {
        if (pointer == nullptr)
            return;
        const auto index = detail::class_for(bytes);
        if (index < CLM_BPA_POOL_CLASSES && detail::pool_active[index] != 0)
            --detail::pool_active[index];
        std::free(pointer);
    }

    static inline int pool_cleanup()
    {
        for (const auto active : detail::pool_active)
            if (active != 0)
                return CLM_BPA_FAILURE;
        std::ranges::fill(detail::pool_capacity, 0);
        return CLM_BPA_SUCCESS;
    }

    static inline int pool_stats(const std::size_t bytes, std::size_t* active, std::size_t* capacity)
    {
        const auto index = detail::class_for(bytes);
        if (index == CLM_BPA_POOL_CLASSES || active == nullptr || capacity == nullptr)
            return CLM_BPA_FAILURE;
        *active = detail::pool_active[index];
        *capacity = detail::pool_capacity[index];
        return CLM_BPA_SUCCESS;
    }

    static inline std::size_t hash_id(const clm_id id)
    {
        return std::hash<clm_id>{}(id);
    }
    static inline clm_bpa_block_node* find_block(const clm_id id)
    {
        return detail::find_block(id);
    }
    static inline clm_bpaa_array_node* find_array(const clm_id id)
    {
        return detail::find_array(id);
    }
    static inline int rehash_blocks(const std::size_t capacity)
    {
        try { detail::blocks.rehash(capacity); return CLM_BPA_SUCCESS; }
        catch (...) { return CLM_BPA_FAILURE; }
    }
    static inline int rehash_arrays(const std::size_t capacity)
    {
        try { detail::arrays.rehash(capacity); return CLM_BPA_SUCCESS; }
        catch (...) { return CLM_BPA_FAILURE; }
    }
    static inline int prepare_blocks() { return CLM_BPA_SUCCESS; }
    static inline int prepare_arrays() { return CLM_BPA_SUCCESS; }

    static inline clm_uint type_size(const clm_byte type)
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
        case CLM_BPA_TYPE_DATETIME: return sizeof(clm_long);
        default: return 0;
        }
    }

    static inline bool block_exists(const clm_id id)
    {
        return detail::find_block(id) != nullptr;
    }
    static inline bool array_exists(const clm_id id)
    {
        return detail::find_array(id) != nullptr;
    }

    static inline int create_sized(clm_id id, const clm_byte type, const clm_uint size)
    {
        if (block_exists(id))
            return CLM_BPA_FAILURE;
        const clm_uint width = type == CLM_BPA_TYPE_STRUCT ? size : type_size(type);
        if ((type == CLM_BPA_TYPE_STRUCT && size == 0) ||
            (type == CLM_BPA_TYPE_STRING && size != 0) ||
            (type != CLM_BPA_TYPE_STRUCT && type != CLM_BPA_TYPE_STRING &&
            (width == 0 || (size != 0 && size != width))))
            return CLM_BPA_FAILURE;
        try {
            clm_bpa_block_node block;
            block.id = id;
            block.type = type;
            block.refcount = 1;
            block.block_size = width;
            if (type != CLM_BPA_TYPE_STRING) {
                block.data.resize(width);
                detail::track_pool_alloc(width);
            }
            detail::blocks.emplace(id, std::move(block));
            return CLM_BPA_SUCCESS;
        } catch (...) { return CLM_BPA_FAILURE; }
    }
    static inline int create(const clm_id id, const clm_byte type)
    {
        return type == CLM_BPA_TYPE_STRUCT ? CLM_BPA_FAILURE :
            create_sized(id, type, 0);
    }
    static inline int erase_block(const clm_id id)
    {
        const auto* block = detail::find_block(id);
        if (block == nullptr) return CLM_BPA_FAILURE;
        if (block->type == CLM_BPA_TYPE_STRING) {
            if (!block->data.empty()) detail::track_pool_free(block->length + 1);
        }
        else
            detail::track_pool_free(block->block_size);
        detail::blocks.erase(id);
        return CLM_BPA_SUCCESS;
    }
    static inline int erase_array(clm_id id);
    static inline int remove_block(const clm_id id) { return erase_block(id); }
    static inline int remove_array(const clm_id id) { return erase_array(id); }
    static inline void insert_block(clm_bpa_block_node* block)
    {
        if (block != nullptr && !block_exists(block->id))
            detail::blocks.emplace(block->id, *block);
    }
    static inline void insert_array(clm_bpaa_array_node* array)
    {
        if (array != nullptr && !array_exists(array->id))
            detail::arrays.emplace(array->id, *array);
    }
    static inline int ref_block(const clm_id id)
    {
        auto* block = detail::find_block(id);
        if (block == nullptr || block->refcount == std::numeric_limits<clm_ushort>::max())
            return CLM_BPA_FAILURE;
        ++block->refcount;
        return CLM_BPA_SUCCESS;
    }
    static inline int unref_block(const clm_id id)
    {
        auto* block = detail::find_block(id);
        if (block == nullptr || block->refcount == 0)
            return CLM_BPA_FAILURE;
        return --block->refcount == 0 ? erase_block(id) : CLM_BPA_SUCCESS;
    }
    static inline int get(const clm_id id, void* output, const clm_uint size)
    {
        auto* block = detail::find_block(id);
        if (block == nullptr || block->type == CLM_BPA_TYPE_STRING ||
            output == nullptr || size < block->block_size)
            return CLM_BPA_FAILURE;
        std::memcpy(output, block->data.data(), block->block_size);
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int set(const clm_id id, const void* input, const clm_uint size)
    {
        auto* block = detail::find_block(id);
        if (block == nullptr || block->type == CLM_BPA_TYPE_STRING ||
            input == nullptr || size != block->block_size)
            return CLM_BPA_FAILURE;
        std::memmove(block->data.data(), input, block->block_size);
        return CLM_BPA_SET_SUCCESS;
    }
    static inline int get_string(const clm_id id, void* output, const clm_uint size)
    {
        auto* block = detail::find_block(id);
        if (block == nullptr || block->type != CLM_BPA_TYPE_STRING || output == nullptr ||
            size < block->length + 1 || block->length > INT_MAX)
            return CLM_BPA_FAILURE;
        if (block->length != 0)
            std::memcpy(output, block->data.data(), block->length);
        static_cast<char*>(output)[block->length] = '\0';
        return static_cast<int>(block->length);
    }
    static inline int set_string(const clm_id id, const void* input, const clm_uint size)
    {
        auto* block = detail::find_block(id);
        std::string text;
        if (block == nullptr || block->type != CLM_BPA_TYPE_STRING ||
            !detail::copy_text(input, size, text) || text.size() > INT_MAX)
            return CLM_BPA_FAILURE;
        if (!block->data.empty())
            detail::track_pool_free(block->length + 1);
        block->data.assign(text.begin(), text.end());
        block->data.push_back(0);
        detail::track_pool_alloc(text.size() + 1);
        block->length = static_cast<clm_uint>(text.size());
        return static_cast<int>(block->length);
    }
    static inline int replace(const clm_id id, const void* input, const clm_uint size)
    {
        const auto* block = detail::find_block(id);
        if (block == nullptr) return CLM_BPA_FAILURE;
        return block->type == CLM_BPA_TYPE_STRING
            ? set_string(id, input, size) : set(id, input, size);
    }
    static inline int copy_string(const void* input, const clm_uint size, char** output, clm_uint* length)
    {
        std::string text;
        if (output == nullptr || length == nullptr ||
            !detail::copy_text(input, size, text) ||
            text.size() > std::numeric_limits<clm_uint>::max())
            return CLM_BPA_FAILURE;
        try {
            auto* copy = static_cast<char*>(pool_alloc(text.size() + 1));
            if (copy == nullptr) return CLM_BPA_FAILURE;
            std::memcpy(copy, text.c_str(), text.size() + 1);
            *output = copy;
            *length = static_cast<clm_uint>(text.size());
            return CLM_BPA_SUCCESS;
        } catch (...) { return CLM_BPA_FAILURE; }
    }

    static inline int create_sized(clm_id id, clm_byte type,
                                            clm_int reserved, clm_uint size,
                                            clm_byte encoding)
    {
        if (array_exists(id) || reserved < 0)
            return CLM_BPA_FAILURE;
        const auto width = type == CLM_BPA_TYPE_STRUCT ? size : type_size(type);
        if ((type == CLM_BPA_TYPE_STRUCT && size == 0) ||
            (type == CLM_BPA_TYPE_STRING && size != 0) ||
            (type != CLM_BPA_TYPE_STRUCT && type != CLM_BPA_TYPE_STRING &&
            (width == 0 || (size != 0 && size != width))))
            return CLM_BPA_FAILURE;
        try {
            clm_bpaa_array_node array;
            array.id = id;
            array.type = type;
            array.refcount = 1;
            array.block_size = width;
            array.encoding = encoding;
            array.capacity = static_cast<clm_uint>(reserved);
            if (type == CLM_BPA_TYPE_STRING)
                array.strings.reserve(array.capacity);
            else
                array.blocks.resize(static_cast<std::size_t>(array.capacity) * width);
            detail::arrays.emplace(id, std::move(array));
            return CLM_BPA_SUCCESS;
        } catch (...) { return CLM_BPA_FAILURE; }
    }
    static inline int create(clm_id id, clm_byte type,
                                    clm_int reserved, clm_byte encoding)
    {
        return type == CLM_BPA_TYPE_STRUCT ? CLM_BPA_FAILURE :
            create_sized(id, type, reserved, 0, encoding);
    }
    static inline int erase_array(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr) return CLM_BPA_FAILURE;
        if (array->type == CLM_BPA_TYPE_STRING)
            for (const auto& text : array->strings)
                detail::track_pool_free(text.size() + 1);
        detail::arrays.erase(id);
        return CLM_BPA_SUCCESS;
    }
    static inline int ref_array(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->refcount == std::numeric_limits<clm_ushort>::max())
            return CLM_BPA_FAILURE;
        ++array->refcount;
        return CLM_BPA_SUCCESS;
    }
    static inline int unref_array(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->refcount == 0) return CLM_BPA_FAILURE;
        return --array->refcount == 0 ? erase_array(id) : CLM_BPA_SUCCESS;
    }
    static inline int get_count_to(clm_id id, clm_uint* output)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || output == nullptr) return CLM_BPA_FAILURE;
        *output = array->count;
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int get_length_to(clm_id id, clm_uint* output)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || output == nullptr) return CLM_BPA_FAILURE;
        if (array->type == CLM_BPA_TYPE_STRING) {
            if (array->index >= array->strings.size() ||
                array->strings[array->index].size() >
                    std::numeric_limits<clm_uint>::max())
                return CLM_BPA_FAILURE;
            *output = static_cast<clm_uint>(array->strings[array->index].size());
        } else {
            *output = array->count * array->block_size;
        }
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int get_capacity_to(clm_id id, clm_uint* output)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || output == nullptr) return CLM_BPA_FAILURE;
        *output = array->capacity;
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int resize(clm_id id, clm_uint capacity)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed ||
            !detail::resize_capacity(*array, capacity)) return CLM_BPA_FAILURE;
        return CLM_BPA_SUCCESS;
    }
    static inline int resize(clm_bpaa_array_node* array, clm_uint capacity)
    {
        return array == nullptr || !detail::resize_capacity(*array, capacity)
            ? CLM_BPA_FAILURE : CLM_BPA_SUCCESS;
    }
    static inline int ensure_capacity(clm_id id, clm_uint capacity)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr) return CLM_BPA_FAILURE;
        return capacity <= array->capacity ? CLM_BPA_SUCCESS :
            resize(id, capacity);
    }
    static inline int ensure_capacity(clm_bpaa_array_node* array,
                                            clm_uint capacity)
    {
        if (array == nullptr) return CLM_BPA_FAILURE;
        return capacity <= array->capacity ? CLM_BPA_SUCCESS :
            resize(array, capacity);
    }
    static inline int extend(clm_id id, clm_uint amount)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || amount > std::numeric_limits<clm_uint>::max() - array->capacity)
            return CLM_BPA_FAILURE;
        return resize(id, array->capacity + amount);
    }
    static inline int shrink(clm_id id, clm_uint new_capacity)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed ||
            new_capacity >= array->capacity || new_capacity < array->count)
            return CLM_BPA_FAILURE;
        return resize(id, new_capacity);
    }
    static inline int get_raw(clm_id id, clm_int index, void* output,
                                    clm_uint size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || !detail::valid_index(*array, index) ||
            array->type == CLM_BPA_TYPE_STRING || output == nullptr ||
            size < array->block_size) return CLM_BPA_FAILURE;
        std::memcpy(output, array->blocks.data() +
                        static_cast<std::size_t>(index) * array->block_size,
                    array->block_size);
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int get_string_raw(clm_id id, clm_int index,
                                            void* output, clm_uint size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || !detail::valid_index(*array, index) ||
            array->type != CLM_BPA_TYPE_STRING || output == nullptr)
            return CLM_BPA_FAILURE;
        const auto& text = array->strings[static_cast<std::size_t>(index)];
        if (text.size() + 1 > size || text.size() > INT_MAX) return CLM_BPA_FAILURE;
        std::memcpy(output, text.c_str(), text.size() + 1);
        return static_cast<int>(text.size());
    }
    static inline int set_string_raw(clm_id id, clm_int index,
                                            const void* input, clm_uint size)
    {
        auto* array = detail::find_array(id);
        std::string text;
        if (array == nullptr || array->sealed ||
            !detail::valid_index(*array, index) || array->type != CLM_BPA_TYPE_STRING ||
            !detail::copy_text(input, size, text) || text.size() > INT_MAX)
            return CLM_BPA_FAILURE;
        auto& value = array->strings[static_cast<std::size_t>(index)];
        detail::track_pool_free(value.size() + 1);
        value = std::move(text);
        detail::track_pool_alloc(value.size() + 1);
        array->deck_size = array->count;
        return static_cast<int>(array->strings[static_cast<std::size_t>(index)].size());
    }
    static inline int get_first_raw(clm_id id, void* output, clm_uint size)
    {
        return get_raw(id, 0, output, size);
    }
    static inline int get_last_raw(clm_id id, void* output, clm_uint size)
    {
        auto* array = detail::find_array(id);
        return array == nullptr || array->count == 0 ? CLM_BPA_FAILURE :
            get_raw(id, static_cast<clm_int>(array->count - 1), output, size);
    }
    static inline int get_current_raw(clm_id id, void* output, clm_uint size)
    {
        auto* array = detail::find_array(id);
        return array == nullptr ? CLM_BPA_FAILURE :
            get_raw(id, static_cast<clm_int>(array->index), output, size);
    }
    static inline int set_raw(clm_id id, clm_int index,
                                    const void* input, clm_uint size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed ||
            !detail::valid_index(*array, index) || array->type == CLM_BPA_TYPE_STRING ||
            input == nullptr || size != array->block_size) return CLM_BPA_FAILURE;
        std::memmove(array->blocks.data() +
                        static_cast<std::size_t>(index) * array->block_size,
                    input, array->block_size);
        array->deck_size = array->count;
        return CLM_BPA_SET_SUCCESS;
    }
    static inline int insert_raw(clm_id id, clm_int index,
                                        const void* input, clm_uint size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed || index < 0 ||
            static_cast<clm_uint>(index) > array->count ||
            array->type == CLM_BPA_TYPE_STRING || input == nullptr ||
            size != array->block_size) return CLM_BPA_FAILURE;
        try {
            std::vector<unsigned char> input_copy(
                static_cast<const unsigned char*>(input),
                static_cast<const unsigned char*>(input) + array->block_size);
            if (array->count == array->capacity &&
                (array->capacity > std::numeric_limits<clm_uint>::max() / 2u ||
                 !detail::resize_capacity(*array, array->capacity == 0 ? 1u :
                    (array->capacity > std::numeric_limits<clm_uint>::max() / 2u ? 0u : array->capacity * 2u))))
                return CLM_BPA_FAILURE;
            auto* destination = array->blocks.data() +
                static_cast<std::size_t>(index) * array->block_size;
            std::memmove(destination + array->block_size, destination,
                static_cast<std::size_t>(array->count - index) * array->block_size);
            std::memcpy(destination, input_copy.data(), array->block_size);
            ++array->count;
            array->deck_size = array->count;
            return CLM_BPA_SUCCESS;
        } catch (...) { return CLM_BPA_FAILURE; }
    }
    static inline int append_raw(clm_id id, const void* input, clm_uint size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || size != array->block_size ||
            (array->sealed && array->count == array->capacity))
            return CLM_BPA_FAILURE;
        return detail::append_bytes(*array, input) ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
    }
    static inline int append_string_raw(clm_id id, const void* input,
                                                clm_uint size)
    {
        auto* array = detail::find_array(id);
        std::string text;
        if (array == nullptr || array->sealed || array->type != CLM_BPA_TYPE_STRING ||
            !detail::copy_text(input, size, text)) return CLM_BPA_FAILURE;
        try {
            array->strings.push_back(std::move(text));
            detail::track_pool_alloc(array->strings.back().size() + 1);
            array->count = static_cast<clm_uint>(array->strings.size());
            array->capacity = std::max(array->capacity, array->count);
            array->deck_size = array->count;
            return CLM_BPA_SUCCESS;
        } catch (...) { return CLM_BPA_FAILURE; }
    }
    static inline int remove(clm_id id, clm_int index)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed || !detail::valid_index(*array, index))
            return CLM_BPA_FAILURE;
        if (array->type == CLM_BPA_TYPE_STRING)
        {
            detail::track_pool_free(array->strings[static_cast<std::size_t>(index)].size() + 1);
            array->strings.erase(array->strings.begin() + index);
        }
        else {
            auto* destination = array->blocks.data() +
                static_cast<std::size_t>(index) * array->block_size;
            std::memmove(destination, destination + array->block_size,
                static_cast<std::size_t>(array->count - index - 1) * array->block_size);
            std::memset(array->blocks.data() +
                            static_cast<std::size_t>(array->count - 1) * array->block_size,
                        0, array->block_size);
        }
        --array->count;
        array->deck_size = array->count;
        array->index = array->count == 0 ? 0 :
            std::min(array->index, array->count - 1);
        return CLM_BPA_SUCCESS;
    }
    static inline int remove_first(clm_id id) { return remove(id, 0); }
    static inline int remove_last(clm_id id)
    {
        auto* array = detail::find_array(id);
        return array == nullptr || array->count == 0 ? CLM_BPA_FAILURE :
            remove(id, static_cast<clm_int>(array->count - 1));
    }
    static inline int clear(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr) return CLM_BPA_FAILURE;
        array->count = array->index = 0;
        array->deck_size = 0;
        array->sealed = 0;
        for (const auto& text : array->strings)
            detail::track_pool_free(text.size() + 1);
        array->strings.clear();
        if (array->type != CLM_BPA_TYPE_STRING)
            std::ranges::fill(array->blocks, 0);
        return CLM_BPA_SUCCESS;
    }

    static inline int reset(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr) return CLM_BPA_FAILURE;
        array->index = 0;
        return CLM_BPA_SUCCESS;
    }
    static inline int move_first(clm_id id) { return reset(id); }
    static inline int move_last(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->count == 0) return CLM_BPA_FAILURE;
        array->index = array->count - 1;
        return CLM_BPA_SUCCESS;
    }
    static inline int move_previous(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->index == 0) return CLM_BPA_FAILURE;
        --array->index;
        return CLM_BPA_SUCCESS;
    }
    static inline int move_next(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->index + 1 >= array->count) return CLM_BPA_FAILURE;
        ++array->index;
        return CLM_BPA_SUCCESS;
    }
    static inline int move_to(clm_id id, clm_uint index)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || index >= array->count) return CLM_BPA_FAILURE;
        array->index = index;
        return CLM_BPA_SUCCESS;
    }
    static inline int get_current_index_to(clm_id id, clm_uint* output)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || output == nullptr) return CLM_BPA_FAILURE;
        *output = array->index;
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int seal(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr) return CLM_BPA_FAILURE;
        array->sealed = 1;
        return CLM_BPA_SUCCESS;
    }
    static inline int is_sealed_to(clm_id id, clm_byte* output)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || output == nullptr) return CLM_BPA_FAILURE;
        *output = array->sealed;
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int unseal(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr) return CLM_BPA_FAILURE;
        array->sealed = 0;
        return CLM_BPA_SUCCESS;
    }

    static inline int sort_with(clm_id id, clm_bpaa_compare_fn compare)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed || array->type == CLM_BPA_TYPE_STRING ||
            compare == nullptr) return CLM_BPA_FAILURE;
        const auto width = array->block_size;
        try {
            std::vector<clm_uint> order(array->count);
            std::iota(order.begin(), order.end(), 0);
            std::ranges::stable_sort(order, [&](const clm_uint left, const clm_uint right) {
                return compare(array->blocks.data() +
                                static_cast<std::size_t>(left) * width,
                            array->blocks.data() +
                                static_cast<std::size_t>(right) * width) < 0;
            });
            std::vector<unsigned char> sorted(
                static_cast<std::size_t>(array->count) * width);
            for (clm_uint i = 0; i < array->count; ++i)
                std::memcpy(sorted.data() + static_cast<std::size_t>(i) * width,
                            array->blocks.data() +
                                static_cast<std::size_t>(order[i]) * width,
                            width);
            if (!sorted.empty())
                std::memcpy(array->blocks.data(), sorted.data(), sorted.size());
            array->index = 0;
            array->deck_size = array->count;
            return CLM_BPA_SUCCESS;
        } catch (...) { return CLM_BPA_FAILURE; }
    }
    static inline int sort(clm_id id, clm_bpaa_predicate_fn predicate)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed || array->type == CLM_BPA_TYPE_STRING ||
            predicate == nullptr) return CLM_BPA_FAILURE;
        const auto width = array->block_size;
        std::vector<unsigned char> temporary;
        try { temporary.resize(width); }
        catch (...) { return CLM_BPA_FAILURE; }
        for (clm_uint i = 0; i + 1 < array->count; ++i) {
            bool swapped = false;
            for (clm_uint j = 0; j + 1 < array->count - i; ++j) {
                auto* left = array->blocks.data() +
                    static_cast<std::size_t>(j) * width;
                auto* right = left + width;
                if (predicate(left, right)) {
                    std::memcpy(temporary.data(), left, width);
                    std::memcpy(left, right, width);
                    std::memcpy(right, temporary.data(), width);
                    swapped = true;
                }
            }
            if (!swapped) break;
        }
        array->index = 0;
        array->deck_size = array->count;
        return CLM_BPA_SUCCESS;
    }
    static inline int reverse(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed) return CLM_BPA_FAILURE;
        if (array->type == CLM_BPA_TYPE_STRING)
            std::ranges::reverse(array->strings);
        else {
            for (clm_uint left = 0, right = array->count; left < right / 2; ++left) {
                --right;
                for (clm_uint byte = 0; byte < array->block_size; ++byte)
                    std::swap(array->blocks[left * array->block_size + byte],
                            array->blocks[right * array->block_size + byte]);
            }
        }
        return CLM_BPA_SUCCESS;
    }
    static inline int rotate(clm_id id, clm_int amount)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed || array->count == 0) return CLM_BPA_FAILURE;
        const auto shift = ((amount % static_cast<clm_int>(array->count)) +
                            static_cast<clm_int>(array->count)) % array->count;
        if (array->type == CLM_BPA_TYPE_STRING)
            std::ranges::rotate(array->strings, array->strings.end() - shift);
        else {
            const auto n = static_cast<std::size_t>(array->count) * array->block_size;
            std::rotate(array->blocks.begin(),
                        array->blocks.begin() + (array->count - shift) * array->block_size,
                        array->blocks.begin() + n); // NOLINT(*-narrowing-conversions)
        }
        return CLM_BPA_SUCCESS;
    }
    static inline int rotate(clm_bpaa_array_node* array, clm_uint start,
                                    clm_uint positions, int right)
    {
        if (array == nullptr || array->type == CLM_BPA_TYPE_STRING ||
            start >= array->count || array->sealed) return CLM_BPA_FAILURE;
        const clm_uint length = array->count - start;
        const clm_uint shift = positions % length;
        if (shift == 0) return CLM_BPA_SUCCESS;
        auto begin = array->blocks.begin() +
            static_cast<std::ptrdiff_t>(start) * array->block_size;
        auto end = begin + static_cast<std::ptrdiff_t>(length) * array->block_size;
        auto pivot = right
            ? end - static_cast<std::ptrdiff_t>(shift) * array->block_size
            : begin + static_cast<std::ptrdiff_t>(shift) * array->block_size;
        std::rotate(begin, pivot, end);
        array->index = 0;
        array->deck_size = array->count;
        return CLM_BPA_SUCCESS;
    }
    static inline int shift_left(clm_id id, clm_uint amount)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || amount > array->count) return CLM_BPA_FAILURE;
        for (clm_uint i = 0; i < amount; ++i)
            if (remove_first(id) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
        return CLM_BPA_SUCCESS;
    }
    static inline int shift_left(clm_id id, clm_uint start,
                                        clm_uint positions)
    {
        auto* array = detail::find_array(id);
        return array == nullptr ? CLM_BPA_FAILURE :
            rotate(array, start, positions, 0);
    }
    static inline int shift_right(clm_id id, clm_uint amount)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || amount > array->count) return CLM_BPA_FAILURE;
        for (clm_uint i = 0; i < amount; ++i)
            if (remove_last(id) != CLM_BPA_SUCCESS) return CLM_BPA_FAILURE;
        return CLM_BPA_SUCCESS;
    }
    static inline int shift_right(clm_id id, clm_uint start,
                                        clm_uint positions)
    {
        auto* array = detail::find_array(id);
        return array == nullptr ? CLM_BPA_FAILURE :
            rotate(array, start, positions, 1);
    }
    static inline int swap(clm_id id, clm_int left, clm_int right)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed || !detail::valid_index(*array, left) ||
            !detail::valid_index(*array, right)) return CLM_BPA_FAILURE;
        if (array->type == CLM_BPA_TYPE_STRING)
            std::swap(array->strings[left], array->strings[right]);
        else
            for (clm_uint i = 0; i < array->block_size; ++i)
                std::swap(array->blocks[left * array->block_size + i],
                        array->blocks[right * array->block_size + i]);
        array->deck_size = array->count;
        return CLM_BPA_SUCCESS;
    }
    static inline int copy(clm_id source, clm_id destination)
    {
        auto* input = detail::find_array(source);
        if (input == nullptr || array_exists(destination)) return CLM_BPA_FAILURE;
        try {
            auto copy = *input;
            copy.id = destination;
            copy.refcount = 1;
            copy.sealed = 0;
            if (copy.type == CLM_BPA_TYPE_STRING)
                for (const auto& text : copy.strings)
                    detail::track_pool_alloc(text.size() + 1);
            detail::arrays.emplace(destination, std::move(copy));
            return CLM_BPA_SUCCESS;
        } catch (...) { return CLM_BPA_FAILURE; }
    }
    static inline int copy(clm_id id, clm_int source_index,
                                    clm_int destination_index)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->type == CLM_BPA_TYPE_STRING ||
            !detail::valid_index(*array, source_index) ||
            !detail::valid_index(*array, destination_index)) return CLM_BPA_FAILURE;
        std::memmove(array->blocks.data() +
                        static_cast<std::size_t>(destination_index) * array->block_size,
                    array->blocks.data() +
                        static_cast<std::size_t>(source_index) * array->block_size,
                    array->block_size);
        array->deck_size = array->count;
        return CLM_BPA_SUCCESS;
    }
    static inline int shuffle(clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->sealed)
            return CLM_BPA_FAILURE;
        std::mt19937 engine(std::random_device{}());
        if (array->type == CLM_BPA_TYPE_STRING)
            std::ranges::shuffle(array->strings, engine);
        else {
            const auto width = array->block_size;
            for (clm_uint i = array->count; i > 1; --i) {
                const clm_uint j = std::uniform_int_distribution<clm_uint>(0, i - 1)(engine);
                for (clm_uint b = 0; b < width; ++b)
                    std::swap(array->blocks[(i - 1) * width + b], array->blocks[j * width + b]);
            }
        }
        array->deck_size = array->count;
        array->index = 0;
        return CLM_BPA_SUCCESS;
    }

    static inline int slice_to(clm_id id, clm_int start, clm_uint length,
                                        void* output, std::size_t size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->type == CLM_BPA_TYPE_STRING || start < 0 ||
            static_cast<clm_uint>(start) > array->count ||
            length > array->count - static_cast<clm_uint>(start) ||
            size < static_cast<std::size_t>(length) * array->block_size ||
            (length != 0 && output == nullptr)) return CLM_BPA_FAILURE;
        if (length != 0)
            std::memcpy(output, array->blocks.data() +
                            static_cast<std::size_t>(start) * array->block_size,
                        static_cast<std::size_t>(length) * array->block_size);
        return CLM_BPA_SUCCESS;
    }
    static inline int slice_borrow(clm_id id, clm_int start, clm_uint length,
                                            const void** output, std::size_t* size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->type == CLM_BPA_TYPE_STRING || output == nullptr ||
            size == nullptr || start < 0 ||
            static_cast<clm_uint>(start) > array->count ||
            length > array->count - static_cast<clm_uint>(start)) return CLM_BPA_FAILURE;
        *size = static_cast<std::size_t>(length) * array->block_size;
        *output = length == 0 ? nullptr : array->blocks.data() +
            static_cast<std::size_t>(start) * array->block_size;
        return CLM_BPA_SUCCESS;
    }
    static inline int slice(clm_id id, clm_int start, clm_uint length,
                                    clm_id destination)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || start < 0 ||
            static_cast<clm_uint>(start) > array->count ||
            length > array->count - static_cast<clm_uint>(start) ||
            array_exists(destination)) return CLM_BPA_FAILURE;
        if (create_sized(destination, array->type, static_cast<clm_int>(length),
                                array->block_size, array->encoding) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        auto* result = detail::find_array(destination);
        try {
            for (clm_uint i = 0; i < length; ++i) {
                if (array->type == CLM_BPA_TYPE_STRING)
                {
                    result->strings.push_back(array->strings[start + i]);
                    detail::track_pool_alloc(result->strings.back().size() + 1);
                    result->count = static_cast<clm_uint>(i + 1);
                }
                else {
                    const auto offset = static_cast<std::size_t>(start + i) * array->block_size;
                    detail::append_bytes(*result, array->blocks.data() + offset);
                }
            }
            result->count = length;
            return CLM_BPA_SUCCESS;
        } catch (...) { erase_array(destination); return CLM_BPA_FAILURE; }
    }

    static inline int borrow(clm_id id, void** output, std::size_t* size)
    {
        auto* block = detail::find_block(id);
        if (block == nullptr || output == nullptr || size == nullptr)
            return CLM_BPA_FAILURE;
        *output = block->data.data();
        *size = block->type == CLM_BPA_TYPE_STRING
            ? static_cast<std::size_t>(block->length) + 1 : block->block_size;
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int copy_block(clm_id source, clm_id destination)
    {
        auto* block = detail::find_block(source);
        auto* target = detail::find_block(destination);
        if (block == nullptr || target == nullptr || block->type != target->type ||
            (block->type != CLM_BPA_TYPE_STRING &&
            block->block_size != target->block_size)) return CLM_BPA_FAILURE;
        if (target->type == CLM_BPA_TYPE_STRING && !target->data.empty())
            detail::track_pool_free(target->length + 1);
        target->data = block->data;
        target->length = block->length;
        if (target->type == CLM_BPA_TYPE_STRING && !target->data.empty())
            detail::track_pool_alloc(target->length + 1);
        return CLM_BPA_SUCCESS;
    }

    static inline int get(clm_id id, clm_int index, clm_id destination,
                                clm_uint size)
    {
        auto* array = detail::find_array(id);
        auto* block = detail::find_block(destination);
        if (array == nullptr || array->type == CLM_BPA_TYPE_STRING ||
            !detail::valid_index(*array, index) || size < array->block_size ||
            block == nullptr || block->type == CLM_BPA_TYPE_STRING ||
            (array->type != CLM_BPA_TYPE_STRUCT && block->type != array->type) ||
            block->block_size != array->block_size)
            return CLM_BPA_FAILURE;
        std::memcpy(block->data.data(), array->blocks.data() +
                    static_cast<std::size_t>(index) * array->block_size, array->block_size);
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int set(clm_id id, clm_int index, clm_id source,
                                clm_uint size)
    {
        auto* block = detail::find_block(source);
        auto* array = detail::find_array(id);
        if (array == nullptr || block == nullptr ||
            (array->type != CLM_BPA_TYPE_STRUCT && array->type != block->type) ||
            size != array->block_size || block->block_size != array->block_size)
            return CLM_BPA_FAILURE;
        return set_raw(id, index, block->data.data(), block->block_size);
    }
    static inline int insert(clm_id id, clm_int index, clm_id source,
                                    clm_uint size)
    {
        auto* block = detail::find_block(source);
        if (block == nullptr || size < block->block_size) return CLM_BPA_FAILURE;
        auto* array = detail::find_array(id);
        if (array == nullptr || block->type == CLM_BPA_TYPE_STRING ||
            (array->type != CLM_BPA_TYPE_STRUCT && block->type != array->type) ||
            size != array->block_size || block->block_size != array->block_size)
            return CLM_BPA_FAILURE;
        if (array->type == CLM_BPA_TYPE_STRING)
            return CLM_BPA_FAILURE;
        return insert_raw(id, index, block->data.data(), block->block_size);
    }
    static inline int append(clm_id id, clm_id source, clm_uint size)
    {
        auto* block = detail::find_block(source);
        auto* array = detail::find_array(id);
        if (block == nullptr || array == nullptr || block->type == CLM_BPA_TYPE_STRING ||
            (array->type != CLM_BPA_TYPE_STRUCT && block->type != array->type) ||
            size != array->block_size || block->block_size != array->block_size)
            return CLM_BPA_FAILURE;
        return array->type == CLM_BPA_TYPE_STRING
            ? append_string_raw(id, block->data.data(), block->length + 1)
            : append_raw(id, block->data.data(), block->block_size);
    }
    static inline int get_string(clm_id id, clm_int index,
                                        clm_id destination, clm_uint size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->type != CLM_BPA_TYPE_STRING ||
            !detail::valid_index(*array, index))
            return CLM_BPA_FAILURE;
        auto* block = detail::find_block(destination);
        if (block != nullptr && block->type != CLM_BPA_TYPE_STRING)
            return CLM_BPA_FAILURE;
        const auto& value = array->strings[index];
        if (size < value.size() + 1)
            return CLM_BPA_FAILURE;
        const bool created = block == nullptr;
        if (created && create(destination, CLM_BPA_TYPE_STRING) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        if (set_string(destination, value.c_str(),
                       static_cast<clm_uint>(value.size() + 1)) < 0) {
            if (created)
                erase_block(destination);
            return CLM_BPA_FAILURE;
        }
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int set_string(clm_id id, clm_int index,
                                        clm_id source, clm_uint size)
    {
        auto* block = detail::find_block(source);
        if (block == nullptr || block->type != CLM_BPA_TYPE_STRING || size < block->length + 1)
            return CLM_BPA_FAILURE;
        return set_string_raw(id, index, block->data.data(), block->length + 1) < 0
            ? CLM_BPA_FAILURE : CLM_BPA_SET_SUCCESS;
    }
    static inline int insert_string(clm_id id, clm_int index,
                                            clm_id source)
    {
        auto* array = detail::find_array(id);
        auto* block = detail::find_block(source);
        if (array == nullptr || block == nullptr || array->type != CLM_BPA_TYPE_STRING ||
            block->type != CLM_BPA_TYPE_STRING || array->sealed || index < 0 ||
            static_cast<clm_uint>(index) > array->count) return CLM_BPA_FAILURE;
        try {
            array->strings.insert(array->strings.begin() + index,
                                reinterpret_cast<const char*>(block->data.data()));
            detail::track_pool_alloc(block->length + 1);
            ++array->count;
            array->capacity = std::max(array->capacity, array->count);
            array->deck_size = array->count;
            return CLM_BPA_SUCCESS;
        } catch (...) { return CLM_BPA_FAILURE; }
    }
    static inline int append_string(clm_id id, clm_id source)
    {
        return insert_string(id, static_cast<clm_int>(
            detail::find_array(id) == nullptr ? 0 : detail::find_array(id)->count), source);
    }

    static inline int advanced_borrow(clm_id id, void** output, std::size_t* size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || output == nullptr || size == nullptr ||
            array->type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
        *output = array->blocks.data();
        *size = array->blocks.size();
        return CLM_BPA_SUCCESS;
    }
    static inline int advanced_release(clm_id id)
    {
        return detail::find_array(id) == nullptr ? CLM_BPA_FAILURE : CLM_BPA_SUCCESS;
    }
    static inline void* advanced_data(clm_id id)
    {
        auto* array = detail::find_array(id);
        return array == nullptr || array->type == CLM_BPA_TYPE_STRING
            ? nullptr : array->blocks.data();
    }
    static inline int advanced_get_raw(clm_id id, clm_int index,
                                                void* output, clm_uint size)
    {
        return get_raw(id, index, output, size);
    }
    static inline int advanced_set_raw(clm_id id, clm_int index,
                                                const void* input, clm_uint size)
    {
        return set_raw(id, index, input, size);
    }
    static inline int advanced_insert_raw(clm_id id, clm_int index,
                                                const void* input, clm_uint size)
    {
        return insert_raw(id, index, input, size);
    }
    static inline int advanced_append_raw(clm_id id, const void* input,
                                                clm_uint size)
    {
        return append_raw(id, input, size);
    }
    static inline int advanced_remove(clm_id id, clm_int index)
    {
        return remove(id, index);
    }
    static inline int advanced_clear(clm_id id) { return clear(id); }
    static inline int advanced_extend(clm_id id, clm_uint amount)
    {
        return extend(id, amount);
    }
    static inline int advanced_shrink(clm_id id, clm_uint amount)
    {
        return shrink(id, amount);
    }
    static inline int advanced_borrow(clm_id id,
                                            clm_bpaa_array_node** borrowed)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || borrowed == nullptr ||
            array->refcount == std::numeric_limits<clm_ushort>::max())
            return CLM_BPA_FAILURE;
        ++array->refcount;
        *borrowed = array;
        return CLM_BPA_SUCCESS;
    }
    static inline int advanced_release(clm_bpaa_array_node** borrowed)
    {
        if (borrowed == nullptr || *borrowed == nullptr)
            return CLM_BPA_FAILURE;
        auto* array = *borrowed;
        *borrowed = nullptr;
        if (array->refcount == 0) return CLM_BPA_FAILURE;
        const auto id = array->id;
        return --array->refcount == 0 ? erase_array(id) : CLM_BPA_SUCCESS;
    }
    static inline void* advanced_data(const clm_bpaa_array_node* array)
    {
        return array == nullptr || array->type == CLM_BPA_TYPE_STRING
            ? nullptr : const_cast<unsigned char*>(array->blocks.data());
    }
    static inline int advanced_get_raw(const clm_bpaa_array_node* array,
                                                clm_int index, void* output,
                                                clm_uint size)
    {
        if (array == nullptr || array->type == CLM_BPA_TYPE_STRING ||
            !detail::valid_index(*array, index) || output == nullptr ||
            size < array->block_size) return CLM_BPA_FAILURE;
        std::memcpy(output, array->blocks.data() +
                        static_cast<std::size_t>(index) * array->block_size,
                    array->block_size);
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int advanced_set_raw(clm_bpaa_array_node* array,
                                                clm_int index, const void* input,
                                                clm_uint size)
    {
        if (array == nullptr || array->type == CLM_BPA_TYPE_STRING || array->sealed ||
            !detail::valid_index(*array, index) || input == nullptr ||
            size != array->block_size) return CLM_BPA_FAILURE;
        std::memmove(array->blocks.data() +
                        static_cast<std::size_t>(index) * array->block_size,
                    input, array->block_size);
        array->deck_size = array->count;
        return CLM_BPA_SET_SUCCESS;
    }
    static inline int advanced_insert_raw(clm_bpaa_array_node* array,
                                                clm_int index, const void* input,
                                                clm_uint size)
    {
        if (array == nullptr || array->sealed || index < 0 ||
            static_cast<clm_uint>(index) > array->count || input == nullptr ||
            size != array->block_size) return CLM_BPA_FAILURE;
        if (array->count == array->capacity) {
            const auto next = array->capacity == 0 ? 1u :
                (array->capacity > std::numeric_limits<clm_uint>::max() / 2u ? 0u : array->capacity * 2u);
            if (next == 0 || !detail::resize_capacity(*array, next)) return CLM_BPA_FAILURE;
        }
        auto* destination = array->blocks.data() +
            static_cast<std::size_t>(index) * array->block_size;
        std::memmove(destination + array->block_size, destination,
            static_cast<std::size_t>(array->count - index) * array->block_size);
        std::memcpy(destination, input, array->block_size);
        ++array->count;
        array->deck_size = array->count;
        return CLM_BPA_SUCCESS;
    }
    static inline int advanced_append_raw(clm_bpaa_array_node* array,
                                                const void* input, clm_uint size)
    {
        if (array == nullptr || array->sealed || size != array->block_size)
            return CLM_BPA_FAILURE;
        return detail::append_bytes(*array, input) ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
    }
    static inline int advanced_remove(const clm_bpaa_array_node* array, const clm_int index)
    {
        if (array == nullptr) return CLM_BPA_FAILURE;
        return remove(array->id, index);
    }
    static inline int advanced_clear(const clm_bpaa_array_node* array)
    {
        return array == nullptr ? CLM_BPA_FAILURE : clear(array->id);
    }
    static inline int advanced_extend(const clm_bpaa_array_node* array, const clm_uint amount)
    {
        return array == nullptr ? CLM_BPA_FAILURE : extend(array->id, amount);
    }
    static inline int advanced_shrink(const clm_bpaa_array_node* array, const clm_uint amount)
    {
        return array == nullptr ? CLM_BPA_FAILURE : shrink(array->id, amount);
    }

    static inline int draw_at_raw(const clm_id id, const clm_int index, void* output, const clm_uint size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->type == CLM_BPA_TYPE_STRING ||
            output == nullptr || size < array->block_size || index < 0 ||
            static_cast<clm_uint>(index) >= array->deck_size)
            return CLM_BPA_FAILURE;
        const clm_uint last = array->deck_size - 1;
        auto* selected = array->blocks.data() +
            static_cast<std::size_t>(index) * array->block_size;
        auto* tail = array->blocks.data() +
            static_cast<std::size_t>(last) * array->block_size;
        if (static_cast<clm_uint>(index) != last)
            for (clm_uint i = 0; i < array->block_size; ++i)
                std::swap(selected[i], tail[i]);
        std::memcpy(output, tail, array->block_size);
        --array->deck_size;
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int draw_raw(clm_id id, void* output, clm_uint size)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->type == CLM_BPA_TYPE_STRING ||
            output == nullptr || size < array->block_size || array->count == 0)
            return CLM_BPA_FAILURE;
        if (array->deck_size == 0 && shuffle(id) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        std::random_device random;
        const auto index = std::uniform_int_distribution<clm_uint>(
            0, array->deck_size - 1)(random);
        return draw_at_raw(id, static_cast<clm_int>(index), output, size);
    }
    static inline int draw(clm_id id, clm_id destination, clm_uint size)
    {
        auto* array = detail::find_array(id);
        auto* block = detail::find_block(destination);
        if (array == nullptr || block == nullptr || array->count == 0 ||
            array->type == CLM_BPA_TYPE_STRING ||
            block->type == CLM_BPA_TYPE_STRING ||
            block->block_size != array->block_size || size < array->block_size)
            return CLM_BPA_FAILURE;
        return draw_raw(id, block->data.data(), size);
    }
    static inline int draw_at(clm_id id, clm_int index,
                                    clm_id destination, clm_uint size)
    {
        auto* array = detail::find_array(id);
        auto* block = detail::find_block(destination);
        if (array == nullptr || block == nullptr || array->type == CLM_BPA_TYPE_STRING ||
            block->type == CLM_BPA_TYPE_STRING || block->block_size != array->block_size ||
            size < array->block_size) return CLM_BPA_FAILURE;
        return draw_at_raw(id, index, block->data.data(), size);
    }

    static inline int get_count(clm_id id)
    {
        auto* array = detail::find_array(id);
        return array == nullptr || array->count > INT_MAX ? CLM_BPA_FAILURE :
            static_cast<int>(array->count);
    }
    static inline int get_capacity(clm_id id)
    {
        auto* array = detail::find_array(id);
        return array == nullptr || array->capacity > INT_MAX ? CLM_BPA_FAILURE :
            static_cast<int>(array->capacity);
    }
    static inline int get_length(clm_id id)
    {
        auto* array = detail::find_array(id);
        const auto bytes = array == nullptr ? 0 :
            static_cast<std::uint64_t>(array->count) * array->block_size;
        return array == nullptr || bytes > INT_MAX ? CLM_BPA_FAILURE :
            static_cast<int>(bytes);
    }
    static inline int get_current_index(clm_id id)
    {
        auto* array = detail::find_array(id);
        return array == nullptr || array->index > INT_MAX ? CLM_BPA_FAILURE :
            static_cast<int>(array->index);
    }
    static inline int is_sealed(clm_id id)
    {
        auto* array = detail::find_array(id);
        return array == nullptr ? CLM_BPA_FAILURE : array->sealed != 0;
    }
    static inline int get_first(clm_id id, clm_id destination, clm_uint size)
    {
        return get(id, 0, destination, size);
    }
    static inline int get_last(clm_id id, clm_id destination, clm_uint size)
    {
        auto* array = detail::find_array(id);
        return array == nullptr || array->count == 0 ? CLM_BPA_FAILURE :
            get(id, static_cast<clm_int>(array->count - 1), destination, size);
    }
    static inline int get_current(clm_id id, clm_id destination, clm_uint size)
    {
        auto* array = detail::find_array(id);
        return array == nullptr ? CLM_BPA_FAILURE :
            get(id, static_cast<clm_int>(array->index), destination, size);
    }

    static inline int write_u32(clm_id id, clm_uint value)
    {
        auto* block = detail::find_block(id);
        if (block == nullptr || block->type != CLM_BPA_TYPE_UNSIGNED_INT ||
            block->block_size != sizeof(value)) return CLM_BPA_FAILURE;
        std::memcpy(block->data.data(), &value, sizeof(value));
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int write_i32(clm_id id, clm_int value)
    {
        auto* block = detail::find_block(id);
        if (block == nullptr || block->type != CLM_BPA_TYPE_INT ||
            block->block_size != sizeof(value)) return CLM_BPA_FAILURE;
        std::memcpy(block->data.data(), &value, sizeof(value));
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int write_byte(clm_id id, clm_byte value)
    {
        auto* block = detail::find_block(id);
        if (block == nullptr || block->type != CLM_BPA_TYPE_BYTE ||
            block->block_size != sizeof(value)) return CLM_BPA_FAILURE;
        std::memcpy(block->data.data(), &value, sizeof(value));
        return CLM_BPA_GET_SUCCESS;
    }
    static inline int get_to(clm_id id, void* output, clm_uint size)
    {
        return get(id, output, size);
    }
    static inline int set_from(clm_id id, const void* input, clm_uint size)
    {
        return set(id, input, size);
    }
    static inline int get_string_to(clm_id id, void* output, clm_uint size)
    {
        return get_string(id, output, size);
    }
    static inline int set_string_from(clm_id id, const void* input, clm_uint size)
    {
        return set_string(id, input, size);
    }
    static inline int get_to(clm_id id, clm_int index, void* output, clm_uint size)
    {
        return get_raw(id, index, output, size);
    }
    static inline int get_first_to(clm_id id, void* output, clm_uint size)
    {
        return get_first_raw(id, output, size);
    }
    static inline int get_last_to(clm_id id, void* output, clm_uint size)
    {
        return get_last_raw(id, output, size);
    }
    static inline int get_current_to(clm_id id, void* output, clm_uint size)
    {
        return get_current_raw(id, output, size);
    }
    static inline int get_string_to(clm_id id, clm_int index,
                                            void* output, clm_uint size)
    {
        return get_string_raw(id, index, output, size);
    }
    static inline int set_from(clm_id id, clm_int index, const void* input,
                                        clm_uint size)
    {
        return set_raw(id, index, input, size);
    }
    static inline int set_string_from(clm_id id, clm_int index,
                                            const void* input, clm_uint size)
    {
        return set_string_raw(id, index, input, size);
    }
    static inline int insert_from(clm_id id, clm_int index, const void* input,
                                        clm_uint size)
    {
        return insert_raw(id, index, input, size);
    }
    static inline int append_from(clm_id id, const void* input, clm_uint size)
    {
        return append_raw(id, input, size);
    }
    static inline int draw_to(clm_id id, void* output, clm_uint size)
    {
        return draw_raw(id, output, size);
    }
    static inline int draw_at_to(clm_id id, clm_int index,
                                        void* output, clm_uint size)
    {
        return draw_at_raw(id, index, output, size);
    }
    static inline int get_count(clm_id array_id, clm_id destination_id)
    {
        clm_uint value = 0;
        return get_count_to(array_id, &value) == CLM_BPA_GET_SUCCESS
            ? write_u32(destination_id, value) : CLM_BPA_FAILURE;
    }
    static inline int get_capacity(clm_id array_id, clm_id destination_id)
    {
        clm_uint value = 0;
        return get_capacity_to(array_id, &value) == CLM_BPA_GET_SUCCESS
            ? write_u32(destination_id, value) : CLM_BPA_FAILURE;
    }
    static inline int get_length(clm_id array_id, clm_id destination_id)
    {
        clm_uint value = 0;
        return get_length_to(array_id, &value) == CLM_BPA_GET_SUCCESS
            ? write_u32(destination_id, value) : CLM_BPA_FAILURE;
    }
    static inline int get_current_index(clm_id array_id, clm_id destination_id)
    {
        clm_uint value = 0;
        return get_current_index_to(array_id, &value) == CLM_BPA_GET_SUCCESS
            ? write_i32(destination_id, static_cast<clm_int>(value))
            : CLM_BPA_FAILURE;
    }
    static inline int is_sealed(clm_id array_id, clm_id destination_id)
    {
        clm_byte value = 0;
        return is_sealed_to(array_id, &value) == CLM_BPA_GET_SUCCESS
            ? write_byte(destination_id, value) : CLM_BPA_FAILURE;
    }
    static inline int get_string(clm_id array_id, clm_int index, void* output, clm_uint size)
    {
        return get_string_raw(array_id, index, output, size);
    }
    static inline int set_string(clm_id array_id, clm_int index, const void* input, clm_uint size)
    {
        return set_string_raw(array_id, index, input, size);
    }

    static inline int shutdown()
    {
        detail::arrays.clear();
        detail::blocks.clear();
        std::ranges::fill(detail::pool_active, 0);
        return pool_cleanup();
    }
}

using clm_bpa_block_node = clm::bpa::clm_bpa_block_node;
using clm_bpaa_array_node = clm::bpa::clm_bpaa_array_node;
using clm::bpa::CLM_BPA_FAILURE;
using clm::bpa::CLM_BPA_SUCCESS;
using clm::bpa::CLM_BPA_GET_SUCCESS;
using clm::bpa::CLM_BPA_SET_SUCCESS;
using clm::bpa::CLM_BPA_TYPE_STRUCT;
using clm::bpa::CLM_BPA_TYPE_SIGNED_BYTE;
using clm::bpa::CLM_BPA_TYPE_BYTE;
using clm::bpa::CLM_BPA_TYPE_SHORT;
using clm::bpa::CLM_BPA_TYPE_UNSIGNED_SHORT;
using clm::bpa::CLM_BPA_TYPE_INT;
using clm::bpa::CLM_BPA_TYPE_UNSIGNED_INT;
using clm::bpa::CLM_BPA_TYPE_LONG;
using clm::bpa::CLM_BPA_TYPE_UNSIGNED_LONG;
using clm::bpa::CLM_BPA_TYPE_FLOAT;
using clm::bpa::CLM_BPA_TYPE_DOUBLE;
using clm::bpa::CLM_BPA_TYPE_STRING;
using clm::bpa::CLM_BPA_TYPE_UUID;
using clm::bpa::CLM_BPA_TYPE_DATETIME;

static inline bool clm_bpa_is_existing(const clm_id id)
{
    return clm::bpa::block_exists(id);
}

static inline bool clm_bpaa_is_existing(const clm_id id)
{
    return clm::bpa::array_exists(id);
}

static inline int clm_bpa_create(const clm_id id, const clm_byte type)
{
    return clm::bpa::create(id, type);
}

static inline int clm_bpa_delete(const clm_id id)
{
    if (clm::bpa::block_exists(id))
        return clm::bpa::erase_block(id);
    if (clm::bpa::array_exists(id))
        return clm::bpa::erase_array(id);
    return clm::bpa::CLM_BPA_FAILURE;
}

static inline int clm_bpa_set_string(const clm_id id, const void* text, const clm_uint size)
{
    return clm::bpa::set_string(id, text, size);
}

static inline int clm_bpa_get_string(const clm_id id, void* buffer, const clm_uint size)
{
    return clm::bpa::get_string(id, buffer, size);
}

static inline int clm_bpa_create_sized(const clm_id id, const clm_byte type, const clm_uint size)
{
    return clm::bpa::create_sized(id, type, size);
}

static inline clm_bpa_block_node* clm_bpa_find_block(const clm_id id)
{
    return clm::bpa::detail::find_block(id);
}

static inline int clm_bpa_set(const clm_id id, const void* input, const clm_uint size)
{
    return clm::bpa::set(id, input, size);
}

static inline int clm_bpa_get(const clm_id id, void* output, const clm_uint size)
{
    return clm::bpa::get(id, output, size);
}

static inline int clm_bpa_replace(const clm_id id, const void* input, const clm_uint size)
{
    return clm::bpa::replace(id, input, size);
}

static inline int clm_bpa_ref(const clm_id id)
{
    if (clm::bpa::block_exists(id))
        return clm::bpa::ref_block(id);
    if (clm::bpa::array_exists(id))
        return clm::bpa::ref_array(id);
    return CLM_BPA_FAILURE;
}

static inline int clm_bpa_unref(const clm_id id)
{
    if (clm::bpa::block_exists(id))
        return clm::bpa::unref_block(id);
    if (clm::bpa::array_exists(id))
        return clm::bpa::unref_array(id);
    return CLM_BPA_FAILURE;
}

static inline clm_bpaa_array_node* clm_bpaa_find_array(const clm_id id)
{
    return clm::bpa::detail::find_array(id);
}

static inline int clm_bpaa_get_raw(const clm_id id, const clm_int index, void* output, const clm_uint size)
{
    return clm::bpa::get_raw(id, index, output, size);
}

static inline int clm_bpaa_create(const clm_id id, const clm_byte type, const clm_uint block_size, const clm_uint reserved)
{
    return clm::bpa::create(id, type, block_size, reserved);
}

static inline int clm_bpaa_append_raw(const clm_id id, const void* input, const clm_uint size)
{
    const auto* array = clm::bpa::detail::find_array(id);
    if (array != nullptr && array->type == CLM_BPA_TYPE_STRING)
        return clm::bpa::append_string_raw(id, input, size);
    return clm::bpa::append_raw(id, input, size);
}

static inline int clm_bpaa_insert_raw(const clm_id id, const clm_int index, const void* input, const clm_uint size)
{
    return clm::bpa::insert_raw(id, index, input, size);
}

static inline int clm_bpaa_remove(const clm_id id, const clm_int index)
{
    return clm::bpa::remove(id, index);
}

static inline int clm_bpaa_clear(const clm_id id)
{
    return clm::bpa::clear(id);
}

static inline int clm_bpaa_get_string_raw(const clm_id id, const clm_int index, void* output, const clm_uint size)
{
    return clm::bpa::get_string_raw(id, index, output, size);
}

static inline int clm_bpaa_append_string_raw(const clm_id id, const void* input, const clm_uint size)
{
    return clm::bpa::append_string_raw(id, input, size);
}

static inline int clm_bpaa_set_string_raw(const clm_id id, const clm_int index, const void* input, const clm_uint size)
{
    return clm::bpa::set_string_raw(id, index, input, size);
}

#define CLM_BPA_FORWARD_GLOBAL(name, target) \
    template <typename... Args> \
    static inline auto name(Args&&... args) \
        -> decltype(clm::bpa::target(std::forward<Args>(args)...)) \
    { \
        return clm::bpa::target(std::forward<Args>(args)...); \
    }

CLM_BPA_FORWARD_GLOBAL(clm_bpaa_get_count_to, get_count_to)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_get_capacity_to, get_capacity_to)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_get_first_raw, get_first_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_get_last_raw, get_last_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_set_raw, set_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_move_to, move_to)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_get_current_index_to, get_current_index_to)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_get_current_raw, get_current_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_move_previous, move_previous)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_move_next, move_next)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_move_last, move_last)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_reset, reset)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_extend, extend)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_shrink, shrink)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_reverse, reverse)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_sort_with, sort_with)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_shift_left, shift_left)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_shift_right, shift_right)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_swap, swap)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_copy, copy)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_shuffle, shuffle)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_draw_at_raw, draw_at_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_draw_raw, draw_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_is_sealed_to, is_sealed_to)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_seal, seal)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_remove_first, remove_first)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_unseal, unseal)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_move_first, move_first)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_get_length_to, get_length_to)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_create_sized, create_sized)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_slice, slice)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_slice_to, slice_to)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_slice_borrow, slice_borrow)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_append_string, append_string)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_get_string, get_string)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_set_string, set_string)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_borrow, advanced_borrow)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_release, advanced_release)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_data, advanced_data)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_get_raw, advanced_get_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_set_raw, advanced_set_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_extend, advanced_extend)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_append_raw, advanced_append_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_insert_raw, advanced_insert_raw)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_remove, advanced_remove)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_shrink, advanced_shrink)
CLM_BPA_FORWARD_GLOBAL(clm_bpaa_advanced_clear, advanced_clear)
CLM_BPA_FORWARD_GLOBAL(clm_bpa_pool_stats, pool_stats)
CLM_BPA_FORWARD_GLOBAL(clm_bpa_pool_cleanup, pool_cleanup)

static inline int clm_bpaa_sort(
    const clm_id id,
    const clm::bpa::clm_bpaa_predicate_fn predicate)
{
    return clm::bpa::sort(id, predicate);
}

static inline int clm_bpaa_ref(const clm_id id)
{
    return clm::bpa::ref_array(id);
}

static inline int clm_bpaa_unref(const clm_id id)
{
    return clm::bpa::unref_array(id);
}

static inline int clm_bpaa_delete(const clm_id id)
{
    return clm::bpa::erase_array(id);
}

#undef CLM_BPA_FORWARD_GLOBAL
