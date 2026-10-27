module clm.bpa;

import clm.base : clm_byte, clm_int, clm_uint, clm_ushort, clm_sbyte, clm_short, 
                  clm_ulong, clm_long, clm_float, clm_double, IdKey;
enum int CLM_BPA_FAILURE = -1;
enum int CLM_BPA_SUCCESS = 0;
enum int CLM_BPA_GET_SUCCESS = 1;
enum int CLM_BPA_SET_SUCCESS = 2;

enum clm_byte CLM_BPA_TYPE_STRUCT = 0;
enum clm_byte CLM_BPA_TYPE_SIGNED_BYTE = 1;
enum clm_byte CLM_BPA_TYPE_BYTE = 2;
enum clm_byte CLM_BPA_TYPE_SHORT = 3;
enum clm_byte CLM_BPA_TYPE_UNSIGNED_SHORT = 4;
enum clm_byte CLM_BPA_TYPE_INT = 5;
enum clm_byte CLM_BPA_TYPE_UNSIGNED_INT = 6;
enum clm_byte CLM_BPA_TYPE_LONG = 7;
enum clm_byte CLM_BPA_TYPE_UNSIGNED_LONG = 8;
enum clm_byte CLM_BPA_TYPE_FLOAT = 9;
enum clm_byte CLM_BPA_TYPE_DOUBLE = 10;
enum clm_byte CLM_BPA_TYPE_STRING = 11;
enum clm_byte CLM_BPA_TYPE_UUID = 12;
enum clm_byte CLM_BPA_TYPE_DATETIME = 13;

enum size_t CLM_BPA_INITIAL_CAPACITY = 16;
enum size_t CLM_BPA_POOL_SLAB_SLOTS = 32;
enum size_t CLM_BPA_POOL_MAX_SLOT = 4096;
enum size_t CLM_BPA_POOL_CLASSES = 10;

class ClmBpaBlockNode
{
    IdKey id;
    clm_byte type;
    clm_ushort refcount = 1;
    clm_uint block_size;
    clm_byte encoding;
    clm_uint length;
    ubyte[] data;
}

class ClmBpaaArrayNode
{
    IdKey id;
    clm_byte type;
    clm_ushort refcount = 1;
    clm_uint count;
    clm_uint capacity;
    clm_uint block_size;
    clm_byte encoding;
    clm_uint index;
    clm_byte sealed;
    clm_uint deck_size;
    ubyte[] blocks;
    ubyte[][] strings;
}

struct ClmBpaPoolClass
{
    size_t slot_size;
    size_t active;
    size_t capacity;
    private ubyte[][] free_slots;
}

alias clm_bpa_block_node = ClmBpaBlockNode;
alias clm_bpaa_array_node = ClmBpaaArrayNode;
alias clm_bpa_pool_class = ClmBpaPoolClass;

private ClmBpaBlockNode[IdKey] blocksById;
private ClmBpaaArrayNode[IdKey] arraysById;
private ClmBpaPoolClass[CLM_BPA_POOL_CLASSES] pools;
size_t clm_bpa_global_blocks_capacity;
size_t clm_bpa_global_blocks_count;
size_t clm_bpa_global_blocks_tombstones;
size_t clm_bpa_global_arrays_capacity;
size_t clm_bpa_global_arrays_count;
size_t clm_bpa_global_arrays_tombstones;

alias clm_bpaa_compare_fn = int function(const(ubyte)[] left, const(ubyte)[] right);
alias clm_bpaa_predicate_fn = bool function(const(ubyte)[] left, const(ubyte)[] right);

size_t clm_bpa_pool_class_for(size_t bytes)
{
    if (bytes == 0 || bytes > CLM_BPA_POOL_MAX_SLOT)
        return CLM_BPA_POOL_CLASSES;
    size_t size = 8;
    size_t index;
    while (size < bytes && index + 1 < CLM_BPA_POOL_CLASSES)
    {
        size *= 2;
        ++index;
    }
    return size >= bytes ? index : CLM_BPA_POOL_CLASSES;
}

ubyte[] clm_bpa_pool_alloc(size_t bytes)
{
    if (bytes == 0)
        return null;
    auto index = clm_bpa_pool_class_for(bytes);
    if (index == CLM_BPA_POOL_CLASSES)
        return new ubyte[bytes];
    auto ref pool = pools[index];
    if (pool.slot_size == 0)
        pool.slot_size = cast(size_t)8 << index;
    // Reserve real, reusable fixed-width slots, rather than merely recording
    // a fictitious slab capacity. Returned slices keep the full slot backing.
    if (pool.free_slots.length == 0)
    {
        foreach (_; 0 .. CLM_BPA_POOL_SLAB_SLOTS)
            pool.free_slots ~= new ubyte[pool.slot_size];
        pool.capacity += CLM_BPA_POOL_SLAB_SLOTS;
    }
    auto slot = pool.free_slots[$ - 1];
    pool.free_slots.length--;
    slot[] = 0;
    ubyte[] result = slot[0 .. bytes];
    ++pool.active;
    return result;
}

void clm_bpa_pool_free(ref ubyte[] memory, size_t bytes)
{
    if (memory.length == 0)
        return;
    auto index = clm_bpa_pool_class_for(bytes);
    if (index != CLM_BPA_POOL_CLASSES)
    {
        auto ref pool = pools[index];
        // Recover the original fixed-size slot without requesting a resize.
        pool.free_slots ~= memory.ptr[0 .. pool.slot_size];
        if (pool.active != 0)
            --pool.active;
    }
    memory = null;
}

