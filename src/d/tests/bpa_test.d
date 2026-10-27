module tests.bpa_test;

import clm.bpa;
import tests.test;

private ubyte[] rawOf(ref int value)
{
    return (cast(ubyte*)&value)[0 .. int.sizeof];
}

private int intOf(const(ubyte)[] bytes)
{
    return bytes.length == int.sizeof ? *cast(const(int)*)bytes.ptr : int.min;
}

private int appendInt(ulong id, int value) { return clm_bpaa_append_raw(id, rawOf(value)); }

private int getInt(ulong id, int index, out int value)
{
    ubyte[] bytes;
    auto status = clm_bpaa_get_raw(id, index, bytes, int.sizeof);
    if (status == CLM_BPA_GET_SUCCESS) value = intOf(bytes);
    return status;
}

private int blockGet(ulong id, out int value)
{
    ubyte[] bytes;
    auto status = clm_bpa_get(id, bytes, int.sizeof);
    if (status == CLM_BPA_GET_SUCCESS) value = intOf(bytes);
    return status;
}

private int compareInts(const(ubyte)[] left, const(ubyte)[] right)
{
    auto l = intOf(left);
    auto r = intOf(right);
    return (l > r) - (l < r);
}

private string textOf(const(ubyte)[] bytes)
{
    return bytes.length == 0 ? "" : cast(string)bytes[0 .. $ - 1].idup;
}

