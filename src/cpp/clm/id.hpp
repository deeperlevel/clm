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
#include <limits>
#include <ranges>
#include <string>
#include <unordered_map>

namespace clm::id {

inline constexpr clm_id CLM_ID_NOT_FOUND = std::numeric_limits<clm_id>::max();
inline constexpr int CLM_BPA_FAILURE = dictionary::CLM_BPA_FAILURE;
inline constexpr int CLM_BPA_SUCCESS = dictionary::CLM_BPA_SUCCESS;
inline constexpr int CLM_BPA_GET_SUCCESS = dictionary::CLM_BPA_GET_SUCCESS;
inline constexpr int CLM_BPA_SET_SUCCESS = dictionary::CLM_BPA_SET_SUCCESS;

namespace detail {
struct mapping {
    clm_id value;
    std::string spelling;
};

struct state {
    std::unordered_map<std::string, mapping> mappings;
    bool case_sensitive = true;
    clm_id next_generated = 0;
    bool generator_exhausted = false;
};

inline state& registry()
{
    static state value;
    return value;
}

inline unsigned char folded(const unsigned char value)
{
    return value >= 'A' && value <= 'Z'
        ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
}

inline std::string normalize(const std::string& value, const bool sensitive)
{
    if (sensitive)
        return value;
    std::string result = value;
    std::ranges::transform(result, result.begin(),
                           [](const unsigned char character) {
                               return static_cast<char>(folded(character));
                           });
    return result;
}

inline bool valid_utf8(const std::string& value)
{
    const auto* text = reinterpret_cast<const unsigned char*>(value.data());
    const std::size_t length = value.size();
    for (std::size_t index = 0; index < length;) {
        const unsigned char first = text[index];
        if (first <= 0x7f) {
            ++index;
        } else if (first >= 0xc2 && first <= 0xdf) {
            if (index + 1 >= length || text[index + 1] < 0x80 ||
                text[index + 1] > 0xbf)
                return false;
            index += 2;
        } else if (first >= 0xe0 && first <= 0xef) {
            if (index + 2 >= length || text[index + 1] < 0x80 ||
                text[index + 1] > 0xbf || text[index + 2] < 0x80 ||
                text[index + 2] > 0xbf ||
                (first == 0xe0 && text[index + 1] < 0xa0) ||
                (first == 0xed && text[index + 1] >= 0xa0))
                return false;
            index += 3;
        } else if (first >= 0xf0 && first <= 0xf4) {
            if (index + 3 >= length || text[index + 1] < 0x80 ||
                text[index + 1] > 0xbf || text[index + 2] < 0x80 ||
                text[index + 2] > 0xbf || text[index + 3] < 0x80 ||
                text[index + 3] > 0xbf ||
                (first == 0xf0 && text[index + 1] < 0x90) ||
                (first == 0xf4 && text[index + 1] >= 0x90))
                return false;
            index += 4;
        } else {
            return false;
        }
    }
    return true;
}
}

inline int add(const char* name, const clm_id value)
{
    if (name == nullptr || *name == '\0')
        return CLM_BPA_FAILURE;
    const std::string text(name);
    if (!detail::valid_utf8(text))
        return CLM_BPA_FAILURE;
    auto& registry = detail::registry();
    const auto key = detail::normalize(text, registry.case_sensitive);
    try {
        if (registry.mappings.contains(key))
            return CLM_BPA_FAILURE;
        registry.mappings.emplace(key, detail::mapping{value, text});
    } catch (...) {
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SET_SUCCESS;
}

inline int map(const char* name, const clm_id value)
{
    return add(name, value);
}

inline clm_id get(const char* name)
{
    if (name == nullptr || *name == '\0')
        return CLM_ID_NOT_FOUND;
    const std::string text(name);
    if (!detail::valid_utf8(text))
        return CLM_ID_NOT_FOUND;
    const auto& registry = detail::registry();
    const auto entry = registry.mappings.find(
        detail::normalize(text, registry.case_sensitive));
    return entry == registry.mappings.end()
        ? CLM_ID_NOT_FOUND : entry->second.value;
}

inline int try_get(const char* name, clm_id* out_id)
{
    if (name == nullptr || *name == '\0' || out_id == nullptr)
        return CLM_BPA_FAILURE;
    const std::string text(name);
    if (!detail::valid_utf8(text))
        return CLM_BPA_FAILURE;
    const auto& registry = detail::registry();
    const auto entry = registry.mappings.find(
        detail::normalize(text, registry.case_sensitive));
    if (entry == registry.mappings.end())
        return CLM_BPA_FAILURE;
    *out_id = entry->second.value;
    return CLM_BPA_GET_SUCCESS;
}

inline int set(const char* name, const clm_id value)
{
    if (name == nullptr || *name == '\0')
        return CLM_BPA_FAILURE;
    const std::string text(name);
    if (!detail::valid_utf8(text))
        return CLM_BPA_FAILURE;
    auto& mappings = detail::registry().mappings;
    const auto entry = mappings.find(
        detail::normalize(text, detail::registry().case_sensitive));
    if (entry == mappings.end()) {
        try {
            mappings.emplace(detail::normalize(text, detail::registry().case_sensitive),
                             detail::mapping{value, text});
        } catch (...) {
            return CLM_BPA_FAILURE;
        }
    } else {
        entry->second.value = value;
    }
    return CLM_BPA_SET_SUCCESS;
}

inline int erase(const char* name)
{
    if (name == nullptr || *name == '\0')
        return CLM_BPA_FAILURE;
    const std::string text(name);
    if (!detail::valid_utf8(text))
        return CLM_BPA_FAILURE;
    return detail::registry().mappings.erase(
        detail::normalize(text, detail::registry().case_sensitive)) == 1
        ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

inline int unmap(const clm_id value)
{
    auto& mappings = detail::registry().mappings;
    const std::size_t before = mappings.size();
    for (auto entry = mappings.begin(); entry != mappings.end();) {
        if (entry->second.value == value)
            entry = mappings.erase(entry);
        else
            ++entry;
    }
    return mappings.size() != before ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
}

inline int unmap_name(const char* name)
{
    return erase(name);
}

inline bool is_case_sensitive()
{
    return detail::registry().case_sensitive;
}

inline int set_case_sensitive(const bool enabled)
{
    auto& registry = detail::registry();
    if (enabled == registry.case_sensitive)
        return CLM_BPA_SUCCESS;
    std::unordered_map<std::string, detail::mapping> updated;
    try {
        updated.reserve(registry.mappings.size());
        for (const auto& val : registry.mappings | std::views::values) {
            if (!updated.emplace(detail::normalize(val.spelling, enabled), val).second)
                return CLM_BPA_FAILURE;
        }
    } catch (...) {
        return CLM_BPA_FAILURE;
    }
    registry.mappings.swap(updated);
    registry.case_sensitive = enabled;
    return CLM_BPA_SUCCESS;
}

inline int clear()
{
    detail::registry().mappings.clear();
    return CLM_BPA_SUCCESS;
}

inline int shutdown()
{
    return clear();
}

inline void gen_reset()
{
    detail::registry().next_generated = 0;
    detail::registry().generator_exhausted = false;
}

inline clm_id gen_next()
{
    auto& registry = detail::registry();
    if (registry.generator_exhausted)
        return CLM_ID_NOT_FOUND;
    const clm_id result = registry.next_generated;
    if (result == CLM_ID_NOT_FOUND - 1)
        registry.generator_exhausted = true;
    else
        ++registry.next_generated;
    return result;
}

}