int clm_bpa_pool_cleanup()
{
    foreach (ref pool; pools)
        if (pool.active != 0)
            return CLM_BPA_FAILURE;
    foreach (ref pool; pools)
        pool = ClmBpaPoolClass.init;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_pool_stats(size_t bytes, out size_t active, out size_t capacity)
{
    auto index = clm_bpa_pool_class_for(bytes);
    if (index == CLM_BPA_POOL_CLASSES)
    {
        active = capacity = 0;
        return CLM_BPA_FAILURE;
    }
    active = pools[index].active;
    capacity = pools[index].capacity;
    return CLM_BPA_SUCCESS;
}

private size_t hashId(IdKey id)
{
    ulong value = id;
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9UL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebUL;
    value ^= value >> 31;
    return cast(size_t)value;
}

size_t clm_bpa_hash_id(IdKey id) { return hashId(id); }

private int prepareBlocks()
{
    if (clm_bpa_global_blocks_capacity == 0)
        clm_bpa_global_blocks_capacity = CLM_BPA_INITIAL_CAPACITY;
    else if ((clm_bpa_global_blocks_count + clm_bpa_global_blocks_tombstones + 1)
            >= clm_bpa_global_blocks_capacity - clm_bpa_global_blocks_capacity / 4)
        clm_bpa_global_blocks_capacity *= 2;
    return CLM_BPA_SUCCESS;
}

private int prepareArrays()
{
    if (clm_bpa_global_arrays_capacity == 0)
        clm_bpa_global_arrays_capacity = CLM_BPA_INITIAL_CAPACITY;
    else if ((clm_bpa_global_arrays_count + clm_bpa_global_arrays_tombstones + 1)
            >= clm_bpa_global_arrays_capacity - clm_bpa_global_arrays_capacity / 4)
        clm_bpa_global_arrays_capacity *= 2;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_prepare_blocks() { return prepareBlocks(); }
int clm_bpa_prepare_arrays() { return prepareArrays(); }

clm_uint clm_bpa_type_size(clm_byte type)
{
    final switch (type)
    {
        case CLM_BPA_TYPE_STRUCT: return 0;
        case CLM_BPA_TYPE_SIGNED_BYTE, CLM_BPA_TYPE_BYTE: return 1;
        case CLM_BPA_TYPE_SHORT, CLM_BPA_TYPE_UNSIGNED_SHORT: return 2;
        case CLM_BPA_TYPE_INT, CLM_BPA_TYPE_UNSIGNED_INT, CLM_BPA_TYPE_FLOAT: return 4;
        case CLM_BPA_TYPE_LONG, CLM_BPA_TYPE_UNSIGNED_LONG, CLM_BPA_TYPE_DOUBLE,
             CLM_BPA_TYPE_DATETIME: return 8;
        case CLM_BPA_TYPE_STRING: return 0;
        case CLM_BPA_TYPE_UUID: return 16;
    }
}

ClmBpaBlockNode clm_bpa_find_block(IdKey id)
{
    auto ptr = id in blocksById;
    return ptr is null ? null : *ptr;
}

ClmBpaaArrayNode clm_bpaa_find_array(IdKey id)
{
    auto ptr = id in arraysById;
    return ptr is null ? null : *ptr;
}

bool clm_bpa_is_existing(IdKey id) { return clm_bpa_find_block(id) !is null; }
bool clm_bpaa_is_existing(IdKey id) { return clm_bpaa_find_array(id) !is null; }

private int validateBlockType(clm_byte type, clm_uint requested, out clm_uint size)
{
    size = clm_bpa_type_size(type);
    if (type == CLM_BPA_TYPE_STRING)
        return requested == 0 ? CLM_BPA_SUCCESS : CLM_BPA_FAILURE;
    if (type == CLM_BPA_TYPE_STRUCT)
    {
        if (requested == 0) return CLM_BPA_FAILURE;
        size = requested;
        return CLM_BPA_SUCCESS;
    }
    if (size == 0 || (requested != 0 && requested != size))
        return CLM_BPA_FAILURE;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_create_sized(IdKey id, clm_byte type, clm_uint block_size)
{
    clm_uint size;
    if (clm_bpa_is_existing(id) || prepareBlocks() != CLM_BPA_SUCCESS ||
        validateBlockType(type, block_size, size) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    auto block = new ClmBpaBlockNode;
    block.id = id;
    block.type = type;
    block.block_size = size;
    if (type != CLM_BPA_TYPE_STRING)
        block.data = clm_bpa_pool_alloc(size);
    blocksById[id] = block;
    ++clm_bpa_global_blocks_count;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_create(IdKey id, clm_byte type)
{
    if (type == CLM_BPA_TYPE_STRUCT) return CLM_BPA_FAILURE;
    return clm_bpa_create_sized(id, type, 0);
}

int clm_bpa_delete(IdKey id)
{
    auto ptr = id in blocksById;
    if (ptr is null) return CLM_BPA_FAILURE;
    auto block = *ptr;
    clm_bpa_pool_free(block.data, block.type == CLM_BPA_TYPE_STRING ? block.length + 1 : block.block_size);
    blocksById.remove(id);
    --clm_bpa_global_blocks_count;
    ++clm_bpa_global_blocks_tombstones;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_ref(IdKey id)
{
    auto b = clm_bpa_find_block(id);
    if (b is null || b.refcount == ushort.max) return CLM_BPA_FAILURE;
    ++b.refcount;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_unref(IdKey id)
{
    auto b = clm_bpa_find_block(id);
    if (b is null || b.refcount == 0) return CLM_BPA_FAILURE;
    if (--b.refcount != 0) return CLM_BPA_SUCCESS;
    return clm_bpa_delete(id);
}

int clm_bpa_get(IdKey id, ref ubyte[] destination, size_t capacity)
{
    auto b = clm_bpa_find_block(id);
    if (b is null || b.type == CLM_BPA_TYPE_STRING || capacity < b.block_size)
        return CLM_BPA_FAILURE;
    destination = b.data.dup;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpa_set(IdKey id, const(ubyte)[] source)
{
    auto b = clm_bpa_find_block(id);
    if (b is null || b.type == CLM_BPA_TYPE_STRING || source.length != b.block_size)
        return CLM_BPA_FAILURE;
    b.data[] = source[];
    return CLM_BPA_SET_SUCCESS;
}

private bool boundedString(const(ubyte)[] source, out ubyte[] value)
{
    ptrdiff_t zero = -1;
    foreach (i, ch; source)
        if (ch == 0)
        {
            zero = cast(ptrdiff_t)i;
            break;
        }
    value = zero < 0 ? source.dup : source[0 .. zero].dup;
    return true;
}

int clm_bpa_get_string(IdKey id, out ubyte[] destination, size_t capacity)
{
    auto b = clm_bpa_find_block(id);
    if (b is null || b.type != CLM_BPA_TYPE_STRING ||
        capacity < b.length + 1 || b.length > int.max)
        return CLM_BPA_FAILURE;
    destination = b.data.dup;
    destination ~= 0;
    return cast(int)b.length;
}

int clm_bpa_set_string(IdKey id, const(ubyte)[] source)
{
    auto b = clm_bpa_find_block(id);
    ubyte[] text;
    if (b is null || b.type != CLM_BPA_TYPE_STRING || !boundedString(source, text) || text.length > int.max)
        return CLM_BPA_FAILURE;
    b.data = text;
    b.length = cast(clm_uint)text.length;
    return cast(int)b.length;
}

int clm_bpa_replace(IdKey id, const(ubyte)[] source)
{
    auto b = clm_bpa_find_block(id);
    if (b is null) return CLM_BPA_FAILURE;
    return b.type == CLM_BPA_TYPE_STRING ? clm_bpa_set_string(id, source) : clm_bpa_set(id, source);
}

int clm_bpa_borrow(IdKey id, out const(ubyte)[] value)
{
    auto b = clm_bpa_find_block(id);
    if (b is null || b.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    value = b.data;
    return CLM_BPA_SUCCESS;
}

int clm_bpa_copy_block(IdKey sourceId, IdKey destinationId)
{
    auto source = clm_bpa_find_block(sourceId);
    if (source is null || clm_bpa_create_sized(destinationId, source.type, source.block_size) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    auto destination = clm_bpa_find_block(destinationId);
    destination.encoding = source.encoding;
    if (source.type == CLM_BPA_TYPE_STRING)
    {
        destination.data = source.data.dup;
    }
    else
    {
        // create_sized already allocated the destination's managed pool slot.
        // Preserve it: replacing the slice leaks the slot from pool accounting.
        destination.data[] = source.data[];
    }
    destination.length = source.length;
    return CLM_BPA_SUCCESS;
}

private bool validArrayItem(ClmBpaaArrayNode a, size_t index)
{
    return a !is null && index < a.count;
}

private ubyte[] itemBytes(ClmBpaaArrayNode a, size_t index)
{
    if (a.type == CLM_BPA_TYPE_STRING) return a.strings[index];
    auto begin = index * a.block_size;
    return a.blocks[begin .. begin + a.block_size];
}

int clm_bpaa_create_sized(IdKey id, clm_byte type, clm_int reserved, clm_uint block_size, clm_byte encoding)
{
    clm_uint size;
    if (clm_bpaa_is_existing(id) || prepareArrays() != CLM_BPA_SUCCESS || reserved < 0 ||
        validateBlockType(type, block_size, size) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    auto a = new ClmBpaaArrayNode;
    a.id = id;
    a.type = type;
    a.encoding = encoding;
    a.block_size = size;
    a.capacity = cast(clm_uint)reserved;
    if (type == CLM_BPA_TYPE_STRING)
        a.strings.length = a.capacity;
    else
        a.blocks.length = cast(size_t)a.capacity * size;
    arraysById[id] = a;
    ++clm_bpa_global_arrays_count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_create(IdKey id, clm_byte type, clm_int reserved, clm_byte encoding)
{
    if (type == CLM_BPA_TYPE_STRUCT) return CLM_BPA_FAILURE;
    return clm_bpaa_create_sized(id, type, reserved, 0, encoding);
}

int clm_bpa_remove_block(IdKey id) { return clm_bpa_delete(id); }
int clm_bpa_remove_array(IdKey id) { return clm_bpaa_delete(id); }
int clm_bpaa_delete(IdKey id)
{
    auto ptr = id in arraysById;
    if (ptr is null) return CLM_BPA_FAILURE;
    arraysById.remove(id);
    --clm_bpa_global_arrays_count;
    ++clm_bpa_global_arrays_tombstones;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_ref(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.refcount == ushort.max) return CLM_BPA_FAILURE;
    ++a.refcount;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_unref(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.refcount == 0) return CLM_BPA_FAILURE;
    if (--a.refcount != 0) return CLM_BPA_SUCCESS;
    return clm_bpaa_delete(id);
}

int clm_bpaa_resize(IdKey id, clm_uint capacity)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || capacity < a.count) return CLM_BPA_FAILURE;
    if (capacity == a.capacity) return CLM_BPA_SUCCESS;
    if (a.type == CLM_BPA_TYPE_STRING)
        a.strings.length = capacity;
    else
        a.blocks.length = cast(size_t)capacity * a.block_size;
    a.capacity = capacity;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_ensure_capacity(ClmBpaaArrayNode a, clm_uint required)
{
    if (a is null) return CLM_BPA_FAILURE;
    if (required <= a.capacity) return CLM_BPA_SUCCESS;
    if (a.sealed != 0) return CLM_BPA_FAILURE;
    clm_uint cap = a.capacity == 0 ? 1 : a.capacity;
    while (cap < required)
    {
        if (cap > clm_uint.max / 2) { cap = required; break; }
        cap *= 2;
    }
    return clm_bpaa_resize(a.id, cap);
}
int clm_bpaa_extend(IdKey id, clm_uint count)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || count > clm_uint.max - a.capacity) return CLM_BPA_FAILURE;
    return clm_bpaa_resize(id, a.capacity + count);
}
int clm_bpaa_shrink(IdKey id, clm_uint newCapacity)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || newCapacity >= a.capacity || newCapacity < a.count)
        return CLM_BPA_FAILURE;
    return clm_bpaa_resize(id, newCapacity);
}

int clm_bpaa_get_raw(IdKey id, clm_int index, out ubyte[] destination, size_t size)
{
    auto a = clm_bpaa_find_array(id);
    if (!validArrayItem(a, index < 0 ? size_t.max : cast(size_t)index) ||
        a.type == CLM_BPA_TYPE_STRING || size < a.block_size)
        return CLM_BPA_FAILURE;
    destination = itemBytes(a, index).dup;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_get_string_raw(IdKey id, clm_int index, out ubyte[] destination, size_t size)
{
    auto a = clm_bpaa_find_array(id);
    if (!validArrayItem(a, index < 0 ? size_t.max : cast(size_t)index) ||
        a.type != CLM_BPA_TYPE_STRING || size < a.strings[index].length + 1)
        return CLM_BPA_FAILURE;
    destination = a.strings[index].dup;
    destination ~= 0;
    return cast(int)a.strings[index].length;
}

int clm_bpaa_get_first_raw(IdKey id, out ubyte[] destination, size_t size)
{
    return clm_bpaa_get_raw(id, 0, destination, size);
}
int clm_bpaa_get_last_raw(IdKey id, out ubyte[] destination, size_t size)
{
    auto a = clm_bpaa_find_array(id);
    return a is null || a.count == 0 || a.count - 1 > int.max
        ? CLM_BPA_FAILURE : clm_bpaa_get_raw(id, cast(int)(a.count - 1), destination, size);
}
int clm_bpaa_get_current_raw(IdKey id, out ubyte[] destination, size_t size)
{
    auto a = clm_bpaa_find_array(id);
    return a is null || a.index >= a.count || a.index > int.max
        ? CLM_BPA_FAILURE : clm_bpaa_get_raw(id, cast(int)a.index, destination, size);
}

int clm_bpaa_set_raw(IdKey id, clm_int index, const(ubyte)[] source)
{
    auto a = clm_bpaa_find_array(id);
    if (!validArrayItem(a, index < 0 ? size_t.max : cast(size_t)index) ||
        a.sealed != 0 || a.type == CLM_BPA_TYPE_STRING || source.length != a.block_size)
        return CLM_BPA_FAILURE;
    auto start = cast(size_t)index * a.block_size;
    a.blocks[start .. start + a.block_size] = source[];
    a.deck_size = a.count;
    return CLM_BPA_SET_SUCCESS;
}

int clm_bpaa_insert_raw(IdKey id, clm_int index, const(ubyte)[] source)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || index < 0 ||
        cast(clm_uint)index > a.count || a.count == clm_uint.max)
        return CLM_BPA_FAILURE;
    ubyte[] text;
    if (a.type == CLM_BPA_TYPE_STRING)
    {
        if (!boundedString(source, text)) return CLM_BPA_FAILURE;
    }
    else if (source.length != a.block_size) return CLM_BPA_FAILURE;
    auto sourceCopy = source.dup;
    source = sourceCopy;
    if (clm_bpaa_ensure_capacity(a, a.count + 1) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    auto pos = cast(size_t)index;
    if (a.type == CLM_BPA_TYPE_STRING)
    {
        auto tail = a.strings[pos .. a.count].dup;
        a.strings[pos + 1 .. a.count + 1] = tail[];
        a.strings[pos] = text;
    }
    else
    {
        auto start = pos * a.block_size;
        auto tail = a.blocks[start .. cast(size_t)a.count * a.block_size].dup;
        a.blocks[start + a.block_size .. start + a.block_size + tail.length] = tail;
        a.blocks[start .. start + a.block_size] = source[];
    }
    ++a.count;
    a.index = cast(clm_uint)index;
    a.deck_size = a.count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_append_raw(IdKey id, const(ubyte)[] source)
{
    auto a = clm_bpaa_find_array(id);
    return a is null || a.count > int.max ? CLM_BPA_FAILURE :
        clm_bpaa_insert_raw(id, cast(int)a.count, source);
}

int clm_bpaa_set_string_raw(IdKey id, clm_int index, const(ubyte)[] source)
{
    auto a = clm_bpaa_find_array(id);
    ubyte[] text;
    if (!validArrayItem(a, index < 0 ? size_t.max : cast(size_t)index) ||
        a.sealed != 0 || a.type != CLM_BPA_TYPE_STRING ||
        !boundedString(source, text) || text.length > int.max)
        return CLM_BPA_FAILURE;
    a.strings[index] = text;
    a.deck_size = a.count;
    return cast(int)text.length;
}

int clm_bpaa_remove(IdKey id, clm_int index)
{
    auto a = clm_bpaa_find_array(id);
    if (!validArrayItem(a, index < 0 ? size_t.max : cast(size_t)index) ||
        a.sealed != 0)
        return CLM_BPA_FAILURE;
    auto pos = cast(size_t)index;
    if (a.type == CLM_BPA_TYPE_STRING)
    {
        auto tail = a.strings[pos + 1 .. a.count].dup;
        a.strings[pos .. a.count - 1] = tail[];
        a.strings[a.count - 1] = null;
    }
    else
    {
        auto start = pos * a.block_size;
        auto end = cast(size_t)a.count * a.block_size;
        auto tail = a.blocks[start + a.block_size .. end].dup;
        a.blocks[start .. end - a.block_size] = tail[];
    }
    --a.count;
    if (a.count == 0) a.index = 0;
    else if (a.index > pos) --a.index;
    else if (a.index >= a.count) a.index = a.count - 1;
    a.deck_size = a.count;
    return CLM_BPA_SUCCESS;
}
int clm_bpaa_remove_first(IdKey id) { return clm_bpaa_remove(id, 0); }
int clm_bpaa_remove_last(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    return a is null || a.count == 0 || a.count - 1 > int.max
        ? CLM_BPA_FAILURE : clm_bpaa_remove(id, cast(int)(a.count - 1));
}
int clm_bpaa_clear(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0) return CLM_BPA_FAILURE;
    if (a.type == CLM_BPA_TYPE_STRING)
        a.strings[] = null;
    else
        a.blocks[] = 0;
    a.count = a.index = a.deck_size = 0;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_slice(IdKey sourceId, clm_int start, clm_int length, IdKey destinationId)
{
    auto src = clm_bpaa_find_array(sourceId);
    if (src is null || start < 0 || length < 0 || cast(clm_uint)start > src.count ||
        cast(clm_uint)length > src.count - cast(clm_uint)start ||
        clm_bpaa_create_sized(destinationId, src.type, length,
            src.type == CLM_BPA_TYPE_STRUCT ? src.block_size : 0, src.encoding) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    auto dst = clm_bpaa_find_array(destinationId);
    foreach (i; 0 .. cast(size_t)length)
        if (src.type == CLM_BPA_TYPE_STRING) dst.strings[i] = src.strings[start + i].dup;
        else dst.blocks[i * dst.block_size .. (i + 1) * dst.block_size] =
            itemBytes(src, start + i)[];
    dst.count = dst.deck_size = cast(clm_uint)length;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_slice_borrow(IdKey sourceId, clm_int start, clm_int length, out const(ubyte)[] value)
{
    value = null;
    auto a = clm_bpaa_find_array(sourceId);
    if (a is null || a.type == CLM_BPA_TYPE_STRING || start < 0 || length < 0 ||
        cast(clm_uint)start > a.count || cast(clm_uint)length > a.count - cast(clm_uint)start)
        return CLM_BPA_FAILURE;
    auto begin = cast(size_t)start * a.block_size;
    value = a.blocks[begin .. begin + cast(size_t)length * a.block_size];
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_get_count_to(IdKey id, out clm_uint value)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null) return CLM_BPA_FAILURE;
    value = a.count;
    return CLM_BPA_GET_SUCCESS;
}
int clm_bpaa_get_length_to(IdKey id, out clm_uint value)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.type != CLM_BPA_TYPE_STRING || a.index >= a.count) return CLM_BPA_FAILURE;
    value = cast(clm_uint)a.strings[a.index].length;
    return CLM_BPA_GET_SUCCESS;
}
int clm_bpaa_get_capacity_to(IdKey id, out clm_uint value)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null) return CLM_BPA_FAILURE;
    value = a.capacity;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_advanced_borrow(IdKey id, out ClmBpaaArrayNode array)
{
    array = clm_bpaa_find_array(id);
    if (array is null || array.refcount == ushort.max)
    {
        array = null;
        return CLM_BPA_FAILURE;
    }
    ++array.refcount;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_advanced_release(ref ClmBpaaArrayNode array)
{
    if (array is null || array.refcount == 0) return CLM_BPA_FAILURE;
    auto id = array.id;
    array = null;
    auto registered = clm_bpaa_find_array(id);
    if (registered is null || registered.refcount == 0) return CLM_BPA_FAILURE;
    if (--registered.refcount != 0) return CLM_BPA_SUCCESS;
    return clm_bpaa_delete(id);
}

ubyte[] clm_bpaa_advanced_data(ClmBpaaArrayNode array)
{
    return array is null || array.type == CLM_BPA_TYPE_STRING ? null : array.blocks;
}

int clm_bpaa_advanced_get_raw(ClmBpaaArrayNode array, clm_int index,
    out ubyte[] destination, size_t size)
{
    if (array is null) return CLM_BPA_FAILURE;
    return clm_bpaa_get_raw(array.id, index, destination, size);
}

int clm_bpaa_advanced_set_raw(ClmBpaaArrayNode array, clm_int index,
    const(ubyte)[] source)
{
    if (array is null) return CLM_BPA_FAILURE;
    return clm_bpaa_set_raw(array.id, index, source);
}

int clm_bpaa_advanced_insert_raw(ClmBpaaArrayNode array, clm_int index,
    const(ubyte)[] source)
{
    if (array is null || array.type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_raw(array.id, index, source);
}

int clm_bpaa_advanced_append_raw(ClmBpaaArrayNode array, const(ubyte)[] source)
{
    if (array is null || array.type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_FAILURE;
    return clm_bpaa_append_raw(array.id, source);
}

int clm_bpaa_advanced_remove(ClmBpaaArrayNode array, clm_int index)
{
    if (array is null || array.type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_FAILURE;
    return clm_bpaa_remove(array.id, index);
}

int clm_bpaa_advanced_clear(ClmBpaaArrayNode array)
{
    if (array is null || array.type == CLM_BPA_TYPE_STRING)
        return CLM_BPA_FAILURE;
    return clm_bpaa_clear(array.id);
}

int clm_bpaa_advanced_extend(ClmBpaaArrayNode array, clm_uint additional)
{
    if (array is null || additional == 0 || additional > clm_uint.max - array.capacity)
        return CLM_BPA_FAILURE;
    return clm_bpaa_resize(array.id, array.capacity + additional);
}

int clm_bpaa_advanced_shrink(ClmBpaaArrayNode array, clm_uint capacity)
{
    if (array is null || capacity >= array.capacity || capacity < array.count)
        return CLM_BPA_FAILURE;
    return clm_bpaa_resize(array.id, capacity);
}

int clm_bpaa_reset(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null) return CLM_BPA_FAILURE;
    a.index = 0;
    return CLM_BPA_SUCCESS;
}
int clm_bpaa_move_first(IdKey id) { return clm_bpaa_reset(id); }
int clm_bpaa_move_previous(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.index == 0) return CLM_BPA_FAILURE;
    --a.index;
    return CLM_BPA_SUCCESS;
}
int clm_bpaa_move_next(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.index + 1 >= a.count) return CLM_BPA_FAILURE;
    ++a.index;
    return CLM_BPA_SUCCESS;
}
int clm_bpaa_move_last(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.count == 0) return CLM_BPA_FAILURE;
    a.index = a.count - 1;
    return CLM_BPA_SUCCESS;
}
int clm_bpaa_move_to(IdKey id, clm_int index)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || index < 0 || cast(clm_uint)index >= a.count) return CLM_BPA_FAILURE;
    a.index = cast(clm_uint)index;
    return CLM_BPA_SUCCESS;
}
int clm_bpaa_get_current_index_to(IdKey id, out clm_int index)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.index > int.max) return CLM_BPA_FAILURE;
    index = cast(clm_int)a.index;
    return CLM_BPA_GET_SUCCESS;
}
int clm_bpaa_seal(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null) return CLM_BPA_FAILURE;
    a.sealed = 1;
    return CLM_BPA_SUCCESS;
}
int clm_bpaa_is_sealed_to(IdKey id, out clm_byte sealed)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null) return CLM_BPA_FAILURE;
    sealed = a.sealed;
    return CLM_BPA_GET_SUCCESS;
}
int clm_bpaa_unseal(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null) return CLM_BPA_FAILURE;
    a.sealed = 0;
    return CLM_BPA_SUCCESS;
}

