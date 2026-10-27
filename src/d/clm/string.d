module clm.string;

import std.random : uniform;
import std.string : fromStringz, toStringz;
import clm.base : IdKey, clm_byte, clm_int, clm_uint;
import clm.bpa;

enum clm_byte CLM_STRING_ENCODING_UTF8 = 1;
enum clm_byte CLM_STRING_ENCODING_LATIN1 = 2;
enum clm_byte CLM_STRING_ENCODING_UTF16LE = 3;

clm_byte clm_string_global_default_encoding = CLM_STRING_ENCODING_UTF8;

private enum int maxStringLength = int.max;

bool clm_string_encoding_supported(clm_byte encoding)
{
    return encoding == CLM_STRING_ENCODING_UTF8 ||
        encoding == CLM_STRING_ENCODING_LATIN1 ||
        encoding == CLM_STRING_ENCODING_UTF16LE;
}

int clm_string_set_default_encoding(clm_byte encoding)
{
    if (!clm_string_encoding_supported(encoding))
        return CLM_BPA_FAILURE;
    clm_string_global_default_encoding = encoding;
    return CLM_BPA_SUCCESS;
}

clm_byte clm_string_get_default_encoding()
{
    return clm_string_global_default_encoding;
}

private bool nextUtf8(const(ubyte)[] text, ref size_t offset, out uint scalar)
{
    auto start = offset;
    scalar = 0;
    if (start >= text.length)
        return false;
    auto first = text[start];
    if (first < 0x80)
    {
        scalar = first;
        offset = start + 1;
        return true;
    }

    size_t sequenceLength;
    if (first >= 0xC2 && first <= 0xDF)
    {
        sequenceLength = 2;
        scalar = first & 0x1F;
    }
    else if (first >= 0xE0 && first <= 0xEF)
    {
        sequenceLength = 3;
        scalar = first & 0x0F;
    }
    else if (first >= 0xF0 && first <= 0xF4)
    {
        sequenceLength = 4;
        scalar = first & 0x07;
    }
    else
        return false;

    if (sequenceLength > text.length - start)
        return false;
    foreach (index; 1 .. sequenceLength)
    {
        auto continuation = text[start + index];
        if ((continuation & 0xC0) != 0x80)
            return false;
        scalar = (scalar << 6) | (continuation & 0x3F);
    }
    if ((sequenceLength == 3 && scalar < 0x800) ||
        (sequenceLength == 4 && scalar < 0x10000) ||
        (scalar >= 0xD800 && scalar <= 0xDFFF) ||
        scalar > 0x10FFFF)
        return false;
    offset = start + sequenceLength;
    return true;
}

private bool measure(string text, out size_t characterCount)
{
    characterCount = 0;
    size_t offset;
    auto bytes = cast(const(ubyte)[])text;
    while (offset < bytes.length)
    {
        uint scalar;
        if (!nextUtf8(bytes, offset, scalar))
            return false;
        ++characterCount;
        if (characterCount > maxStringLength)
            return false;
    }
    return true;
}

private bool byteOffset(string text, size_t characterIndex, out size_t result)
{
    result = 0;
    size_t currentIndex;
    auto bytes = cast(const(ubyte)[])text;
    while (currentIndex < characterIndex && result < bytes.length)
    {
        uint scalar;
        if (!nextUtf8(bytes, result, scalar))
            return false;
        ++currentIndex;
    }
    return currentIndex == characterIndex;
}

private bool containsNul(string text)
{
    foreach (character; text)
        if (character == '\0')
            return true;
    return false;
}

private bool getString(IdKey id, out clm_bpa_block_node block, out string text, out size_t characters)
{
    block = clm_bpa_find_block(id);
    text = null;
    characters = 0;
    if (block is null || block.type != CLM_BPA_TYPE_STRING ||
        block.length > maxStringLength ||
        (block.length != 0 && block.data is null))
        return false;

    if (block.length == 0)
        text = "";
    else
        text = (cast(const(char)*)block.data)[0 .. block.length].idup;
    if (containsNul(text) || !measure(text, characters))
        return false;
    return true;
}

