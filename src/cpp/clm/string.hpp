#pragma once

#include "base.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace clm::string {

    inline constexpr clm_int failure = -1;
    inline constexpr clm_int success = 0;
    inline constexpr clm_int get_success = 1;
    inline constexpr clm_int set_success = 2;

    enum class encoding : clm_byte {
        utf8 = 1,
        latin1 = 2,
        utf16le = 3
    };

    namespace detail {
        struct string_entry {
            std::string text;
            encoding preferred = encoding::utf8;
        };

        struct array_entry {
            std::vector<std::string> values;
            std::size_t index = 0;
            std::size_t deck_size = 0;
        };

        inline std::unordered_map<clm_id, string_entry>& strings()
        {
            static std::unordered_map<clm_id, string_entry> value;
            return value;
        }

        inline std::unordered_map<clm_id, array_entry>& arrays()
        {
            static std::unordered_map<clm_id, array_entry> value;
            return value;
        }

        inline encoding& default_encoding()
        {
            static encoding value = encoding::utf8;
            return value;
        }

        inline bool supported(const encoding value)
        {
            return value == encoding::utf8 || value == encoding::latin1 ||
                   value == encoding::utf16le;
        }

        inline bool next_codepoint(
            const std::string_view text,
            std::size_t& offset,
            std::uint32_t& codepoint)
        {
            if (offset >= text.size())
                return false;
            const auto first = static_cast<unsigned char>(text[offset]);
            std::size_t length = 0;
            if (first < 0x80) {
                length = 1;
                codepoint = first;
            } else if (first >= 0xc2 && first <= 0xdf) {
                length = 2;
                codepoint = first & 0x1f;
            } else if (first >= 0xe0 && first <= 0xef) {
                length = 3;
                codepoint = first & 0x0f;
            } else if (first >= 0xf0 && first <= 0xf4) {
                length = 4;
                codepoint = first & 0x07;
            } else {
                return false;
            }
            if (length > text.size() - offset)
                return false;
            for (std::size_t i = 1; i < length; ++i) {
                const auto next = static_cast<unsigned char>(text[offset + i]);
                if ((next & 0xc0) != 0x80)
                    return false;
                if (i == 1 &&
                    ((first == 0xe0 && next < 0xa0) ||
                     (first == 0xed && next >= 0xa0) ||
                     (first == 0xf0 && next < 0x90) ||
                     (first == 0xf4 && next > 0x8f)))
                    return false;
                codepoint = (codepoint << 6) | (next & 0x3f);
            }
            if (codepoint == 0 ||
                (codepoint >= 0xd800 && codepoint <= 0xdfff) ||
                codepoint > 0x10ffff)
                return false;
            offset += length;
            return true;
        }

        inline bool measure(std::string_view text, std::size_t& count)
        {
            if (text.find('\0') != std::string_view::npos)
                return false;
            std::size_t offset = 0;
            count = 0;
            while (offset < text.size()) {
                std::uint32_t codepoint;
                if (!next_codepoint(text, offset, codepoint))
                    return false;
                ++count;
                if (count > static_cast<std::size_t>(
                        std::numeric_limits<clm_int>::max()))
                    return false;
            }
            return true;
        }

        inline bool byte_offset(
            const std::string_view text,
            const std::size_t character_index,
            std::size_t& result)
        {
            std::size_t offset = 0;
            std::size_t current = 0;
            while (current < character_index && offset < text.size()) {
                std::uint32_t codepoint;
                if (!next_codepoint(text, offset, codepoint))
                    return false;
                ++current;
            }
            if (current != character_index)
                return false;
            result = offset;
            return true;
        }

        inline bool valid_input(std::string_view text)
        {
            std::size_t count;
            return measure(text, count);
        }

        inline void append_utf8(std::string& output, const std::uint32_t cp)
        {
            if (cp < 0x80) {
                output.push_back(static_cast<char>(cp));
            } else if (cp < 0x800) {
                output.push_back(static_cast<char>(0xc0 | (cp >> 6)));
                output.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            } else if (cp < 0x10000) {
                output.push_back(static_cast<char>(0xe0 | (cp >> 12)));
                output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                output.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            } else {
                output.push_back(static_cast<char>(0xf0 | (cp >> 18)));
                output.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
                output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                output.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
            }
        }

        inline std::optional<std::vector<std::byte>> encode(
            const std::string_view text,
            const encoding target)
        {
            if (!supported(target) || !valid_input(text))
                return std::nullopt;
            std::vector<std::byte> bytes;
            bytes.reserve(text.size());
            std::size_t offset = 0;
            while (offset < text.size()) {
                std::uint32_t cp;
                if (!next_codepoint(text, offset, cp))
                    return std::nullopt;
                if (target == encoding::utf8) {
                    if (cp < 0x80) {
                        bytes.push_back(static_cast<std::byte>(cp));
                    } else if (cp < 0x800) {
                        bytes.push_back(static_cast<std::byte>(0xc0 | (cp >> 6)));
                        bytes.push_back(static_cast<std::byte>(0x80 | (cp & 0x3f)));
                    } else if (cp < 0x10000) {
                        bytes.push_back(static_cast<std::byte>(0xe0 | (cp >> 12)));
                        bytes.push_back(static_cast<std::byte>(0x80 | ((cp >> 6) & 0x3f)));
                        bytes.push_back(static_cast<std::byte>(0x80 | (cp & 0x3f)));
                    } else {
                        bytes.push_back(static_cast<std::byte>(0xf0 | (cp >> 18)));
                        bytes.push_back(static_cast<std::byte>(0x80 | ((cp >> 12) & 0x3f)));
                        bytes.push_back(static_cast<std::byte>(0x80 | ((cp >> 6) & 0x3f)));
                        bytes.push_back(static_cast<std::byte>(0x80 | (cp & 0x3f)));
                    }
                } else if (target == encoding::latin1) {
                    if (cp > 0xff)
                        return std::nullopt;
                    bytes.push_back(static_cast<std::byte>(cp));
                } else if (cp < 0x10000) {
                    bytes.push_back(static_cast<std::byte>(cp & 0xff));
                    bytes.push_back(static_cast<std::byte>(cp >> 8));
                } else {
                    cp -= 0x10000;
                    const auto high = static_cast<std::uint16_t>(0xd800 | (cp >> 10));
                    const auto low = static_cast<std::uint16_t>(0xdc00 | (cp & 0x3ff));
                    bytes.push_back(static_cast<std::byte>(high & 0xff));
                    bytes.push_back(static_cast<std::byte>(high >> 8));
                    bytes.push_back(static_cast<std::byte>(low & 0xff));
                    bytes.push_back(static_cast<std::byte>(low >> 8));
                }
            }
            if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<clm_int>::max()))
                return std::nullopt;
            return bytes;
        }

        inline std::optional<std::string> decode(
            const std::span<const std::byte> bytes,
            const encoding source)
        {
            if (!supported(source) ||
                bytes.size() > static_cast<std::size_t>(std::numeric_limits<clm_int>::max()) ||
                (source == encoding::utf16le && bytes.size() % 2 != 0))
                return std::nullopt;
            std::string result;
            if (source == encoding::utf8) {
                if (!bytes.empty())
                    result.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                if (!valid_input(result))
                    return std::nullopt;
                return result;
            }
            if (source == encoding::latin1) {
                for (const std::byte value : bytes) {
                    const auto cp = std::to_integer<unsigned char>(value);
                    if (cp == 0)
                        return std::nullopt;
                    append_utf8(result, cp);
                }
            } else {
                for (std::size_t i = 0; i < bytes.size();) {
                    const auto first = std::to_integer<unsigned char>(bytes[i]) |
                        (static_cast<std::uint32_t>(
                            std::to_integer<unsigned char>(bytes[i + 1])) << 8);
                    i += 2;
                    std::uint32_t cp = first;
                    if (first >= 0xd800 && first <= 0xdbff) {
                        if (i + 1 >= bytes.size())
                            return std::nullopt;
                        const auto tail = std::to_integer<unsigned char>(bytes[i]) |
                            (static_cast<std::uint32_t>(
                                std::to_integer<unsigned char>(bytes[i + 1])) << 8);
                        if (tail < 0xdc00 || tail > 0xdfff)
                            return std::nullopt;
                        cp = 0x10000 + ((first - 0xd800) << 10) + (tail - 0xdc00);
                        i += 2;
                    } else if (first >= 0xdc00 && first <= 0xdfff) {
                        return std::nullopt;
                    }
                    if (cp == 0)
                        return std::nullopt;
                    append_utf8(result, cp);
                }
            }
            if (result.size() > static_cast<std::size_t>(std::numeric_limits<clm_int>::max()))
                return std::nullopt;
            return result;
        }

        inline clm_int replace_all(
            std::string& text,
            const std::string_view search,
            const std::string_view replacement)
        {
            if (search.empty() || !valid_input(search) || !valid_input(replacement))
                return failure;
            constexpr auto limit = static_cast<std::size_t>(std::numeric_limits<clm_int>::max());
            if (text.size() > limit)
                return failure;
            std::size_t count = 0;
            for (std::size_t pos = 0;
                 (pos = text.find(search, pos)) != std::string::npos;
                 pos += search.size())
                ++count;
            if (count == 0)
                return 0;
            std::size_t output_size = text.size();
            if (replacement.size() >= search.size()) {
                const std::size_t growth = replacement.size() - search.size();
                if (growth != 0 && count > (limit - text.size()) / growth)
                    return failure;
                output_size += count * growth;
            } else {
                output_size -= count * (search.size() - replacement.size());
            }
            std::string result;
            result.reserve(output_size);
            std::size_t source = 0;
            while (true) {
                const auto match = text.find(search, source);
                if (match == std::string::npos)
                    break;
                result.append(text, source, match - source);
                result.append(replacement);
                source = match + search.size();
            }
            result.append(text, source, std::string::npos);
            text = std::move(result);
            return static_cast<clm_int>(count);
        }

        inline string_entry* find_string(const clm_id id)
        {
            const auto found = strings().find(id);
            return found == strings().end() ? nullptr : &found->second;
        }

        inline array_entry* find_array(const clm_id id)
        {
            const auto found = arrays().find(id);
            return found == arrays().end() ? nullptr : &found->second;
        }

        inline clm_int edit_at(
            const clm_id id,
            const clm_int index,
            const std::string_view suffix,
            const bool newline)
        {
            auto* array = find_array(id);
            if (array == nullptr || index < 0 ||
                static_cast<std::size_t>(index) >= array->values.size() ||
                !valid_input(suffix))
                return failure;
            auto& text = array->values[static_cast<std::size_t>(index)];
            std::size_t old_chars, suffix_chars;
            if (text.size() > static_cast<std::size_t>(
                    std::numeric_limits<clm_int>::max()) ||
                !measure(text, old_chars) || !measure(suffix, suffix_chars))
                return failure;
            const std::size_t extra = suffix.size() + (newline ? 1 : 0);
            if (text.size() > static_cast<std::size_t>(
                    std::numeric_limits<clm_int>::max()) - extra)
                return failure;
            text.append(suffix);
            if (newline)
                text.push_back('\n');
            return static_cast<clm_int>(suffix_chars + (newline ? 1 : 0));
        }

        inline clm_int insert_at(
            const clm_id id,
            const clm_int index,
            const clm_int character_index,
            const std::string_view inserted)
        {
            auto* array = find_array(id);
            if (array == nullptr || index < 0 || character_index < 0 ||
                static_cast<std::size_t>(index) >= array->values.size() ||
                !valid_input(inserted))
                return failure;
            auto& text = array->values[static_cast<std::size_t>(index)];
            std::size_t chars, inserted_chars, byte;
            if (text.size() > static_cast<std::size_t>(
                    std::numeric_limits<clm_int>::max()) ||
                !measure(text, chars) ||
                static_cast<std::size_t>(character_index) > chars ||
                !measure(inserted, inserted_chars) ||
                inserted.size() > static_cast<std::size_t>(
                    std::numeric_limits<clm_int>::max()) - text.size() ||
                !byte_offset(text, static_cast<std::size_t>(character_index), byte))
                return failure;
            text.insert(byte, inserted);
            return static_cast<clm_int>(inserted_chars);
        }

        inline clm_int remove_at(
            const clm_id id,
            const clm_int index,
            const clm_int character_index,
            const clm_int length)
        {
            auto* array = find_array(id);
            if (array == nullptr || index < 0 || character_index < 0 || length < 0 ||
                static_cast<std::size_t>(index) >= array->values.size())
                return failure;
            auto& text = array->values[static_cast<std::size_t>(index)];
            std::size_t chars, first, last;
            if (text.size() > static_cast<std::size_t>(
                    std::numeric_limits<clm_int>::max()) ||
                !measure(text, chars) ||
                static_cast<std::size_t>(character_index) > chars ||
                static_cast<std::size_t>(length) >
                    chars - static_cast<std::size_t>(character_index))
                return failure;
            if (length == 0)
                return 0;
            if (!byte_offset(text, static_cast<std::size_t>(character_index), first) ||
                !byte_offset(text, static_cast<std::size_t>(character_index + length), last)) // NOLINT(*-misplaced-widening-cast)
                return failure;
            text.erase(first, last - first);
            return length;
        }
    }

    inline bool encoding_supported(const encoding value)
    {
        return detail::supported(value);
    }

    inline clm_int set_default_encoding(const encoding value)
    {
        if (!detail::supported(value))
            return failure;
        detail::default_encoding() = value;
        return success;
    }

    inline encoding get_default_encoding()
    {
        return detail::default_encoding();
    }

    inline bool create(const clm_id id, const std::string_view text)
    {
        if (text.size() >= std::numeric_limits<clm_uint>::max() ||
            !detail::valid_input(text) || detail::find_string(id) != nullptr)
            return false;
        detail::strings().emplace(
            id, detail::string_entry{std::string(text), detail::default_encoding()});
        return true;
    }

    inline bool erase(const clm_id id)
    {
        return detail::strings().erase(id) != 0;
    }

    inline clm_int set(const clm_id id, const std::string_view text)
    {
        auto* value = detail::find_string(id);
        std::size_t count;
        if (value == nullptr || text.size() >= std::numeric_limits<clm_uint>::max() ||
            !detail::measure(text, count))
            return failure;
        value->text.assign(text);
        return static_cast<clm_int>(count);
    }

    inline clm_int append(const clm_id id, const std::string_view suffix)
    {
        auto* value = detail::find_string(id);
        std::size_t count;
        if (value == nullptr || value->text.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) ||
            !detail::measure(suffix, count) ||
            suffix.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) - value->text.size())
            return failure;
        value->text.append(suffix);
        return static_cast<clm_int>(count);
    }

    inline clm_int append_line(const clm_id id, const std::string_view suffix)
    {
        auto* value = detail::find_string(id);
        std::size_t count;
        if (value == nullptr || !detail::measure(suffix, count) ||
            value->text.size() >= static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) ||
            suffix.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) - value->text.size() - 1)
            return failure;
        value->text.append(suffix);
        value->text.push_back('\n');
        return static_cast<clm_int>(count + 1);
    }

    inline clm_int insert(
        const clm_id id,
        const clm_int index,
        const std::string_view inserted)
    {
        auto* value = detail::find_string(id);
        std::size_t chars, inserted_chars, byte;
        if (value == nullptr || index < 0 ||
            value->text.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) ||
            !detail::measure(value->text, chars) ||
            static_cast<std::size_t>(index) > chars ||
            !detail::measure(inserted, inserted_chars) ||
            inserted.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) - value->text.size() ||
            !detail::byte_offset(value->text, static_cast<std::size_t>(index), byte))
            return failure;
        value->text.insert(byte, inserted);
        return static_cast<clm_int>(inserted_chars);
    }

    inline clm_int remove(
        const clm_id id,
        const clm_int index,
        const clm_int length)
    {
        auto* value = detail::find_string(id);
        std::size_t chars, first, last;
        if (value == nullptr || index < 0 || length < 0 ||
            value->text.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) ||
            !detail::measure(value->text, chars) ||
            static_cast<std::size_t>(index) > chars ||
            static_cast<std::size_t>(length) >
                chars - static_cast<std::size_t>(index))
            return failure;
        if (length == 0)
            return 0;
        if (!detail::byte_offset(value->text, static_cast<std::size_t>(index), first) ||
            !detail::byte_offset(value->text,
                static_cast<std::size_t>(index) + static_cast<std::size_t>(length), last))
            return failure;
        value->text.erase(first, last - first);
        return length;
    }

    inline bool contains(const clm_id id, const std::string_view search)
    {
        const auto* value = detail::find_string(id);
        return value != nullptr && value->text.size() <= static_cast<std::size_t>(
                   std::numeric_limits<clm_int>::max()) &&
               detail::valid_input(search) &&
               value->text.find(search) != std::string::npos;
    }

    inline clm_int length(const clm_id id)
    {
        const auto* value = detail::find_string(id);
        std::size_t count;
        return value != nullptr && value->text.size() <= static_cast<std::size_t>(
                   std::numeric_limits<clm_int>::max()) &&
               detail::measure(value->text, count)
            ? static_cast<clm_int>(count) : failure;
    }

    inline clm_int replace(
        const clm_id id,
        const std::string_view search,
        const std::string_view replacement)
    {
        auto* value = detail::find_string(id);
        return value == nullptr ? failure
            : detail::replace_all(value->text, search, replacement);
    }

    inline std::optional<std::vector<std::byte>> get_bytes(const clm_id id)
    {
        const auto* value = detail::find_string(id);
        return value == nullptr ? std::nullopt
            : detail::encode(value->text, value->preferred);
    }

    inline clm_int get_bytes(const clm_id id, const std::span<std::byte> output)
    {
        const auto encoded = get_bytes(id);
        if (!encoded || encoded->size() >
                static_cast<std::size_t>(std::numeric_limits<clm_int>::max()))
            return failure;
        if (output.empty())
            return static_cast<clm_int>(encoded->size());
        if (output.size() < encoded->size())
            return failure;
        std::ranges::copy(*encoded, output.begin());
        return static_cast<clm_int>(encoded->size());
    }

    inline clm_int from_bytes(
        const clm_id id,
        const encoding source,
        const std::span<const std::byte> bytes)
    {
        auto decoded = detail::decode(bytes, source);
        if (!decoded)
            return failure;
        auto* existing = detail::find_string(id);
        if (existing != nullptr) {
            existing->text = std::move(*decoded);
            existing->preferred = source;
        } else {
            detail::strings().emplace(
                id, detail::string_entry{std::move(*decoded), source});
        }
        return success;
    }

    inline clm_int change_encoding(const clm_id id, const encoding target)
    {
        auto* value = detail::find_string(id);
        if (value == nullptr || !detail::supported(target) ||
            !detail::encode(value->text, target))
            return failure;
        value->preferred = target;
        return success;
    }

    inline clm_int slice(
        const clm_id source_id,
        const clm_int start,
        const clm_int count,
        const clm_id destination_id)
    {
        const auto* source = detail::find_string(source_id);
        std::size_t characters, first, last;
        if (source == nullptr || start < 0 || count < 0 ||
            source->text.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) ||
            detail::find_string(destination_id) != nullptr ||
            !detail::measure(source->text, characters) ||
            static_cast<std::size_t>(start) > characters ||
            static_cast<std::size_t>(count) >
                characters - static_cast<std::size_t>(start) ||
            !detail::byte_offset(source->text, static_cast<std::size_t>(start), first) ||
            !detail::byte_offset(source->text,
                static_cast<std::size_t>(start) + static_cast<std::size_t>(count), last))
            return failure;
        const encoding preference = source->preferred;
        const std::string text = source->text.substr(first, last - first);
        if (!create(destination_id, text) ||
            change_encoding(destination_id, preference) != success) {
            erase(destination_id);
            return failure;
        }
        return success;
    }

    inline std::optional<std::string_view> slice_borrow(
        const clm_id source_id,
        const clm_int start,
        const clm_int count)
    {
        const auto* source = detail::find_string(source_id);
        std::size_t characters, first, last;
        if (source == nullptr || start < 0 || count < 0 ||
            source->text.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) ||
            !detail::measure(source->text, characters) ||
            static_cast<std::size_t>(start) > characters ||
            static_cast<std::size_t>(count) >
                characters - static_cast<std::size_t>(start) ||
            !detail::byte_offset(source->text, static_cast<std::size_t>(start), first) ||
            !detail::byte_offset(source->text,
                static_cast<std::size_t>(start) + static_cast<std::size_t>(count), last))
            return std::nullopt;
        return std::string_view(source->text).substr(first, last - first);
    }

    inline const std::string* value(const clm_id id)
    {
        const auto* found = detail::find_string(id);
        return found == nullptr ? nullptr : &found->text;
    }

    inline std::optional<encoding> preferred_encoding(const clm_id id)
    {
        const auto* found = detail::find_string(id);
        return found == nullptr ? std::nullopt
            : std::optional<encoding>(found->preferred);
    }

    inline bool array_create(const clm_id id, const clm_int reserved = 0)
    {
        if (reserved < 0 || detail::find_array(id) != nullptr)
            return false;
        detail::array_entry array;
        array.values.reserve(static_cast<std::size_t>(reserved));
        detail::arrays().emplace(id, std::move(array));
        return true;
    }

    inline bool array_erase(const clm_id id)
    {
        return detail::arrays().erase(id) != 0;
    }

    inline clm_int array_length_at(const clm_id id, const clm_int index)
    {
        auto* array = detail::find_array(id);
        std::size_t count;
        if (array == nullptr || index < 0 ||
            static_cast<std::size_t>(index) >= array->values.size() ||
            !detail::measure(array->values[static_cast<std::size_t>(index)], count))
            return failure;
        return static_cast<clm_int>(count);
    }

    inline bool array_contains_at(
        const clm_id id,
        const clm_int index,
        const std::string_view search)
    {
        auto* array = detail::find_array(id);
        return array != nullptr && index >= 0 &&
            static_cast<std::size_t>(index) < array->values.size() &&
            detail::valid_input(search) &&
            array->values[static_cast<std::size_t>(index)].find(search) !=
                std::string::npos;
    }

    inline clm_int array_append_at(
        const clm_id id,
        const clm_int index,
        const std::string_view suffix)
    {
        return detail::edit_at(id, index, suffix, false);
    }

    inline clm_int array_append_line_at(
        const clm_id id,
        const clm_int index,
        const std::string_view suffix)
    {
        return detail::edit_at(id, index, suffix, true);
    }

    inline clm_int array_insert_at(
        const clm_id id,
        const clm_int index,
        const clm_int character_index,
        const std::string_view inserted)
    {
        return detail::insert_at(id, index, character_index, inserted);
    }

    inline clm_int array_remove_at(
        const clm_id id,
        const clm_int index,
        const clm_int character_index,
        const clm_int count)
    {
        return detail::remove_at(id, index, character_index, count);
    }

    inline clm_int array_replace_at(
        const clm_id id,
        const clm_int index,
        const std::string_view search,
        const std::string_view replacement)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || index < 0 ||
            static_cast<std::size_t>(index) >= array->values.size())
            return failure;
        return detail::replace_all(
            array->values[static_cast<std::size_t>(index)], search, replacement);
    }

    inline clm_int array_count(const clm_id id)
    {
        const auto* array = detail::find_array(id);
        return array == nullptr ||
            array->values.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max())
            ? failure : static_cast<clm_int>(array->values.size());
    }

    inline clm_int array_get(
        const clm_id id,
        const clm_int index,
        const clm_id destination_id)
    {
        auto* array = detail::find_array(id);
        auto* destination = detail::find_string(destination_id);
        if (array == nullptr || destination == nullptr || index < 0 ||
            static_cast<std::size_t>(index) >= array->values.size())
            return failure;
        destination->text = array->values[static_cast<std::size_t>(index)];
        destination->preferred = encoding::utf8;
        return get_success;
    }

    inline clm_int array_set(
        const clm_id id,
        const clm_int index,
        const clm_id source_id)
    {
        auto* array = detail::find_array(id);
        const auto* source = detail::find_string(source_id);
        if (array == nullptr || source == nullptr ||
            source->text.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) ||
            index < 0 ||
            static_cast<std::size_t>(index) >= array->values.size())
            return failure;
        array->values[static_cast<std::size_t>(index)] = source->text;
        array->deck_size = array->values.size();
        return set_success;
    }

    inline clm_int array_insert(
        const clm_id id,
        const clm_int index,
        const clm_id source_id)
    {
        auto* array = detail::find_array(id);
        const auto* source = detail::find_string(source_id);
        if (array == nullptr || source == nullptr ||
            source->text.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()) ||
            index < 0 ||
            static_cast<std::size_t>(index) > array->values.size() ||
            array->values.size() >= static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()))
            return failure;
        array->values.insert(array->values.begin() + index, source->text);
        array->index = static_cast<std::size_t>(index);
        array->deck_size = array->values.size();
        return success;
    }

    inline clm_int array_append(const clm_id id, const clm_id source_id)
    {
        const auto* array = detail::find_array(id);
        if (array == nullptr || array->values.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()))
            return failure;
        return array_insert(id, static_cast<clm_int>(array->values.size()), source_id);
    }

    inline clm_int array_remove(const clm_id id, const clm_int index)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || index < 0 ||
            static_cast<std::size_t>(index) >= array->values.size())
            return failure;
        array->values.erase(array->values.begin() + index);
        if (array->values.empty())
            array->index = 0;
        else if (array->index > static_cast<std::size_t>(index))
            --array->index;
        else if (array->index >= array->values.size())
            array->index = array->values.size() - 1;
        array->deck_size = array->values.size();
        return success;
    }

    inline clm_int array_clear(const clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr)
            return failure;
        array->values.clear();
        array->index = 0;
        array->deck_size = 0;
        return success;
    }

    inline clm_int array_find(const clm_id id, const clm_id source_id)
    {
        const auto* array = detail::find_array(id);
        const auto* source = detail::find_string(source_id);
        if (array == nullptr || source == nullptr ||
            source->text.size() > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max()))
            return failure;
        const auto found = std::ranges::find(array->values, source->text);
        if (found == array->values.end())
            return failure;
        return static_cast<clm_int>(found - array->values.begin());
    }

    inline clm_int array_sort(const clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr)
            return failure;
        std::ranges::sort(array->values);
        array->index = 0;
        array->deck_size = array->values.size();
        return success;
    }

    inline clm_int array_shuffle(const clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr)
            return failure;
        static std::mt19937 generator(std::random_device{}());
        std::ranges::shuffle(array->values, generator);
        array->index = 0;
        array->deck_size = array->values.size();
        return success;
    }

    inline clm_int array_move_first(const clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr)
            return failure;
        array->index = 0;
        return success;
    }

    inline clm_int array_move_previous(const clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->index == 0)
            return failure;
        --array->index;
        return success;
    }

    inline clm_int array_move_next(const clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->index + 1 >= array->values.size())
            return failure;
        ++array->index;
        return success;
    }

    inline clm_int array_move_last(const clm_id id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || array->values.empty())
            return failure;
        array->index = array->values.size() - 1;
        return success;
    }

    inline clm_int array_move_to(const clm_id id, const clm_int index)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || index < 0 ||
            static_cast<std::size_t>(index) >= array->values.size())
            return failure;
        array->index = static_cast<std::size_t>(index);
        return success;
    }

    inline clm_int array_get_current(const clm_id id, const clm_id destination_id)
    {
        auto* array = detail::find_array(id);
        return array == nullptr || array->index > static_cast<std::size_t>(
                std::numeric_limits<clm_int>::max())
            ? failure : array_get(id, static_cast<clm_int>(array->index), destination_id);
    }

    inline clm_int array_draw_at(
        const clm_id id,
        const clm_int deck_index,
        const clm_id destination_id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr || deck_index < 0 ||
            static_cast<std::size_t>(deck_index) >= array->deck_size ||
            array_get(id, deck_index, destination_id) != get_success)
            return failure;
        const auto last = array->deck_size - 1;
        std::swap(array->values[static_cast<std::size_t>(deck_index)],
                  array->values[last]);
        --array->deck_size;
        return get_success;
    }

    inline clm_int array_draw(const clm_id id, const clm_id destination_id)
    {
        auto* array = detail::find_array(id);
        if (array == nullptr)
            return failure;
        if (array->deck_size == 0 && array_shuffle(id) != success)
            return failure;
        return array_draw_at(id, 0, destination_id);
    }
}