private void swapItems(ClmBpaaArrayNode a, size_t left, size_t right)
{
    if (left == right) return;
    auto x = itemBytes(a, left).dup;
    auto y = itemBytes(a, right).dup;
    if (a.type == CLM_BPA_TYPE_STRING)
    {
        a.strings[left] = y;
        a.strings[right] = x;
    }
    else
    {
        auto start1 = left * a.block_size;
        auto start2 = right * a.block_size;
        a.blocks[start1 .. start1 + a.block_size] = y[];
        a.blocks[start2 .. start2 + a.block_size] = x[];
    }
}

int clm_bpaa_sort_with(IdKey id, clm_bpaa_compare_fn compare)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || a.type == CLM_BPA_TYPE_STRING || compare is null)
        return CLM_BPA_FAILURE;
    for (size_t i = 1; i < a.count; ++i)
        for (size_t j = i; j > 0 && compare(itemBytes(a, j - 1), itemBytes(a, j)) > 0; --j)
            swapItems(a, j - 1, j);
    a.index = 0;
    a.deck_size = a.count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_sort(IdKey id, clm_bpaa_predicate_fn predicate)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || a.type == CLM_BPA_TYPE_STRING || predicate is null)
        return CLM_BPA_FAILURE;
    for (size_t i = 1; i < a.count; ++i)
        for (size_t j = i; j > 0 && predicate(itemBytes(a, j - 1), itemBytes(a, j)); --j)
            swapItems(a, j - 1, j);
    a.index = 0;
    a.deck_size = a.count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_reverse(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || a.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    size_t i;
    size_t j = a.count;
    while (i < j && i < --j) swapItems(a, i++, j);
    a.index = 0;
    a.deck_size = a.count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_shift_left(IdKey id, clm_uint index, clm_uint positions)
{
    return rotate(id, index, positions, false);
}
int clm_bpaa_shift_right(IdKey id, clm_uint index, clm_uint positions)
{
    return rotate(id, index, positions, true);
}
private int rotate(IdKey id, clm_uint index, clm_uint positions, bool right)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || a.type == CLM_BPA_TYPE_STRING || index >= a.count)
        return CLM_BPA_FAILURE;
    auto length = a.count - index;
    auto shift = positions % length;
    if (shift != 0)
    {
        auto size = a.block_size;
        auto region = a.blocks[cast(size_t)index * size .. cast(size_t)a.count * size].dup;
        auto offset = cast(size_t)(right ? length - shift : shift) * size;
        auto target = a.blocks[cast(size_t)index * size .. cast(size_t)a.count * size];
        target[0 .. region.length - offset] = region[offset .. $];
        target[region.length - offset .. $] = region[0 .. offset];
    }
    a.index = 0;
    a.deck_size = a.count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_swap(IdKey id, clm_int index1, clm_int index2)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || a.type == CLM_BPA_TYPE_STRING || index1 < 0 || index2 < 0 ||
        cast(clm_uint)index1 >= a.count || cast(clm_uint)index2 >= a.count)
        return CLM_BPA_FAILURE;
    swapItems(a, index1, index2);
    a.deck_size = a.count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_copy(IdKey id, clm_int sourceIndex, clm_int destIndex)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || a.type == CLM_BPA_TYPE_STRING || sourceIndex < 0 || destIndex < 0 ||
        cast(clm_uint)sourceIndex >= a.count || cast(clm_uint)destIndex >= a.count)
        return CLM_BPA_FAILURE;
    auto item = itemBytes(a, sourceIndex).dup;
    a.blocks[cast(size_t)destIndex * a.block_size .. (cast(size_t)destIndex + 1) * a.block_size] = item[];
    a.deck_size = a.count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_slice_to(IdKey sourceId, clm_int start, clm_int length, ubyte[] buffer)
{
    const(ubyte)[] view;
    if (clm_bpaa_slice_borrow(sourceId, start, length, view) != CLM_BPA_SUCCESS)
        return CLM_BPA_FAILURE;
    if (view.length == 0) return CLM_BPA_SUCCESS;
    if (buffer.length < view.length) return CLM_BPA_FAILURE;
    buffer[0 .. view.length] = view[];
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_shuffle(IdKey id)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.sealed != 0 || a.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    foreach_reverse (i; 1 .. cast(size_t)a.count)
    {
        import std.random : uniform;
        auto j = uniform(0, i + 1);
        swapItems(a, i, j);
    }
    a.index = 0;
    a.deck_size = a.count;
    return CLM_BPA_SUCCESS;
}

