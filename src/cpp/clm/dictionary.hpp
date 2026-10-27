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

#include "base.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

namespace clm::dictionary {

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

namespace detail {
struct dictionary_data {
    clm_byte type;
    std::size_t value_size;
    std::vector<clm_id> keys;
    std::unordered_map<clm_id, std::vector<unsigned char>> values;
    std::size_t iterator = 0;
};

inline std::unordered_map<clm_id, dictionary_data>& dictionaries()
{
    static std::unordered_map<clm_id, dictionary_data> value;
    return value;
}

inline std::size_t type_size(const clm_byte type)
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
    case CLM_BPA_TYPE_DATETIME: return sizeof(std::int64_t);
    default: return 0;
    }
}
}

/** Creates an empty dictionary for fixed-size values, or variable-length strings. */
inline int create(const clm_id id, const clm_byte type, const std::size_t size)
{
    std::size_t value_size = detail::type_size(type);
    if (detail::dictionaries().contains(id))
        return CLM_BPA_FAILURE;
    if (type == CLM_BPA_TYPE_STRUCT) {
        if (size == 0 || size > UINT_MAX)
            return CLM_BPA_FAILURE;
        value_size = size;
    } else if (type == CLM_BPA_TYPE_STRING) {
        if (size != 0)
            return CLM_BPA_FAILURE;
        value_size = 0;
    } else if (value_size == 0 || (size != 0 && size != value_size)) {
        return CLM_BPA_FAILURE;
    }
    try {
        detail::dictionaries().emplace(id, detail::dictionary_data{type, value_size, {}, {}, 0});
    } catch (...) {
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SUCCESS;
}

inline int erase(const clm_id id)
{
    return detail::dictionaries().erase(id) == 1
        ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

/** Copies bytes into a key. Fixed-width values require exact size; strings need a NUL. */
inline int set_from(const clm_id id, const clm_id key, const void* value, const std::size_t value_size)
{
    auto dictionary = detail::dictionaries().find(id);
    if (dictionary == detail::dictionaries().end() || value == nullptr ||
        value_size > UINT_MAX ||
        (dictionary->second.type != CLM_BPA_TYPE_STRING &&
         value_size != dictionary->second.value_size))
        return CLM_BPA_FAILURE;
    if (dictionary->second.type == CLM_BPA_TYPE_STRING &&
        (value_size == 0 || std::memchr(value, '\0', value_size) == nullptr))
        return CLM_BPA_FAILURE;
    try {
        if (dictionary->second.type == CLM_BPA_TYPE_STRING) {
            const auto* termination = static_cast<const unsigned char*>(
                std::memchr(value, '\0', value_size));
            const std::size_t actual_size = static_cast<std::size_t>(termination - static_cast<const unsigned char*>(value)) + 1;
            std::vector<unsigned char> bytes(actual_size);
            std::memcpy(bytes.data(), value, actual_size);
            auto existing = dictionary->second.values.find(key);
            if (existing == dictionary->second.values.end()) {
                dictionary->second.keys.push_back(key);
                try { dictionary->second.values.emplace(key, std::move(bytes)); }
                catch (...) { dictionary->second.keys.pop_back(); return CLM_BPA_FAILURE; }
                dictionary->second.iterator = 0;
            } else { existing->second = std::move(bytes); }
            return CLM_BPA_SET_SUCCESS;
        }
        std::vector<unsigned char> bytes(value_size);
        if (value_size != 0)
            std::memcpy(bytes.data(), value, value_size);
        auto existing = dictionary->second.values.find(key);
        if (existing == dictionary->second.values.end()) {
            dictionary->second.keys.push_back(key);
            try {
                dictionary->second.values.emplace(key, std::move(bytes));
            } catch (...) {
                dictionary->second.keys.pop_back();
                return CLM_BPA_FAILURE;
            }
            dictionary->second.iterator = 0;
        } else {
            existing->second = std::move(bytes);
        }
    } catch (...) {
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SET_SUCCESS;
}

inline int set(const clm_id id, const clm_id key, const void* value, const std::size_t value_size)
{
    return set_from(id, key, value, value_size);
}

inline int get_to(const clm_id id, const clm_id key, void* value, const std::size_t value_size)
{
    const auto dictionary = detail::dictionaries().find(id);
    if (dictionary == detail::dictionaries().end() || value == nullptr)
        return CLM_BPA_FAILURE;
    const auto found = dictionary->second.values.find(key);
    if (found == dictionary->second.values.end() ||
        value_size < found->second.size())
        return CLM_BPA_FAILURE;
    if (!found->second.empty())
        std::memcpy(value, found->second.data(), found->second.size());
    return CLM_BPA_GET_SUCCESS;
}

inline int get(const clm_id id, const clm_id key,
                              void* value, const std::size_t value_size)
{
    return get_to(id, key, value, value_size);
}

inline bool contains(const clm_id id, const clm_id key)
{
    const auto dictionary = detail::dictionaries().find(id);
    return dictionary != detail::dictionaries().end() &&
        dictionary->second.values.contains(key);
}

inline clm_int count(const clm_id id)
{
    const auto dictionary = detail::dictionaries().find(id);
    if (dictionary == detail::dictionaries().end() ||
        dictionary->second.keys.size() > static_cast<std::size_t>(INT_MAX))
        return CLM_BPA_FAILURE;
    return static_cast<clm_int>(dictionary->second.keys.size());
}

inline int remove(const clm_id id, const clm_id key)
{
    auto dictionary = detail::dictionaries().find(id);
    if (dictionary == detail::dictionaries().end() ||
        dictionary->second.values.erase(key) != 1)
        return CLM_BPA_FAILURE;
    auto& keys = dictionary->second.keys;
    keys.erase(std::ranges::find(keys, key));
    dictionary->second.iterator = 0;
    return CLM_BPA_SUCCESS;
}

inline int iterate_reset(const clm_id id)
{
    auto dictionary = detail::dictionaries().find(id);
    if (dictionary == detail::dictionaries().end())
        return CLM_BPA_FAILURE;
    dictionary->second.iterator = 0;
    return CLM_BPA_SUCCESS;
}

inline int iterate_next(const clm_id id, clm_id* key)
{
    auto dictionary = detail::dictionaries().find(id);
    if (dictionary == detail::dictionaries().end() || key == nullptr ||
        dictionary->second.iterator >= dictionary->second.keys.size())
        return CLM_BPA_FAILURE;
    *key = dictionary->second.keys[dictionary->second.iterator++];
    return CLM_BPA_GET_SUCCESS;
}

inline int clear(const clm_id id)
{
    auto dictionary = detail::dictionaries().find(id);
    if (dictionary == detail::dictionaries().end())
        return CLM_BPA_FAILURE;
    dictionary->second.keys.clear();
    dictionary->second.values.clear();
    dictionary->second.iterator = 0;
    return CLM_BPA_SUCCESS;
}

inline int add_from(const clm_id id, const clm_id key, const void* value, const std::size_t size)
{
    return contains(id, key)
        ? CLM_BPA_FAILURE : set_from(id, key, value, size);
}

inline int update_from(const clm_id id, const clm_id key, const void* value, const std::size_t size)
{
    return contains(id, key)
        ? set_from(id, key, value, size) : CLM_BPA_FAILURE;
}

inline int keys_to(const clm_id id, clm_id* buffer, const std::size_t capacity)
{
    const auto dictionary = detail::dictionaries().find(id);
    if (dictionary == detail::dictionaries().end() ||
        capacity < dictionary->second.keys.size() ||
        (!dictionary->second.keys.empty() && buffer == nullptr))
        return CLM_BPA_FAILURE;
    if (!dictionary->second.keys.empty())
        std::ranges::copy(dictionary->second.keys, buffer);
    return CLM_BPA_GET_SUCCESS;
}

}
