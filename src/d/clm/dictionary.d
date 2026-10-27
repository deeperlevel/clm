module clm.dictionary;

import clm.base : clm_byte, clm_int, clm_uint, IdKey;
import clm.bpa;

private final class Dictionary
{
    clm_byte type;
    clm_uint valueSize;
    ulong[] order;
    ubyte[][ulong] values;
    size_t cursor;
}

private __gshared Dictionary[IdKey] dictionaries;

private Dictionary find(IdKey id)
{
    auto ptr = id in dictionaries;
    return ptr is null ? null : *ptr;
}

private bool compatible(Dictionary d, clm_bpa_block_node b)
{
    if (b is null) return false;
    if (d.type == CLM_BPA_TYPE_STRING) return b.type == CLM_BPA_TYPE_STRING;
    return b.type != CLM_BPA_TYPE_STRING && b.block_size == d.valueSize &&
        (d.type == CLM_BPA_TYPE_STRUCT || b.type == d.type);
}

int clm_dictionary_new(IdKey id, clm_byte type, clm_uint valueSize)
{
    if (id in dictionaries || clm_bpa_is_existing(id) || clm_bpaa_is_existing(id) ||
        type > CLM_BPA_TYPE_DATETIME)
        return CLM_BPA_FAILURE;
    auto d = new Dictionary;
    d.type = type;
    if (type == CLM_BPA_TYPE_STRING)
    {
        if (valueSize != 0) return CLM_BPA_FAILURE;
    }
    else if (type == CLM_BPA_TYPE_STRUCT)
    {
        if (valueSize == 0) return CLM_BPA_FAILURE;
        d.valueSize = valueSize;
    }
    else
    {
        d.valueSize = clm_bpa_type_size(type);
        if (valueSize != 0 && valueSize != d.valueSize) return CLM_BPA_FAILURE;
    }
    dictionaries[id] = d;
    return CLM_BPA_SUCCESS;
}

int clm_dictionary_delete(IdKey id)
{
    if (find(id) is null) return CLM_BPA_FAILURE;
    dictionaries.remove(id);
    return CLM_BPA_SUCCESS;
}

private int store(Dictionary d, ulong key, const(ubyte)[] raw, bool allowInsert, bool allowUpdate)
{
    if (d.type != CLM_BPA_TYPE_STRING && raw.length != d.valueSize) return CLM_BPA_FAILURE;
    bool exists = (key in d.values) !is null;
    if ((exists && !allowUpdate) || (!exists && !allowInsert)) return CLM_BPA_FAILURE;
    if (!exists) d.order ~= key;
    d.values[key] = raw.dup;
    return CLM_BPA_SET_SUCCESS;
}

private int storeFrom(IdKey id, ulong key, const(ubyte)[] raw, bool allowInsert, bool allowUpdate)
{
    auto d = find(id);
    return d is null ? CLM_BPA_FAILURE : store(d, key, raw, allowInsert, allowUpdate);
}

private const(ubyte)[] asBytes(T)(ref const T value)
{
    return (cast(const(ubyte)*)&value)[0 .. T.sizeof];
}