int clm_bpaa_draw_at_raw(IdKey id, clm_int index, out ubyte[] destination, size_t size)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.type == CLM_BPA_TYPE_STRING || index < 0 ||
        cast(clm_uint)index >= a.deck_size || size < a.block_size)
        return CLM_BPA_FAILURE;
    if (a.deck_size == 0) a.deck_size = a.count;
    auto selected = cast(size_t)index;
    swapItems(a, selected, a.deck_size - 1);
    destination = itemBytes(a, a.deck_size - 1).dup;
    --a.deck_size;
    return CLM_BPA_GET_SUCCESS;
}
int clm_bpaa_draw_raw(IdKey id, out ubyte[] destination, size_t size)
{
    auto a = clm_bpaa_find_array(id);
    if (a is null || a.type == CLM_BPA_TYPE_STRING) return CLM_BPA_FAILURE;
    if (a.deck_size == 0) a.deck_size = a.count;
    if (a.deck_size == 0) return CLM_BPA_FAILURE;
    return clm_bpaa_draw_at_raw(id, 0, destination, size);
}

int clm_bpaa_get(IdKey id, clm_int index, IdKey blockId, size_t expectedSize)
{
    auto a = clm_bpaa_find_array(id);
    auto b = clm_bpa_find_block(blockId);
    if (a is null || b is null || a.type == CLM_BPA_TYPE_STRING ||
        b.type == CLM_BPA_TYPE_STRING ||
        (a.type != CLM_BPA_TYPE_STRUCT && a.type != b.type) ||
        a.block_size != b.block_size || expectedSize != a.block_size)
        return CLM_BPA_FAILURE;
    ubyte[] value;
    if (clm_bpaa_get_raw(id, index, value, expectedSize) != CLM_BPA_GET_SUCCESS)
        return CLM_BPA_FAILURE;
    b.data = value;
    return CLM_BPA_GET_SUCCESS;
}