private int replaceString(IdKey id, string text)
{
    if (containsNul(text) || text.length >= clm_uint.max)
        return CLM_BPA_FAILURE;
    return clm_bpa_replace(id, cast(const(ubyte)[])(text ~ '\0')) < 0 ? CLM_BPA_FAILURE : CLM_BPA_SUCCESS;
}

private string readArrayString(clm_bpaa_array_node array, size_t index)
{
    auto value = array.strings[index];
    if (value is null)
        return null;
    return cast(string)value.idup;
}

private int getStringArray(IdKey id, out clm_bpaa_array_node array)
{
    array = clm_bpaa_find_array(id);
    if (array is null || array.type != CLM_BPA_TYPE_STRING ||
        array.encoding != CLM_STRING_ENCODING_UTF8)
        return CLM_BPA_FAILURE;
    return CLM_BPA_SUCCESS;
}

private bool validateStringBlock(IdKey id)
{
    auto block = clm_bpa_find_block(id);
    string text;
    size_t characters;
    return block !is null && block.type == CLM_BPA_TYPE_STRING &&
        block.length <= maxStringLength &&
        (block.length == 0 || block.data !is null) &&
        getString(id, block, text, characters);
}

private int replaceArrayString(clm_bpaa_array_node array, clm_int index, string text)
{
    size_t characters;
    if (index < 0 || cast(clm_uint)index >= array.count ||
        text.length > maxStringLength || containsNul(text) ||
        !measure(text, characters))
        return CLM_BPA_FAILURE;
    array.strings[index] = cast(ubyte[])text.dup;
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

private size_t findBytes(string text, string search, size_t from = 0)
{
    if (search.length == 0)
        return from <= text.length ? from : size_t.max;
    if (from > text.length || search.length > text.length - from)
        return size_t.max;
    for (size_t index = from; index <= text.length - search.length; ++index)
        if (text[index .. index + search.length] == search)
            return index;
    return size_t.max;
}

bool clm_string_new(IdKey id, string text)
{
    size_t characters;
    if (text is null || containsNul(text) || !measure(text, characters) ||
        text.length >= clm_uint.max ||
        clm_bpa_create(id, CLM_BPA_TYPE_STRING) != CLM_BPA_SUCCESS)
        return false;
    auto block = clm_bpa_find_block(id);
    if (block is null)
    {
        clm_bpa_delete(id);
        return false;
    }
    block.encoding = clm_string_get_default_encoding();
    if (clm_bpa_set_string(id, cast(const(ubyte)[])(text ~ '\0')) < 0)
    {
        clm_bpa_delete(id);
        return false;
    }
    return true;
}

bool clm_string_delete(IdKey id)
{
    auto block = clm_bpa_find_block(id);
    return block !is null && block.type == CLM_BPA_TYPE_STRING &&
        clm_bpa_delete(id) == CLM_BPA_SUCCESS;
}

clm_int clm_string_set(IdKey id, string text)
{
    size_t characters;
    auto block = clm_bpa_find_block(id);
    if (text is null || containsNul(text) || !measure(text, characters) ||
        text.length >= clm_uint.max || block is null ||
        block.type != CLM_BPA_TYPE_STRING ||
        clm_bpa_replace(id, cast(const(ubyte)[])(text ~ '\0')) < 0)
        return CLM_BPA_FAILURE;
    return cast(clm_int)characters;
}

clm_int clm_string_append(IdKey id, string suffix)
{
    auto block = clm_bpa_find_block(id);
    string current;
    size_t currentCharacters, suffixCharacters;
    if (suffix is null || containsNul(suffix) ||
        !getString(id, block, current, currentCharacters) ||
        !measure(suffix, suffixCharacters) ||
        suffix.length > maxStringLength - current.length)
        return CLM_BPA_FAILURE;
    if (replaceString(id, current ~ suffix) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return cast(clm_int)suffixCharacters;
}

clm_int clm_string_append_line(IdKey id, string suffix)
{
    auto block = clm_bpa_find_block(id);
    string current;
    size_t currentCharacters, suffixCharacters;
    if (suffix is null || containsNul(suffix) ||
        !getString(id, block, current, currentCharacters) ||
        !measure(suffix, suffixCharacters) ||
        current.length >= maxStringLength ||
        suffix.length > maxStringLength - current.length - 1)
        return CLM_BPA_FAILURE;
    if (replaceString(id, current ~ suffix ~ "\n") != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return cast(clm_int)(suffixCharacters + 1);
}

clm_int clm_string_insert(IdKey id, clm_int index, string inserted)
{
    auto block = clm_bpa_find_block(id);
    string current;
    size_t currentCharacters, insertedCharacters, offset;
    if (index < 0 || inserted is null || containsNul(inserted) ||
        !getString(id, block, current, currentCharacters) ||
        cast(size_t)index > currentCharacters ||
        !measure(inserted, insertedCharacters) ||
        !byteOffset(current, cast(size_t)index, offset) ||
        inserted.length > maxStringLength - current.length)
        return CLM_BPA_FAILURE;
    auto result = current[0 .. offset] ~ inserted ~ current[offset .. $];
    if (replaceString(id, result) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return cast(clm_int)insertedCharacters;
}

clm_int clm_string_remove(IdKey id, clm_int index, clm_int length)
{
    auto block = clm_bpa_find_block(id);
    string current;
    size_t characters, start, end;
    if (index < 0 || length < 0 ||
        !getString(id, block, current, characters) ||
        cast(size_t)index > characters ||
        cast(size_t)length > characters - cast(size_t)index)
        return CLM_BPA_FAILURE;
    if (length == 0)
        return 0;
    if (!byteOffset(current, cast(size_t)index, start) ||
        !byteOffset(current, cast(size_t)index + cast(size_t)length, end))
        return CLM_BPA_FAILURE;
    auto result = current[0 .. start] ~ current[end .. $];
    return replaceString(id, result) == CLM_BPA_SUCCESS ? length : CLM_BPA_FAILURE;
}

bool clm_string_contains(IdKey id, string search)
{
    auto block = clm_bpa_find_block(id);
    string current;
    size_t characters, searchCharacters;
    if (search is null || containsNul(search) ||
        !getString(id, block, current, characters) ||
        !measure(search, searchCharacters))
        return false;
    return findBytes(current, search) != size_t.max;
}

clm_int clm_string_length(IdKey id)
{
    auto block = clm_bpa_find_block(id);
    string current;
    size_t characters;
    return getString(id, block, current, characters)
        ? cast(clm_int)characters : CLM_BPA_FAILURE;
}

clm_int clm_string_replace(IdKey id, string search, string replacement)
{
    auto block = clm_bpa_find_block(id);
    string current;
    size_t characters, searchCharacters, replacementCharacters;
    if (search is null || replacement is null || search.length == 0 ||
        containsNul(search) || containsNul(replacement) ||
        !getString(id, block, current, characters) ||
        !measure(search, searchCharacters) ||
        !measure(replacement, replacementCharacters))
        return CLM_BPA_FAILURE;

    size_t count;
    auto offset = findBytes(current, search);
    if (offset == size_t.max)
        return 0;
    string result;
    size_t copied;
    while (offset != size_t.max)
    {
        result ~= current[copied .. offset] ~ replacement;
        copied = offset + search.length;
        ++count;
        offset = findBytes(current, search, copied);
    }
    result ~= current[copied .. $];
    if (result.length > maxStringLength ||
        replaceString(id, result) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return cast(clm_int)count;
}

private bool decodeBytes(clm_byte encoding, const(ubyte)[] input, out string output)
{
    output = null;
    if (!clm_string_encoding_supported(encoding) || input.length > maxStringLength)
        return false;
    if (encoding == CLM_STRING_ENCODING_UTF8)
    {
        output = cast(string)input.idup;
        size_t characters;
        return !containsNul(output) && measure(output, characters);
    }
    if (encoding == CLM_STRING_ENCODING_LATIN1)
    {
        foreach (value; input)
        {
            if (value == 0)
                return false;
            if (value < 0x80)
                output ~= cast(char)value;
            else
                {
                output ~= cast(char)(0xC0 | (value >> 6));
                output ~= cast(char)(0x80 | (value & 0x3F));
            }
        }
        return true;
    }
    if ((input.length & 1) != 0)
        return false;
    size_t index;
    while (index < input.length)
    {
        uint scalar = input[index] | (cast(uint)input[index + 1] << 8);
        index += 2;
        if (scalar == 0)
            return false;
        if (scalar >= 0xD800 && scalar <= 0xDBFF)
        {
            if (index + 1 >= input.length)
                return false;
            auto tail = input[index] | (cast(uint)input[index + 1] << 8);
            if (tail < 0xDC00 || tail > 0xDFFF)
                return false;
            scalar = 0x10000 + ((scalar - 0xD800) << 10) + (tail - 0xDC00);
            index += 2;
        }
        else if (scalar >= 0xDC00 && scalar <= 0xDFFF)
            return false;
        output ~= encodeScalar(scalar);
        if (output.length > maxStringLength)
            return false;
    }
    return true;
}

private string encodeScalar(uint scalar)
{
    if (scalar < 0x80)
        return [cast(char)scalar].idup;
    if (scalar < 0x800)
        return [cast(char)(0xC0 | (scalar >> 6)),
            cast(char)(0x80 | (scalar & 0x3F))].idup;
    if (scalar < 0x10000)
        return [cast(char)(0xE0 | (scalar >> 12)),
            cast(char)(0x80 | ((scalar >> 6) & 0x3F)),
            cast(char)(0x80 | (scalar & 0x3F))].idup;
    return [cast(char)(0xF0 | (scalar >> 18)),
        cast(char)(0x80 | ((scalar >> 12) & 0x3F)),
        cast(char)(0x80 | ((scalar >> 6) & 0x3F)),
        cast(char)(0x80 | (scalar & 0x3F))].idup;
}

clm_int clm_string_get_bytes(IdKey id, ubyte[] buffer)
{
    auto block = clm_bpa_find_block(id);
    string text;
    size_t characters;
    if (!getString(id, block, text, characters) ||
        !clm_string_encoding_supported(block.encoding))
        return CLM_BPA_FAILURE;
    size_t required;
    size_t offset;
    auto bytes = cast(const(ubyte)[])text;
    while (offset < bytes.length)
    {
        uint scalar;
        if (!nextUtf8(bytes, offset, scalar) || scalar == 0)
            return CLM_BPA_FAILURE;
        if (block.encoding == CLM_STRING_ENCODING_UTF8)
            required += offset - (offset - (scalar < 0x80 ? 1 :
                scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4));
        else if (block.encoding == CLM_STRING_ENCODING_LATIN1)
        {
            if (scalar > 255)
                return CLM_BPA_FAILURE;
            ++required;
        }
        else
            required += scalar < 0x10000 ? 2 : 4;
        if (required > maxStringLength)
            return CLM_BPA_FAILURE;
    }
    if (buffer.length == 0)
        return cast(clm_int)required;
    if (buffer.length < required)
        return CLM_BPA_FAILURE;

    offset = 0;
    size_t position;
    while (offset < bytes.length)
    {
        uint scalar;
        if (!nextUtf8(bytes, offset, scalar))
            return CLM_BPA_FAILURE;
        if (block.encoding == CLM_STRING_ENCODING_UTF8)
        {
            auto sequenceLength = scalar < 0x80 ? 1 :
                scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
            auto begin = offset - sequenceLength;
            foreach (index; begin .. offset)
                buffer[position++] = bytes[index];
        }
        else if (block.encoding == CLM_STRING_ENCODING_LATIN1)
            buffer[position++] = cast(ubyte)scalar;
        else if (scalar < 0x10000)
        {
            buffer[position++] = cast(ubyte)scalar;
            buffer[position++] = cast(ubyte)(scalar >> 8);
        }
        else
        {
            scalar -= 0x10000;
            auto high = 0xD800 | (scalar >> 10);
            auto low = 0xDC00 | (scalar & 0x3FF);
            buffer[position++] = cast(ubyte)high;
            buffer[position++] = cast(ubyte)(high >> 8);
            buffer[position++] = cast(ubyte)low;
            buffer[position++] = cast(ubyte)(low >> 8);
        }
    }
    return cast(clm_int)required;
}

int clm_string_from_bytes(IdKey id, clm_byte encoding, const(ubyte)[] bytes)
{
    string decoded;
    if (!decodeBytes(encoding, bytes, decoded) ||
        decoded.length >= clm_uint.max)
        return CLM_BPA_FAILURE;
    auto block = clm_bpa_find_block(id);
    bool created;
    if (block is null)
    {
        if (clm_bpa_create(id, CLM_BPA_TYPE_STRING) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        created = true;
        block = clm_bpa_find_block(id);
    }
    if (block is null || block.type != CLM_BPA_TYPE_STRING ||
        clm_bpa_set_string(id, cast(const(ubyte)[])(decoded ~ '\0')) < 0)
    {
        if (created)
            clm_bpa_delete(id);
        return CLM_BPA_FAILURE;
    }
    block.encoding = encoding;
    return CLM_BPA_SUCCESS;
}

int clm_string_change_encoding(IdKey id, clm_byte newEncoding)
{
    auto block = clm_bpa_find_block(id);
    if (block is null || block.type != CLM_BPA_TYPE_STRING ||
        !clm_string_encoding_supported(newEncoding))
        return CLM_BPA_FAILURE;
    auto previousEncoding = block.encoding;
    block.encoding = newEncoding;
    if (clm_string_get_bytes(id, null) < 0)
    {
        block.encoding = previousEncoding;
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SUCCESS;
}

int clm_string_slice(IdKey sourceId, clm_int start, clm_int length, IdKey destinationId)
{
    auto block = clm_bpa_find_block(sourceId);
    string text;
    size_t characters, first, last;
    if (start < 0 || length < 0 ||
        !getString(sourceId, block, text, characters) ||
        cast(size_t)start > characters ||
        cast(size_t)length > characters - cast(size_t)start ||
        clm_bpa_is_existing(destinationId) ||
        !byteOffset(text, cast(size_t)start, first) ||
        !byteOffset(text, cast(size_t)start + cast(size_t)length, last))
        return CLM_BPA_FAILURE;
    auto preference = block.encoding;
    if (!clm_string_new(destinationId, text[first .. last]))
        return CLM_BPA_FAILURE;
    if (clm_string_change_encoding(destinationId, preference) != CLM_BPA_SUCCESS)
    {
        if (clm_string_delete(destinationId) != CLM_BPA_SUCCESS)
            return CLM_BPA_FAILURE;
        return CLM_BPA_FAILURE;
    }
    return CLM_BPA_SUCCESS;
}

int clm_string_slice_borrow(IdKey sourceId, clm_int start, clm_int length,
    out string text, out size_t byteLength)
{
    text = null;
    byteLength = 0;
    auto block = clm_bpa_find_block(sourceId);
    string source;
    size_t characters, first, last;
    if (start < 0 || length < 0 ||
        !getString(sourceId, block, source, characters) ||
        cast(size_t)start > characters ||
        cast(size_t)length > characters - cast(size_t)start ||
        !byteOffset(source, cast(size_t)start, first) ||
        !byteOffset(source, cast(size_t)start + cast(size_t)length, last))
        return CLM_BPA_FAILURE;
    text = source[first .. last];
    byteLength = last - first;
    return CLM_BPA_SUCCESS;
}

bool clm_string_array_new(IdKey id, clm_int reserved)
{
    return clm_bpaa_create(id, CLM_BPA_TYPE_STRING, reserved,
        CLM_STRING_ENCODING_UTF8) == CLM_BPA_SUCCESS;
}

clm_int clm_string_array_length_at(IdKey id, clm_int index)
{
    auto array = clm_bpaa_find_array(id);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS ||
        index < 0 || cast(clm_uint)index >= array.count)
        return CLM_BPA_FAILURE;
    auto text = readArrayString(array, cast(size_t)index);
    size_t characters;
    return !text.isNull && measure(text, characters)
        ? cast(clm_int)characters : CLM_BPA_FAILURE;
}

private bool isNull(string value) { return value.ptr is null; }

bool clm_string_array_contains_at(IdKey id, clm_int index, string search)
{
    auto array = clm_bpaa_find_array(id);
    size_t characters, searchCharacters;
    if (search is null || containsNul(search) ||
        getStringArray(id, array) != CLM_BPA_SUCCESS ||
        index < 0 || cast(clm_uint)index >= array.count ||
        !measure(search, searchCharacters))
        return false;
    auto current = readArrayString(array, cast(size_t)index);
    return !isNull(current) && measure(current, characters) &&
        findBytes(current, search) != size_t.max;
}

clm_int clm_string_array_append_at(IdKey id, clm_int index, string suffix)
{
    auto array = clm_bpaa_find_array(id);
    size_t currentCharacters, suffixCharacters;
    if (suffix is null || containsNul(suffix) ||
        getStringArray(id, array) != CLM_BPA_SUCCESS ||
        index < 0 || cast(clm_uint)index >= array.count)
        return CLM_BPA_FAILURE;
    auto current = readArrayString(array, cast(size_t)index);
    if (isNull(current) || !measure(current, currentCharacters) ||
        !measure(suffix, suffixCharacters) ||
        suffix.length > maxStringLength - current.length ||
        replaceArrayString(array, index, current ~ suffix) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return cast(clm_int)suffixCharacters;
}

clm_int clm_string_array_append_line_at(IdKey id, clm_int index, string suffix)
{
    auto array = clm_bpaa_find_array(id);
    size_t currentCharacters, suffixCharacters;
    if (suffix is null || containsNul(suffix) ||
        getStringArray(id, array) != CLM_BPA_SUCCESS ||
        index < 0 || cast(clm_uint)index >= array.count)
        return CLM_BPA_FAILURE;
    auto current = readArrayString(array, cast(size_t)index);
    if (isNull(current) || !measure(current, currentCharacters) ||
        !measure(suffix, suffixCharacters) ||
        current.length >= maxStringLength ||
        suffix.length > maxStringLength - current.length - 1 ||
        replaceArrayString(array, index, current ~ suffix ~ "\n") != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return cast(clm_int)(suffixCharacters + 1);
}

clm_int clm_string_array_insert_at(IdKey id, clm_int index,
    clm_int characterIndex, string inserted)
{
    auto array = clm_bpaa_find_array(id);
    size_t currentCharacters, insertedCharacters, offset;
    if (inserted is null || containsNul(inserted) || characterIndex < 0 ||
        getStringArray(id, array) != CLM_BPA_SUCCESS ||
        index < 0 || cast(clm_uint)index >= array.count)
        return CLM_BPA_FAILURE;
    auto current = readArrayString(array, cast(size_t)index);
    if (isNull(current) || !measure(current, currentCharacters) ||
        cast(size_t)characterIndex > currentCharacters ||
        !measure(inserted, insertedCharacters) ||
        !byteOffset(current, cast(size_t)characterIndex, offset) ||
        inserted.length > maxStringLength - current.length)
        return CLM_BPA_FAILURE;
    auto result = current[0 .. offset] ~ inserted ~ current[offset .. $];
    return replaceArrayString(array, index, result) == CLM_BPA_SUCCESS
        ? cast(clm_int)insertedCharacters : CLM_BPA_FAILURE;
}

clm_int clm_string_array_remove_at(IdKey id, clm_int index,
    clm_int characterIndex, clm_int length)
{
    auto array = clm_bpaa_find_array(id);
    size_t characters, start, end;
    if (characterIndex < 0 || length < 0 ||
        getStringArray(id, array) != CLM_BPA_SUCCESS ||
        index < 0 || cast(clm_uint)index >= array.count)
        return CLM_BPA_FAILURE;
    auto current = readArrayString(array, cast(size_t)index);
    if (isNull(current) || !measure(current, characters) ||
        cast(size_t)characterIndex > characters ||
        cast(size_t)length > characters - cast(size_t)characterIndex)
        return CLM_BPA_FAILURE;
    if (length == 0)
        return 0;
    if (!byteOffset(current, cast(size_t)characterIndex, start) ||
        !byteOffset(current, cast(size_t)characterIndex + cast(size_t)length, end))
        return CLM_BPA_FAILURE;
    auto result = current[0 .. start] ~ current[end .. $];
    return replaceArrayString(array, index, result) == CLM_BPA_SUCCESS
        ? length : CLM_BPA_FAILURE;
}

clm_int clm_string_array_replace_at(IdKey id, clm_int index,
    string search, string replacement)
{
    auto array = clm_bpaa_find_array(id);
    size_t characters, searchCharacters, replacementCharacters;
    if (search is null || replacement is null || search.length == 0 ||
        containsNul(search) || containsNul(replacement) ||
        getStringArray(id, array) != CLM_BPA_SUCCESS ||
        index < 0 || cast(clm_uint)index >= array.count)
        return CLM_BPA_FAILURE;
    auto current = readArrayString(array, cast(size_t)index);
    if (isNull(current) || !measure(current, characters) ||
        !measure(search, searchCharacters) ||
        !measure(replacement, replacementCharacters))
        return CLM_BPA_FAILURE;
    size_t count, copied;
    auto offset = findBytes(current, search);
    if (offset == size_t.max)
        return 0;
    string result;
    while (offset != size_t.max)
    {
        result ~= current[copied .. offset] ~ replacement;
        copied = offset + search.length;
        ++count;
        offset = findBytes(current, search, copied);
    }
    result ~= current[copied .. $];
    return replaceArrayString(array, index, result) == CLM_BPA_SUCCESS
        ? cast(clm_int)count : CLM_BPA_FAILURE;
}

bool clm_string_array_delete(IdKey id)
{
    auto array = clm_bpaa_find_array(id);
    return getStringArray(id, array) == CLM_BPA_SUCCESS &&
        clm_bpaa_delete(id) == CLM_BPA_SUCCESS;
}

clm_int clm_string_array_count(IdKey id)
{
    auto array = clm_bpaa_find_array(id);
    return getStringArray(id, array) == CLM_BPA_SUCCESS && array.count <= clm_int.max
        ? cast(clm_int)array.count : CLM_BPA_FAILURE;
}

int clm_string_array_get(IdKey id, clm_int index, IdKey destinationStringId)
{
    auto array = clm_bpaa_find_array(id);
    auto destination = clm_bpa_find_block(destinationStringId);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS ||
        destination is null || destination.type != CLM_BPA_TYPE_STRING ||
        index < 0 || cast(clm_uint)index >= array.count ||
        clm_bpaa_get_string(id, index, destinationStringId, size_t.max) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    destination.encoding = array.encoding;
    return CLM_BPA_GET_SUCCESS;
}

int clm_string_array_set(IdKey id, clm_int index, IdKey sourceStringId)
{
    auto array = clm_bpaa_find_array(id);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS ||
        !validateStringBlock(sourceStringId))
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_string(id, index, sourceStringId, size_t.max);
}

int clm_string_array_insert(IdKey id, clm_int index, IdKey sourceStringId)
{
    auto array = clm_bpaa_find_array(id);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS ||
        !validateStringBlock(sourceStringId))
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_string(id, index, sourceStringId, size_t.max);
}

int clm_string_array_append(IdKey id, IdKey sourceStringId)
{
    auto array = clm_bpaa_find_array(id);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS ||
        !validateStringBlock(sourceStringId))
        return CLM_BPA_FAILURE;
    return clm_bpaa_append_string(id, sourceStringId, size_t.max);
}

int clm_string_array_remove(IdKey id, clm_int index)
{
    auto array = clm_bpaa_find_array(id);
    return getStringArray(id, array) == CLM_BPA_SUCCESS
        ? clm_bpaa_remove(id, index) : CLM_BPA_FAILURE;
}

int clm_string_array_clear(IdKey id)
{
    auto array = clm_bpaa_find_array(id);
    return getStringArray(id, array) == CLM_BPA_SUCCESS
        ? clm_bpaa_clear(id) : CLM_BPA_FAILURE;
}

clm_int clm_string_array_find(IdKey id, IdKey sourceStringId)
{
    auto array = clm_bpaa_find_array(id);
    auto source = clm_bpa_find_block(sourceStringId);
    string sourceText;
    size_t characters;
    if (getStringArray(id, array) != CLM_BPA_SUCCESS ||
        !getString(sourceStringId, source, sourceText, characters))
        return CLM_BPA_FAILURE;
    foreach (index; 0 .. array.count)
        if (readArrayString(array, index) == sourceText)
            return index <= clm_int.max ? cast(clm_int)index : CLM_BPA_FAILURE;
    return CLM_BPA_FAILURE;
}

int clm_string_array_sort(IdKey id)
{
    auto array = clm_bpaa_find_array(id);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (array.count > 1)
        sortStrings(array);
    array.index = 0;
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

private void sortStrings(clm_bpaa_array_node array)
{
    auto values = array.strings[0 .. array.count];
    import std.algorithm.sorting : sort;
    sort!((left, right) => cast(const(char)[])left < cast(const(char)[])right)(values);
}

int clm_string_array_shuffle(IdKey id)
{
    auto array = clm_bpaa_find_array(id);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    for (size_t index = array.count; index > 1; --index)
    {
        auto other = uniform(0, index);
        auto temporary = array.strings[index - 1];
        array.strings[index - 1] = array.strings[other];
        array.strings[other] = temporary;
    }
    array.index = 0;
    array.deck_size = array.count;
    return CLM_BPA_SUCCESS;
}

int clm_string_array_move_first(IdKey id)
{
    auto array = clm_bpaa_find_array(id);
    return getStringArray(id, array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_first(id) : CLM_BPA_FAILURE;
}

int clm_string_array_move_previous(IdKey id)
{
    auto array = clm_bpaa_find_array(id);
    return getStringArray(id, array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_previous(id) : CLM_BPA_FAILURE;
}

int clm_string_array_move_next(IdKey id)
{
    auto array = clm_bpaa_find_array(id);
    return getStringArray(id, array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_next(id) : CLM_BPA_FAILURE;
}

int clm_string_array_move_last(IdKey id)
{
    auto array = clm_bpaa_find_array(id);
    return getStringArray(id, array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_last(id) : CLM_BPA_FAILURE;
}

int clm_string_array_move_to(IdKey id, clm_int index)
{
    auto array = clm_bpaa_find_array(id);
    return getStringArray(id, array) == CLM_BPA_SUCCESS
        ? clm_bpaa_move_to(id, index) : CLM_BPA_FAILURE;
}

int clm_string_array_get_current(IdKey id, IdKey destinationStringId)
{
    auto array = clm_bpaa_find_array(id);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS ||
        array.index > clm_int.max)
        return CLM_BPA_FAILURE;
    return clm_string_array_get(id, cast(clm_int)array.index, destinationStringId);
}

int clm_string_array_draw_at(IdKey id, clm_int deckIndex, IdKey destinationStringId)
{
    auto array = clm_bpaa_find_array(id);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS ||
        deckIndex < 0 || cast(clm_uint)deckIndex >= array.deck_size ||
        clm_string_array_get(id, deckIndex, destinationStringId) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    auto last = array.deck_size - 1;
    auto temporary = array.strings[deckIndex];
    array.strings[deckIndex] = array.strings[last];
    array.strings[last] = temporary;
    --array.deck_size;
    return CLM_BPA_GET_SUCCESS;
}

int clm_string_array_draw(IdKey id, IdKey destinationStringId)
{
    auto array = clm_bpaa_find_array(id);
    if (getStringArray(id, array) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (array.deck_size == 0 &&
        clm_string_array_shuffle(id) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    return clm_string_array_draw_at(id, 0, destinationStringId);
}