int clmBpaTests()
{
    ClmTestResult result;
    clmTestBegin("CLM - Block Pool Allocation (BPA)");

    int value;
    int index;
    int[2] structValue = [123, 456];
    uint count;
    uint capacity;
    int[4] values = [4, 1, 3, 2];
    ubyte[] bytes;

    clmAssert(result, !clm_bpa_is_existing(1) && !clm_bpaa_is_existing(10),
        "new BPA block and array IDs are absent");
    clmAssert(result, clm_bpa_create(1, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS &&
        clm_bpa_is_existing(1) && !clm_bpaa_is_existing(1),
        "create a block in the block registry");
    value = 9;
    clmAssert(result, clm_bpa_replace(1, rawOf(value)) == CLM_BPA_SET_SUCCESS &&
        blockGet(1, value) == CLM_BPA_GET_SUCCESS && value == 9,
        "replace and retrieve a typed block value");
    clmAssert(result, clm_bpa_create(1, CLM_BPA_TYPE_INT) == CLM_BPA_FAILURE,
        "reject duplicate block IDs");
    clmAssert(result, clm_bpa_set(1, rawOf(values[0])) == CLM_BPA_SET_SUCCESS &&
        blockGet(1, value) == CLM_BPA_GET_SUCCESS && value == 4,
        "set and retrieve a block value");
    clmAssert(result, clm_bpa_get(1, bytes, 1) == CLM_BPA_FAILURE,
        "reject reads with a too-small capacity");
    clmAssert(result, clm_bpa_ref(1) == CLM_BPA_SUCCESS &&
        clm_bpa_unref(1) == CLM_BPA_SUCCESS &&
        blockGet(1, value) == CLM_BPA_GET_SUCCESS &&
        clm_bpa_unref(1) == CLM_BPA_SUCCESS &&
        blockGet(1, value) == CLM_BPA_FAILURE && !clm_bpa_is_existing(1),
        "reference counts keep a block alive until its final release");

    clmAssert(result, clm_bpa_create_sized(2, CLM_BPA_TYPE_STRUCT, structValue.sizeof) == CLM_BPA_SUCCESS &&
        clm_bpa_set(2, (cast(ubyte*)structValue.ptr)[0 .. structValue.sizeof]) == CLM_BPA_SET_SUCCESS &&
        clm_bpa_get(2, bytes, structValue.sizeof) == CLM_BPA_GET_SUCCESS &&
        (cast(int*)bytes.ptr)[0 .. 2] == structValue[] && clm_bpa_delete(2) == CLM_BPA_SUCCESS,
        "store and retrieve a fixed-size struct payload");
    ubyte[] text;
    clmAssert(result, clm_bpa_create(3, CLM_BPA_TYPE_STRING) == CLM_BPA_SUCCESS &&
        clm_bpa_set_string(3, cast(const(ubyte)[])"hello\0") == 5,
        "create and populate a string block");
    clmAssert(result, clm_bpa_get_string(3, text, 16) == 5 && textOf(text) == "hello",
        "retrieve string block contents");
    clmAssert(result, clm_bpa_get_string(3, text, 5) == CLM_BPA_FAILURE,
        "reject a string read with no room for the terminator");
    clmAssert(result, clm_bpa_set_string(3, cast(const(ubyte)[])"world!\0") == 6 &&
        clm_bpa_get_string(3, text, 16) == 6 && textOf(text) == "world!",
        "replace string block contents");
    clmAssert(result, clm_bpa_replace(3, cast(const(ubyte)[])"world! updated\0") == 14 &&
        clm_bpa_get_string(3, text, 32) == 14 && textOf(text) == "world! updated",
        "replace a string block with a longer value");
    clmAssert(result, clm_bpa_replace(999, cast(const(ubyte)[])"missing\0") == CLM_BPA_FAILURE &&
        clm_bpa_delete(3) == CLM_BPA_SUCCESS &&
        clm_bpa_create(4, CLM_BPA_TYPE_STRUCT) == CLM_BPA_FAILURE,
        "reject invalid replacements and unsized struct blocks");

    clmAssert(result, clm_bpaa_create(10, CLM_BPA_TYPE_INT, 1, 0) == CLM_BPA_SUCCESS &&
        clm_bpaa_is_existing(10) && !clm_bpa_is_existing(10),
        "create an integer array");
    foreach (item; values)
        clmAssert(result, appendInt(10, item) == CLM_BPA_SUCCESS, "append an integer to the array");
    clmAssert(result, clm_bpaa_get_count_to(10, count) == CLM_BPA_GET_SUCCESS &&
        count == 4 && clm_bpaa_get_capacity_to(10, capacity) == CLM_BPA_GET_SUCCESS &&
        capacity >= count, "report array count and capacity");
    clmAssert(result, clm_bpaa_get_first_raw(10, bytes, int.sizeof) == CLM_BPA_GET_SUCCESS && intOf(bytes) == 4 &&
        clm_bpaa_get_last_raw(10, bytes, int.sizeof) == CLM_BPA_GET_SUCCESS && intOf(bytes) == 2,
        "read the first and last array values");
    clmAssert(result, getInt(10, -1, value) == CLM_BPA_FAILURE && getInt(10, 4, value) == CLM_BPA_FAILURE,
        "reject invalid array indices");
    clmAssert(result, clm_bpaa_insert_raw(10, 1, rawOf(values[1])) == CLM_BPA_SUCCESS &&
        getInt(10, 1, value) == CLM_BPA_GET_SUCCESS && value == 1 &&
        clm_bpaa_set_raw(10, 1, rawOf(values[0])) == CLM_BPA_SET_SUCCESS,
        "insert and replace array elements");
    clmAssert(result, clm_bpaa_move_to(10, 2) == CLM_BPA_SUCCESS &&
        clm_bpaa_get_current_index_to(10, index) == CLM_BPA_GET_SUCCESS && index == 2 &&
        clm_bpaa_get_current_raw(10, bytes, int.sizeof) == CLM_BPA_GET_SUCCESS,
        "move the array cursor and read the current item");
    clmAssert(result, clm_bpaa_move_previous(10) == CLM_BPA_SUCCESS &&
        clm_bpaa_move_next(10) == CLM_BPA_SUCCESS &&
        clm_bpaa_move_last(10) == CLM_BPA_SUCCESS &&
        clm_bpaa_move_next(10) == CLM_BPA_FAILURE &&
        clm_bpaa_reset(10) == CLM_BPA_SUCCESS,
        "navigate and reset the array cursor");
    clmAssert(result, clm_bpaa_extend(10, 2) == CLM_BPA_SUCCESS &&
        clm_bpaa_get_capacity_to(10, capacity) == CLM_BPA_GET_SUCCESS &&
        clm_bpaa_get_count_to(10, capacity) == CLM_BPA_GET_SUCCESS &&
        clm_bpaa_shrink(10, capacity) == CLM_BPA_SUCCESS,
        "extend and shrink array capacity");
    clmAssert(result, clm_bpaa_reverse(10) == CLM_BPA_SUCCESS &&
        clm_bpaa_sort_with(10, &compareInts) == CLM_BPA_SUCCESS &&
        getInt(10, 0, value) == CLM_BPA_GET_SUCCESS && value == 1,
        "reverse and sort an array");
    clmAssert(result, clm_bpaa_shift_left(10, 1, 1) == CLM_BPA_SUCCESS &&
        clm_bpaa_shift_right(10, 1, 1) == CLM_BPA_SUCCESS &&
        clm_bpaa_swap(10, 0, 1) == CLM_BPA_SUCCESS,
        "shift and swap array elements");
    clmAssert(result, clm_bpaa_shuffle(10) == CLM_BPA_SUCCESS, "shuffle array elements");
    clmAssert(result, clm_bpaa_draw_at_raw(10, 2, bytes, int.sizeof) == CLM_BPA_GET_SUCCESS,
        "draw an element by index");
    foreach (_; 0 .. 5)
        clmAssert(result, clm_bpaa_draw_raw(10, bytes, int.sizeof) == CLM_BPA_GET_SUCCESS,
            "draw an element from the array deck");
    ubyte sealed;
    clmAssert(result, clm_bpaa_is_sealed_to(10, sealed) == CLM_BPA_GET_SUCCESS && sealed == 0 &&
        clm_bpaa_seal(10) == CLM_BPA_SUCCESS &&
        clm_bpaa_is_sealed_to(10, sealed) == CLM_BPA_GET_SUCCESS && sealed != 0,
        "query and change array sealed state");
    clmAssert(result, clm_bpaa_clear(10) == CLM_BPA_FAILURE,
        "reject clearing a sealed array");
    clmAssert(result, clm_bpaa_unseal(10) == CLM_BPA_SUCCESS,
        "unseal the array before clearing");
    clmAssert(result, clm_bpaa_clear(10) == CLM_BPA_SUCCESS,
        "clear an unsealed array");
    clmAssert(result, clm_bpaa_get_count_to(10, count) == CLM_BPA_GET_SUCCESS && count == 0,
        "cleared array has no elements");
    clmAssert(result, clm_bpaa_draw_raw(10, bytes, int.sizeof) == CLM_BPA_FAILURE,
        "reject drawing from an empty array");
    clmAssert(result, clm_bpaa_ref(10) == CLM_BPA_SUCCESS &&
        clm_bpaa_unref(10) == CLM_BPA_SUCCESS &&
        clm_bpaa_unref(10) == CLM_BPA_SUCCESS && !clm_bpaa_is_existing(10) &&
        clm_bpaa_get_count_to(10, count) == CLM_BPA_FAILURE,
        "release an array after its final reference");

    clmAssert(result, clm_bpaa_create(11, CLM_BPA_TYPE_STRING, 0, 1) == CLM_BPA_SUCCESS &&
        clm_bpaa_append_raw(11, cast(const(ubyte)[])"hello\0") == CLM_BPA_SUCCESS &&
        clm_bpaa_append_raw(11, cast(const(ubyte)[])"\0") == CLM_BPA_SUCCESS &&
        clm_bpaa_get_string_raw(11, 0, text, 16) == 5 && textOf(text) == "hello" &&
        clm_bpaa_set_string_raw(11, 1, cast(const(ubyte)[])"world!\0") == 6,
        "append, get, and update string array elements");
    clmAssert(result, clm_bpaa_move_first(11) == CLM_BPA_SUCCESS &&
        clm_bpaa_get_length_to(11, count) == CLM_BPA_GET_SUCCESS && count == 5 &&
        clm_bpaa_remove(11, 0) == CLM_BPA_SUCCESS &&
        clm_bpaa_get_string_raw(11, 0, text, 16) == 6 && textOf(text) == "world!",
        "navigate and remove string array elements");
    clmAssert(result, clm_bpaa_clear(11) == CLM_BPA_SUCCESS &&
        clm_bpaa_delete(11) == CLM_BPA_SUCCESS && !clm_bpaa_is_existing(11),
        "clear and delete a string array");

    clmAssert(result, clm_bpaa_create(13, CLM_BPA_TYPE_INT, 0, 0) == CLM_BPA_SUCCESS,
        "create source array for slicing");
    foreach (item; values)
        clmAssert(result, appendInt(13, item) == CLM_BPA_SUCCESS, "append source slice item");
    int first;
    int second;
    clmAssert(result, clm_bpaa_slice(13, 1, 2, 14) == CLM_BPA_SUCCESS &&
        clm_bpaa_get_count_to(14, count) == CLM_BPA_GET_SUCCESS && count == 2,
        "copy a range into a new array");
    clmAssert(result, getInt(14, 0, first) == CLM_BPA_GET_SUCCESS &&
        getInt(14, 1, second) == CLM_BPA_GET_SUCCESS && first == 1 && second == 3,
        "preserve selected values in a copied slice");
    clmAssert(result, clm_bpaa_slice(13, 4, 0, 19) == CLM_BPA_SUCCESS &&
        clm_bpaa_get_count_to(19, count) == CLM_BPA_GET_SUCCESS && count == 0 &&
        clm_bpaa_delete(19) == CLM_BPA_SUCCESS,
        "allow an empty copied slice at the end");
    const(ubyte)[] borrowed;
    clmAssert(result, clm_bpaa_slice_borrow(13, 1, 2, borrowed) == CLM_BPA_SUCCESS &&
        borrowed.length == 2 * int.sizeof, "borrow a contiguous fixed-width slice");
    clmAssert(result, clm_bpaa_slice(13, 3, 2, 18) == CLM_BPA_FAILURE &&
        clm_bpaa_slice(13, -1, 1, 18) == CLM_BPA_FAILURE,
        "reject invalid slice bounds");
    clmAssert(result, clm_bpaa_delete(14) == CLM_BPA_SUCCESS &&
        clm_bpaa_delete(13) == CLM_BPA_SUCCESS, "delete slice arrays");

    enum ulong wideId = 0x100000001UL;
    clmAssert(result, clm_bpaa_create(wideId, CLM_BPA_TYPE_INT, 0, 0) == CLM_BPA_SUCCESS &&
        appendInt(wideId, values[0]) == CLM_BPA_SUCCESS &&
        getInt(wideId, 0, value) == CLM_BPA_GET_SUCCESS && value == 4 &&
        clm_bpaa_delete(wideId) == CLM_BPA_SUCCESS,
        "support IDs wider than 32 bits");

    foreach (offset; 0 .. 32)
        clmAssert(result, clm_bpa_create(1000 + offset, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS,
            "grow the BPA block registry");
    foreach (offset; 0 .. 32)
        clmAssert(result, clm_bpa_delete(1000 + offset) == CLM_BPA_SUCCESS,
            "release BPA registry entries");
    foreach (offset; 0 .. 16)
        clmAssert(result, clm_bpaa_create(3000 + offset, CLM_BPA_TYPE_INT, 0, 0) == CLM_BPA_SUCCESS,
            "grow the BPAA array registry");
    foreach (offset; 0 .. 16)
        clmAssert(result, clm_bpaa_delete(3000 + offset) == CLM_BPA_SUCCESS,
            "release BPAA registry entries");

    size_t activeBefore;
    size_t reservedBefore;
    size_t activeAfter;
    size_t reservedAfter;
    clmAssert(result, clm_bpa_pool_stats(int.sizeof, activeBefore, reservedBefore) == CLM_BPA_SUCCESS &&
        clm_bpa_create(50001, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS,
        "query pool state and allocate a pooled block");
    value = 777;
    clmAssert(result, clm_bpa_set(50001, rawOf(value)) == CLM_BPA_SET_SUCCESS &&
        clm_bpa_pool_stats(int.sizeof, activeAfter, reservedAfter) == CLM_BPA_SUCCESS &&
        activeAfter == activeBefore + 1 && reservedAfter >= activeAfter,
        "track active and reserved pool capacity");
    clmAssert(result, clm_bpa_delete(50001) == CLM_BPA_SUCCESS &&
        clm_bpa_pool_stats(int.sizeof, activeAfter, reservedBefore) == CLM_BPA_SUCCESS &&
        activeAfter == activeBefore && reservedBefore == reservedAfter,
        "reuse released pool slots while retaining the slab");
    clmAssert(result, clm_bpa_pool_cleanup() == CLM_BPA_SUCCESS,
        "clean up unused BPA pool allocations");

    {
        enum id = 52000UL;
        clm_bpaa_array_node array;
        int[2] initial = [2, 4];
        int third = 6;
        int observed = 0;
        clmAssert(result, clm_bpaa_create(id, CLM_BPA_TYPE_INT, 1, 0) == CLM_BPA_SUCCESS,
            "create fixed-width array for advanced borrow");
        clmAssert(result, appendInt(id, initial[0]) == CLM_BPA_SUCCESS,
            "append initial advanced-array item");
        clmAssert(result, clm_bpaa_advanced_borrow(id, array) == CLM_BPA_SUCCESS && array !is null,
            "borrow array metadata");
        clmAssert(result, array.count == 1 && array.capacity == 1 && array.block_size == int.sizeof,
            "borrow exposes array metadata");
        clmAssert(result, clm_bpaa_advanced_data(array) !is null,
            "borrow exposes fixed-width payload");
        *cast(int*)clm_bpaa_advanced_data(array).ptr = 3;
        ubyte[] got;
        clmAssert(result, clm_bpaa_advanced_get_raw(array, 0, got, int.sizeof) == CLM_BPA_GET_SUCCESS &&
            intOf(got) == 3, "direct payload updates are visible through advanced API");
        clmAssert(result, clm_bpaa_advanced_set_raw(array, 0, rawOf(initial[1])) == CLM_BPA_SET_SUCCESS,
            "advanced set avoids ID lookup");
        clmAssert(result, clm_bpaa_advanced_extend(array, 1) == CLM_BPA_SUCCESS,
            "advanced extend reserves capacity");
        clmAssert(result, clm_bpaa_advanced_append_raw(array, rawOf(third)) == CLM_BPA_SUCCESS,
            "advanced append uses borrowed metadata");
        clmAssert(result, clm_bpaa_advanced_insert_raw(array, 1, rawOf(initial[0])) == CLM_BPA_SUCCESS,
            "advanced insert uses borrowed metadata");
        clmAssert(result, array.count == 3, "advanced insert updates count");
        clmAssert(result, clm_bpaa_advanced_get_raw(array, 1, got, int.sizeof) == CLM_BPA_GET_SUCCESS &&
            intOf(got) == 2, "advanced insert preserves expected item order");
        clmAssert(result, clm_bpaa_advanced_remove(array, 0) == CLM_BPA_SUCCESS, "advanced remove succeeds");
        clmAssert(result, array.count == 2, "advanced remove updates array metadata");
        clmAssert(result, clm_bpaa_advanced_shrink(array, array.count) == CLM_BPA_SUCCESS,
            "advanced shrink succeeds");
        clmAssert(result, array.capacity == array.count, "advanced shrink releases unused capacity");
        clmAssert(result, clm_bpaa_advanced_clear(array) == CLM_BPA_SUCCESS && array.count == 0,
            "advanced clear preserves borrowed array");
        clmAssert(result, clm_bpaa_unref(id) == CLM_BPA_SUCCESS && clm_bpaa_is_existing(id),
            "borrow retains array against ordinary unref");
        clmAssert(result, clm_bpaa_advanced_release(array) == CLM_BPA_SUCCESS && array is null,
            "release advanced borrow and null pointer");
        clmAssert(result, !clm_bpaa_is_existing(id),
            "releasing final reference destroys advanced-borrowed array");
        clmAssert(result, clm_bpaa_advanced_release(array) == CLM_BPA_FAILURE,
            "advanced release rejects already released pointer");
    }
    {
        enum id = 52001UL;
        clm_bpaa_array_node array;
        ubyte[] got;
        clmAssert(result, clm_bpaa_advanced_borrow(id, array) == CLM_BPA_FAILURE && array is null,
            "advanced borrow rejects missing array");
        clmAssert(result, clm_bpaa_create(id, CLM_BPA_TYPE_STRING, 0, 0) == CLM_BPA_SUCCESS,
            "create string array for advanced borrow");
        clmAssert(result, clm_bpaa_advanced_borrow(id, array) == CLM_BPA_SUCCESS,
            "borrow string-array metadata");
        clmAssert(result, clm_bpaa_advanced_data(array) is null,
            "advanced fixed-width data accessor rejects string array");
        clmAssert(result, clm_bpaa_advanced_get_raw(array, 0, got, int.sizeof) == CLM_BPA_FAILURE,
            "advanced fixed-width operations reject string arrays");
        clmAssert(result, clm_bpaa_advanced_release(array) == CLM_BPA_SUCCESS,
            "release string-array borrow");
        clmAssert(result, clm_bpaa_delete(id) == CLM_BPA_SUCCESS,
            "delete string array after releasing borrow");
    }
    {
        enum id = 53000UL;
        int a = 10, b = 20, c = 30, v;
        clmAssert(result, clm_bpaa_create(id, CLM_BPA_TYPE_INT, 3, 0) == CLM_BPA_SUCCESS &&
            appendInt(id, a) == CLM_BPA_SUCCESS && appendInt(id, b) == CLM_BPA_SUCCESS &&
            appendInt(id, c) == CLM_BPA_SUCCESS, "create array for copy and slice_to");
        clmAssert(result, clm_bpaa_copy(id, 0, 2) == CLM_BPA_SUCCESS, "copy item within array");
        clmAssert(result, getInt(id, 2, v) == CLM_BPA_GET_SUCCESS && v == 10, "copied item has source value");
        clmAssert(result, clm_bpaa_copy(id, 3, 0) == CLM_BPA_FAILURE, "copy rejects out-of-range source");
        clmAssert(result, clm_bpaa_copy(id, 0, -1) == CLM_BPA_FAILURE, "copy rejects negative destination");
        ubyte[8] buf;
        clmAssert(result, clm_bpaa_slice_to(id, 1, 2, buf[]) == CLM_BPA_SUCCESS, "slice_to copies a range");
        clmAssert(result, intOf(buf[0 .. 4]) == 20 && intOf(buf[4 .. 8]) == 10, "slice_to contents match");
        clmAssert(result, clm_bpaa_slice_to(id, 1, 2, buf[0 .. 4]) == CLM_BPA_FAILURE,
            "slice_to rejects small buffer");
        clmAssert(result, clm_bpaa_slice_to(id, 2, 2, buf[]) == CLM_BPA_FAILURE,
            "slice_to rejects out-of-range length");
        clmAssert(result, clm_bpaa_shrink(id, 3) == CLM_BPA_FAILURE, "shrink rejects capacity not smaller");
        clmAssert(result, clm_bpaa_shrink(id, 2) == CLM_BPA_FAILURE, "shrink rejects capacity below count");
        clmAssert(result, clm_bpaa_delete(id) == CLM_BPA_SUCCESS, "delete copy/slice_to array");
    }

    // Regression coverage: block copies retain their managed pool allocation.
    {
        enum ulong sourceId = 770001;
        enum ulong copyId = 770002;
        size_t before, reserved;
        size_t after, copyReservedAfter;
        int number = 12345;
        clm_bpa_pool_stats(int.sizeof, before, reserved);
        clmAssert(result,
            clm_bpa_create(sourceId, CLM_BPA_TYPE_INT) == CLM_BPA_SUCCESS &&
            clm_bpa_set(sourceId, rawOf(number)) == CLM_BPA_SET_SUCCESS &&
            clm_bpa_copy_block(sourceId, copyId) == CLM_BPA_SUCCESS &&
            blockGet(copyId, number) == CLM_BPA_GET_SUCCESS && number == 12345,
            "copy a block without replacing its managed pool allocation");
        clmAssert(result,
            clm_bpa_delete(copyId) == CLM_BPA_SUCCESS &&
            clm_bpa_delete(sourceId) == CLM_BPA_SUCCESS &&
            clm_bpa_pool_stats(int.sizeof, after, copyReservedAfter) == CLM_BPA_SUCCESS &&
            after == before,
            "block-copy deletion restores the pool active count");
    }
    // A sealed array must reject mutation, including operations that don't resize.
    {
        enum ulong fixedId = 770003;
        int number = 5;
        ubyte[] supplied = rawOf(number).dup;
        clmAssert(result,
            clm_bpaa_create(fixedId, CLM_BPA_TYPE_INT, 1, 0) == CLM_BPA_SUCCESS &&
            clm_bpaa_append_raw(fixedId, supplied) == CLM_BPA_SUCCESS &&
            clm_bpaa_seal(fixedId) == CLM_BPA_SUCCESS,
            "construct and seal a fixed-width array");
        clmAssert(result,
            clm_bpaa_set_raw(fixedId, 0, supplied) == CLM_BPA_FAILURE &&
            clm_bpaa_insert_raw(fixedId, 0, supplied) == CLM_BPA_FAILURE &&
            clm_bpaa_remove(fixedId, 0) == CLM_BPA_FAILURE &&
            clm_bpaa_clear(fixedId) == CLM_BPA_FAILURE &&
            clm_bpaa_reverse(fixedId) == CLM_BPA_FAILURE &&
            clm_bpaa_shuffle(fixedId) == CLM_BPA_FAILURE,
            "sealed arrays reject direct and bulk mutations");
        clmAssert(result,
            clm_bpaa_unseal(fixedId) == CLM_BPA_SUCCESS &&
            clm_bpaa_remove(fixedId, 0) == CLM_BPA_SUCCESS &&
            clm_bpaa_delete(fixedId) == CLM_BPA_SUCCESS,
            "unsealed arrays can be modified and deleted");
    }

    clmTestSummary(result.passed, result.failed);
    return result.failed;
}