int clm_bpaa_set(IdKey id, clm_int index, IdKey blockId, size_t expectedSize)
{
    auto b = clm_bpa_find_block(blockId);
    if (b is null || b.type == CLM_BPA_TYPE_STRING || b.block_size != expectedSize)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_raw(id, index, b.data);
}
int clm_bpaa_insert(IdKey id, clm_int index, IdKey blockId, size_t expectedSize)
{
    auto b = clm_bpa_find_block(blockId);
    if (b is null || b.type == CLM_BPA_TYPE_STRING || b.block_size != expectedSize)
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_raw(id, index, b.data);
}
int clm_bpaa_append(IdKey id, IdKey blockId, size_t expectedSize)
{
    auto a = clm_bpaa_find_array(id);
    return a is null || a.count > int.max ? CLM_BPA_FAILURE :
        clm_bpaa_insert(id, cast(int)a.count, blockId, expectedSize);
}
int clm_bpaa_get_string(IdKey id, clm_int index, IdKey blockId, size_t expectedSize)
{
    auto b = clm_bpa_find_block(blockId);
    if (b is null || b.type != CLM_BPA_TYPE_STRING || expectedSize < b.length + 1)
        return CLM_BPA_FAILURE;
    ubyte[] text;
    auto result = clm_bpaa_get_string_raw(id, index, text, expectedSize);
    if (result < 0) return result;
    return clm_bpa_set_string(blockId, text) < 0 ? CLM_BPA_FAILURE : CLM_BPA_GET_SUCCESS;
}
int clm_bpaa_set_string(IdKey id, clm_int index, IdKey blockId, size_t expectedSize)
{
    auto b = clm_bpa_find_block(blockId);
    if (b is null || b.type != CLM_BPA_TYPE_STRING || b.data.length + 1 > expectedSize)
        return CLM_BPA_FAILURE;
    return clm_bpaa_set_string_raw(id, index, b.data) < 0 ? CLM_BPA_FAILURE : CLM_BPA_SET_SUCCESS;
}
int clm_bpaa_insert_string(IdKey id, clm_int index, IdKey blockId, size_t expectedSize)
{
    auto b = clm_bpa_find_block(blockId);
    if (b is null || b.type != CLM_BPA_TYPE_STRING || b.data.length + 1 > expectedSize)
        return CLM_BPA_FAILURE;
    return clm_bpaa_insert_raw(id, index, b.data);
}
int clm_bpaa_append_string(IdKey id, IdKey blockId, size_t expectedSize)
{
    auto a = clm_bpaa_find_array(id);
    return a is null || a.count > int.max ? CLM_BPA_FAILURE :
        clm_bpaa_insert_string(id, cast(int)a.count, blockId, expectedSize);
}

