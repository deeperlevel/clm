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

#pragma once

#include "base.hpp"
#include "dictionary.hpp"
#include "id.hpp"
#include "bpa.hpp"

#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#define CLM_CLP_OPERATION_SUCCESS          0
#define CLM_CLP_OPERATION_FAILURE         -1
#define CLM_CLP_OPERATION_INVALID_ARGUMENT -2
#define CLM_CLP_OPERATION_INVALID_SYNTAX   -3
#define CLM_CLP_OPERATION_DUPLICATE_ID     -4

namespace clm::clp {

inline constexpr clm_int SUCCESS = CLM_CLP_OPERATION_SUCCESS;
inline constexpr clm_int FAILURE = CLM_CLP_OPERATION_FAILURE;
inline constexpr clm_int INVALID_ARGUMENT = CLM_CLP_OPERATION_INVALID_ARGUMENT;
inline constexpr clm_int INVALID_SYNTAX = CLM_CLP_OPERATION_INVALID_SYNTAX;
inline constexpr clm_int DUPLICATE_ID = CLM_CLP_OPERATION_DUPLICATE_ID;

namespace detail {

inline bool is_space(const unsigned char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' ||
           value == '\v' || value == '\f';
}

inline bool is_option_token(const char* token)
{
    if (token == nullptr || token[0] == '\0')
        return false;
    if (std::strchr(token, '=') != nullptr)
        return true;
    if (token[0] != '-')
        return false;
    if (token[1] == '-')
        return token[2] != '\0';
    if (token[1] == '\0')
        return false;
    return !((token[1] >= '0' && token[1] <= '9') ||
             (token[1] == '.' && token[2] >= '0' && token[2] <= '9'));
}

inline clm_int resolve_key(
    const std::string& name,
    std::vector<std::string>& registered,
    clm_id& out_id)
{
    if (name.empty())
        return INVALID_SYNTAX;
    if (clm::id::try_get(name.c_str(), &out_id) == clm::id::CLM_BPA_GET_SUCCESS)
        return SUCCESS;
    for (;;) {
        const clm_id candidate = clm::id::gen_next();
        std::size_t name_length = 0;
        if (candidate == clm::id::CLM_ID_NOT_FOUND)
            return FAILURE;
        if (clm::id::get_name(candidate, nullptr, 0, &name_length) == clm::id::CLM_BPA_GET_SUCCESS)
            continue;
        registered.push_back(name);
        if (clm::id::add(name.c_str(), candidate) != clm::id::CLM_BPA_SET_SUCCESS) {
            registered.pop_back();
            return FAILURE;
        }
        out_id = candidate;
        return SUCCESS;
    }
}

inline clm_int set_value(
    const clm_id dictionary_id,
    const std::string& name,
    const std::string& value,
    std::vector<std::string>& registered)
{
    clm_id key = 0;
    const clm_int key_status = resolve_key(name, registered, key);
    if (key_status != SUCCESS)
        return key_status;
    return clm::dictionary::set_from(dictionary_id, key, value.c_str(), value.size() + 1) ==
           clm::dictionary::CLM_BPA_SET_SUCCESS ? SUCCESS : FAILURE;
}

inline clm_int tokenize(
    const char* command_line,
    std::vector<std::string>& tokens)
{
    std::string buffer;
    char quote = '\0';
    bool escaped = false;
    bool token_started = false;

    for (std::size_t index = 0;; ++index) {
        const unsigned char current = static_cast<unsigned char>(command_line[index]);
        if (current == '\0' || (quote == '\0' && !escaped && is_space(current))) {
            if (escaped || quote != '\0')
                return INVALID_SYNTAX;
            if (token_started) {
                tokens.push_back(std::move(buffer));
                buffer.clear();
                token_started = false;
            }
            if (current == '\0')
                break;
            continue;
        }
        if (escaped) {
            buffer.push_back(static_cast<char>(current));
            escaped = false;
            token_started = true;
        } else if (current == '\\') {
            escaped = true;
            token_started = true;
        } else if (quote != '\0') {
            if (current == static_cast<unsigned char>(quote))
                quote = '\0';
            else
                buffer.push_back(static_cast<char>(current));
        } else if (current == '\'' || current == '"') {
            quote = static_cast<char>(current);
            token_started = true;
        } else {
            buffer.push_back(static_cast<char>(current));
            token_started = true;
        }
    }
    return SUCCESS;
}

inline clm_int parse_arguments(
    const clm_int count,
    const char* const arguments[],
    const clm_id output_dictionary_id)
{
    if (count < 0 || (count != 0 && arguments == nullptr))
        return INVALID_ARGUMENT;
    if (clm::bpa::block_exists(output_dictionary_id) ||
        clm::bpa::array_exists(output_dictionary_id) ||
        clm::dictionary::detail::dictionaries().contains(output_dictionary_id))
        return DUPLICATE_ID;

    if (clm::dictionary::create(output_dictionary_id, clm::dictionary::CLM_BPA_TYPE_STRING, 0) !=
        clm::dictionary::CLM_BPA_SUCCESS)
        return FAILURE;

    std::vector<std::string> registered;
    clm_int status = SUCCESS;

    for (clm_int index = 0; index < count; ++index) {
        const char* token = arguments[index];
        if (token == nullptr || token[0] == '\0') {
            status = INVALID_SYNTAX;
            break;
        }
        const char* name_ptr = token;
        while (*name_ptr == '-')
            ++name_ptr;
        if (name_ptr == token && std::strchr(token, '=') == nullptr) {
            status = INVALID_SYNTAX;
            break;
        }
        const char* equals = std::strchr(name_ptr, '=');
        std::string name;
        std::string value;

        if (equals != nullptr) {
            const std::size_t name_length = static_cast<std::size_t>(equals - name_ptr);
            if (name_length == 0) {
                status = INVALID_SYNTAX;
                break;
            }
            name.assign(name_ptr, name_length);
            value = equals + 1;
        } else if (index + 1 < count && !is_option_token(arguments[index + 1])) {
            name = name_ptr;
            value = arguments[++index];
        } else {
            name = name_ptr;
            value = "true";
        }

        status = set_value(output_dictionary_id, name, value, registered);
        if (status != SUCCESS)
            break;
    }

    if (status != SUCCESS) {
        clm::dictionary::erase(output_dictionary_id);
        for (const auto& reg_name : registered)
            clm::id::erase(reg_name.c_str());
        return status;
    }

    return SUCCESS;
}

} // namespace detail

inline clm_int parse_tokens(
    const clm_int count,
    const char* const tokens[],
    const clm_id output_dictionary_id)
{
    return detail::parse_arguments(count, tokens, output_dictionary_id);
}

inline clm_int parse_argv(
    const clm_int argc,
    const char* const argv[],
    const clm_id output_dictionary_id)
{
    if (argc < 0 || (argc > 0 && argv == nullptr))
        return INVALID_ARGUMENT;
    const clm_int count = argc > 0 ? argc - 1 : 0;
    const char* const* arguments = argc > 1 ? argv + 1 : nullptr;
    return detail::parse_arguments(count, arguments, output_dictionary_id);
}

inline clm_int parse_string(
    const char* command_line,
    const clm_id output_dictionary_id)
{
    if (command_line == nullptr)
        return INVALID_ARGUMENT;
    std::vector<std::string> tokens;
    const clm_int token_status = detail::tokenize(command_line, tokens);
    if (token_status != SUCCESS)
        return token_status;
    if (tokens.size() > static_cast<std::size_t>(INT_MAX))
        return INVALID_ARGUMENT;

    std::vector<const char*> token_ptrs;
    token_ptrs.reserve(tokens.size());
    for (const auto& t : tokens)
        token_ptrs.push_back(t.c_str());

    return detail::parse_arguments(static_cast<clm_int>(token_ptrs.size()),
                                   token_ptrs.data(),
                                   output_dictionary_id);
}

} // namespace clm::clp

static inline clm_int clm_clp_parse_tokens(
    const clm_int count,
    const char* const tokens[],
    const clm_id output_dictionary_id)
{
    return clm::clp::parse_tokens(count, tokens, output_dictionary_id);
}

static inline clm_int clm_clp_parse_argv(
    const clm_int argc,
    const char* const argv[],
    const clm_id output_dictionary_id)
{
    return clm::clp::parse_argv(argc, argv, output_dictionary_id);
}

static inline clm_int clm_clp_parse_string(
    const char* command_line,
    const clm_id output_dictionary_id)
{
    return clm::clp::parse_string(command_line, output_dictionary_id);
}

