module clm.id;

import std.utf : validate;
import clm.base : IdKey;
import clm.bpa : CLM_BPA_FAILURE, CLM_BPA_GET_SUCCESS, CLM_BPA_SET_SUCCESS, CLM_BPA_SUCCESS;

enum IdKey CLM_ID_NOT_FOUND = IdKey.max;

private struct IdMapping
{
    IdKey id;
    string originalName;
}
private __gshared IdMapping[string] idEntries;
private __gshared bool idCaseSensitive = true;
private __gshared IdKey idGeneratorValue;
private __gshared bool idGeneratorExhausted;

private bool validName(string name)
{
    if (name is null || name.length == 0)
        return false;
    try
    {
        validate(name);
        return true;
    }
    catch (Exception)
    {
        return false;
    }
}

private string effectiveName(string name, bool caseSensitive)
{
    if (caseSensitive)
        return name;
    auto folded = name.dup;
    foreach (ref character; folded)
    {
        if (character >= 'A' && character <= 'Z')
            character += 'a' - 'A';
    }
    return folded.idup;
}

int clm_id_register(string name, IdKey id)
{
    if (!validName(name))
        return CLM_BPA_FAILURE;
    auto key = effectiveName(name, idCaseSensitive);
    if (key in idEntries)
        return CLM_BPA_FAILURE;
    idEntries[key] = IdMapping(id, name);
    return CLM_BPA_SET_SUCCESS;
}

int clm_id_map(string name, IdKey id)
{
    return clm_id_register(name, id);
}

IdKey clm_id_get(string name)
{
    if (!validName(name))
        return CLM_ID_NOT_FOUND;
    auto key = effectiveName(name, idCaseSensitive);
    auto found = key in idEntries;
    return found is null ? CLM_ID_NOT_FOUND : found.id;
}

int clm_id_try_get(string name, out IdKey id)
{
    if (!validName(name))
        return CLM_BPA_FAILURE;
    auto key = effectiveName(name, idCaseSensitive);
    auto found = key in idEntries;
    if (found is null)
        return CLM_BPA_FAILURE;
    id = found.id;
    return CLM_BPA_GET_SUCCESS;
}

int clm_id_set(string name, IdKey id)
{
    if (!validName(name))
        return CLM_BPA_FAILURE;
    auto key = effectiveName(name, idCaseSensitive);
    auto found = key in idEntries;
    if (found is null)
        idEntries[key] = IdMapping(id, name);
    else
        found.id = id;
    return CLM_BPA_SET_SUCCESS;
}

int clm_id_remove(string name)
{
    if (!validName(name))
        return CLM_BPA_FAILURE;
    auto key = effectiveName(name, idCaseSensitive);
    if (key !in idEntries)
        return CLM_BPA_FAILURE;
    idEntries.remove(key);
    return CLM_BPA_SUCCESS;
}

int clm_id_unmap(IdKey id)
{
    string[] removed;
    foreach (name, value; idEntries)
    {
        if (value.id == id)
            removed ~= name;
    }
    if (removed.length == 0)
        return CLM_BPA_FAILURE;
    foreach (name; removed)
        idEntries.remove(name);
    return CLM_BPA_SUCCESS;
}

int clm_id_unmap_name(string name)
{
    return clm_id_remove(name);
}

bool clm_id_is_case_sensitive()
{
    return idCaseSensitive;
}

int clm_id_set_case_sensitive(bool enabled)
{
    if (enabled == idCaseSensitive)
        return CLM_BPA_SUCCESS;
    IdMapping[string] remapped;
    foreach (entry; idEntries)
    {
        auto key = effectiveName(entry.originalName, enabled);
        if (key in remapped)
            return CLM_BPA_FAILURE;
        remapped[key] = entry;
    }
    idEntries = remapped;
    idCaseSensitive = enabled;
    return CLM_BPA_SUCCESS;
}

int clm_id_clear()
{
    idEntries = null;
    return CLM_BPA_SUCCESS;
}

int clm_id_shutdown()
{
    idEntries = null;
    return CLM_BPA_SUCCESS;
}

void clm_id_gen_reset()
{
    idGeneratorValue = 0;
    idGeneratorExhausted = false;
}

IdKey clm_id_gen_next()
{
    if (idGeneratorExhausted)
        return CLM_ID_NOT_FOUND;
    auto next = idGeneratorValue;
    if (next == CLM_ID_NOT_FOUND - 1)
        idGeneratorExhausted = true;
    else
        ++idGeneratorValue;
    return next;
}