int clm_bpaa_draw(IdKey id, IdKey blockId, size_t expectedSize)
{
    ubyte[] value;
    auto a = clm_bpaa_find_array(id);
    auto b = clm_bpa_find_block(blockId);
    if (a is null || b is null || a.type == CLM_BPA_TYPE_STRING || b.type == CLM_BPA_TYPE_STRING ||
        (a.type != CLM_BPA_TYPE_STRUCT && a.type != b.type) || a.block_size != b.block_size ||
        expectedSize != a.block_size || a.count == 0)
        return CLM_BPA_FAILURE;
    auto r = clm_bpaa_draw_raw(id, value, expectedSize);
    if (r != CLM_BPA_GET_SUCCESS) return r;
    b.data = value;
    return r;
}
int clm_bpaa_draw_at(IdKey id, clm_int index, IdKey blockId, size_t expectedSize)
{
    ubyte[] value;
    auto a = clm_bpaa_find_array(id);
    auto b = clm_bpa_find_block(blockId);
    if (a is null || b is null || a.type == CLM_BPA_TYPE_STRING || b.type == CLM_BPA_TYPE_STRING ||
        (a.type != CLM_BPA_TYPE_STRUCT && a.type != b.type) || a.block_size != b.block_size ||
        expectedSize != a.block_size)
        return CLM_BPA_FAILURE;
    auto r = clm_bpaa_draw_at_raw(id, index, value, expectedSize);
    if (r != CLM_BPA_GET_SUCCESS) return r;
    b.data = value;
    return r;
}