int clm_dictionary_set_from(IdKey id, ulong key, const(char)[] text)
{
    auto d = find(id);
    if (d is null || d.type != CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    return store(d, key, cast(const(ubyte)[])text, true, true);
}

int clm_dictionary_set_from(T)(IdKey id, ulong key, auto ref const T value)
    if (!is(T : const(char)[]))
{
    return storeFrom(id, key, asBytes(value), true, true);
}

int clm_dictionary_add_from(T)(IdKey id, ulong key, auto ref const T value)
{
    static if (is(T : const(char)[]))
        return storeFrom(id, key, cast(const(ubyte)[])value, true, false);
    else
        return storeFrom(id, key, asBytes(value), true, false);
}

int clm_dictionary_update_from(T)(IdKey id, ulong key, auto ref const T value)
{
    static if (is(T : const(char)[]))
        return storeFrom(id, key, cast(const(ubyte)[])value, false, true);
    else
        return storeFrom(id, key, asBytes(value), false, true);
}

int clm_dictionary_set(IdKey id, ulong key, IdKey blockId)
{
    auto d = find(id);
    auto b = clm_bpa_find_block(blockId);
    if (d is null || !compatible(d, b)) return CLM_BPA_FAILURE;
    return store(d, key, b.data[0 .. d.type == CLM_BPA_TYPE_STRING ? b.length : b.block_size], true, true);
}

int clm_dictionary_get_to(IdKey id, ulong key, out string text)
{
    auto d = find(id);
    if (d is null || d.type != CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    auto ptr = key in d.values;
    if (ptr is null) return CLM_BPA_FAILURE;
    text = cast(string)(*ptr).idup;
    return CLM_BPA_GET_SUCCESS;
}

int clm_dictionary_get_to(T)(IdKey id, ulong key, ref T value)
    if (!is(T : const(char)[]))
{
    auto d = find(id);
    if (d is null || d.type == CLM_BPA_TYPE_STRING || d.valueSize != T.sizeof) return CLM_BPA_FAILURE;
    auto ptr = key in d.values;
    if (ptr is null) return CLM_BPA_FAILURE;
    (cast(ubyte*)&value)[0 .. T.sizeof] = (*ptr)[];
    return CLM_BPA_GET_SUCCESS;
}

int clm_dictionary_get(IdKey id, ulong key, IdKey blockId)
{
    auto d = find(id);
    auto b = clm_bpa_find_block(blockId);
    if (d is null || !compatible(d, b)) return CLM_BPA_FAILURE;
    auto ptr = key in d.values;
    if (ptr is null) return CLM_BPA_FAILURE;
    if (d.type == CLM_BPA_TYPE_STRING)
    {
        b.data = (*ptr).dup;
        b.length = cast(clm_uint)b.data.length;
    }
    else
        b.data[] = (*ptr)[];
    return CLM_BPA_GET_SUCCESS;
}

bool clm_dictionary_contains(IdKey id, ulong key)
{
    auto d = find(id);
    return d !is null && (key in d.values) !is null;
}

clm_int clm_dictionary_count(IdKey id)
{
    auto d = find(id);
    return d is null ? CLM_BPA_FAILURE : cast(clm_int)d.order.length;
}

int clm_dictionary_remove(IdKey id, ulong key)
{
    auto d = find(id);
    if (d is null || (key in d.values) is null) return CLM_BPA_FAILURE;
    d.values.remove(key);
    foreach (i, k; d.order)
        if (k == key)
        {
            d.order = d.order[0 .. i] ~ d.order[i + 1 .. $];
            if (i < d.cursor) --d.cursor;
            break;
        }
    return CLM_BPA_SUCCESS;
}

int clm_dictionary_iterate_reset(IdKey id)
{
    auto d = find(id);
    if (d is null) return CLM_BPA_FAILURE;
    d.cursor = 0;
    return CLM_BPA_SUCCESS;
}

int clm_dictionary_iterate_next(IdKey id, out ulong key)
{
    auto d = find(id);
    if (d is null || d.cursor >= d.order.length) return CLM_BPA_FAILURE;
    key = d.order[d.cursor++];
    return CLM_BPA_GET_SUCCESS;
}

int clm_dictionary_clear(IdKey id)
{
    auto d = find(id);
    if (d is null) return CLM_BPA_FAILURE;
    d.order = null;
    d.values = null;
    d.cursor = 0;
    return CLM_BPA_SUCCESS;
}

int clm_dictionary_keys_to(IdKey id, ulong[] destination)
{
    auto d = find(id);
    if (d is null || destination.length < d.order.length) return CLM_BPA_FAILURE;
    destination[0 .. d.order.length] = d.order[];
    return CLM_BPA_GET_SUCCESS;
}