int clm_bpaa_write_u32(IdKey destinationId, clm_uint value)
{
    auto b = clm_bpa_find_block(destinationId);
    if (b is null || b.type != CLM_BPA_TYPE_UNSIGNED_INT || b.block_size != 4)
        return CLM_BPA_FAILURE;
    import std.bitmanip : nativeToLittleEndian;
    b.data[] = nativeToLittleEndian(value);
    return CLM_BPA_GET_SUCCESS;
}
int clm_bpaa_write_i32(IdKey destinationId, clm_int value)
{
    auto b = clm_bpa_find_block(destinationId);
    if (b is null || b.type != CLM_BPA_TYPE_INT || b.block_size != 4)
        return CLM_BPA_FAILURE;
    import std.bitmanip : nativeToLittleEndian;
    b.data[] = nativeToLittleEndian(value);
    return CLM_BPA_GET_SUCCESS;
}
int clm_bpaa_write_byte(IdKey destinationId, clm_byte value)
{
    auto b = clm_bpa_find_block(destinationId);
    if (b is null || b.type != CLM_BPA_TYPE_BYTE || b.block_size != 1)
        return CLM_BPA_FAILURE;
    b.data[0] = value;
    return CLM_BPA_GET_SUCCESS;
}
int clm_bpaa_get_count(IdKey arrayId, IdKey destinationId)
{
    clm_uint n;
    return clm_bpaa_get_count_to(arrayId, n) == CLM_BPA_GET_SUCCESS
        ? clm_bpaa_write_u32(destinationId, n) : CLM_BPA_FAILURE;
}
int clm_bpaa_get_capacity(IdKey arrayId, IdKey destinationId)
{
    clm_uint n;
    return clm_bpaa_get_capacity_to(arrayId, n) == CLM_BPA_GET_SUCCESS
        ? clm_bpaa_write_u32(destinationId, n) : CLM_BPA_FAILURE;
}
int clm_bpaa_get_length(IdKey arrayId, IdKey destinationId)
{
    return clm_bpaa_get_count(arrayId, destinationId);
}
int clm_bpaa_get_current_index(IdKey arrayId, IdKey destinationId)
{
    clm_int n;
    return clm_bpaa_get_current_index_to(arrayId, n) == CLM_BPA_GET_SUCCESS
        ? clm_bpaa_write_i32(destinationId, n) : CLM_BPA_FAILURE;
}
int clm_bpaa_is_sealed(IdKey arrayId, IdKey destinationId)
{
    clm_byte n;
    return clm_bpaa_is_sealed_to(arrayId, n) == CLM_BPA_GET_SUCCESS
        ? clm_bpaa_write_byte(destinationId, n) : CLM_BPA_FAILURE;
}
int clm_bpaa_get_first(IdKey id, IdKey blockId, size_t size) { return clm_bpaa_get(id, 0, blockId, size); }
int clm_bpaa_get_last(IdKey id, IdKey blockId, size_t size)
{
    auto a = clm_bpaa_find_array(id);
    return a is null || a.count == 0 || a.count - 1 > int.max
        ? CLM_BPA_FAILURE : clm_bpaa_get(id, cast(int)(a.count - 1), blockId, size);
}
int clm_bpaa_get_current(IdKey id, IdKey blockId, size_t size)
{
    auto a = clm_bpaa_find_array(id);
    return a is null || a.index >= a.count || a.index > int.max
        ? CLM_BPA_FAILURE : clm_bpaa_get(id, cast(int)a.index, blockId, size);
}

int clm_bpa_shutdown()
{
    arraysById = null;
    blocksById = null;
    clm_bpa_global_arrays_capacity = clm_bpa_global_arrays_count = clm_bpa_global_arrays_tombstones = 0;
    clm_bpa_global_blocks_capacity = clm_bpa_global_blocks_count = clm_bpa_global_blocks_tombstones = 0;
    return clm_bpa_pool_cleanup();
}

int clm_bpa_get_to(IdKey id, ref ubyte[] destination, size_t size) { return clm_bpa_get(id, destination, size); }
int clm_bpa_set_from(IdKey id, const(ubyte)[] source) { return clm_bpa_set(id, source); }
int clm_bpa_get_string_to(IdKey id, out ubyte[] destination, size_t size) { 
    return clm_bpa_get_string(id, destination, size); 
}
int clm_bpa_set_string_from(IdKey id, const(ubyte)[] source) { return clm_bpa_set_string(id, source); }
int clm_bpaa_get_to(IdKey id, clm_int index, out ubyte[] destination, size_t size) { 
    return clm_bpaa_get_raw(id, index, destination, size); 
}
int clm_bpaa_get_first_to(IdKey id, out ubyte[] destination, size_t size) { 
    return clm_bpaa_get_first_raw(id, destination, size); 
}
int clm_bpaa_get_last_to(IdKey id, out ubyte[] destination, size_t size) { 
    return clm_bpaa_get_last_raw(id, destination, size); 
}
int clm_bpaa_get_current_to(IdKey id, out ubyte[] destination, size_t size) { 
    return clm_bpaa_get_current_raw(id, destination, size); 
}
int clm_bpaa_get_string_to(IdKey id, clm_int index, out ubyte[] destination, size_t size) { 
    return clm_bpaa_get_string_raw(id, index, destination, size); 
}
int clm_bpaa_set_from(IdKey id, clm_int index, const(ubyte)[] source) { 
    return clm_bpaa_set_raw(id, index, source); 
}
int clm_bpaa_set_string_from(IdKey id, clm_int index, const(ubyte)[] source) { 
    return clm_bpaa_set_string_raw(id, index, source); 
}
int clm_bpaa_insert_from(IdKey id, clm_int index, const(ubyte)[] source) { 
    return clm_bpaa_insert_raw(id, index, source); 
}
int clm_bpaa_append_from(IdKey id, const(ubyte)[] source) { 
    return clm_bpaa_append_raw(id, source); 
}
int clm_bpaa_draw_to(IdKey id, out ubyte[] destination, size_t size) { 
    return clm_bpaa_draw_raw(id, destination, size); 
}
int clm_bpaa_draw_at_to(IdKey id, clm_int index, out ubyte[] destination, size_t size) { 
    return clm_bpaa_draw_at_raw(id, index, destination, size); 
}
