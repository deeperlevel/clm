//! UTF-8 name registry and sequential ID generator.

const std = @import("std");
const dictionary = @import("clm_dictionary");
const bpa = @import("clm_bpa");

pub const clm_id = dictionary.clm_id;
pub const CLM_ID_NOT_FOUND: clm_id = std.math.maxInt(clm_id);

pub const ID_STORAGE_ID: clm_id = 0x0F000001;
pub const CLM_ID_INDEX_STORAGE_ID: clm_id = 0x0F000002;
pub const CLM_ID_NAME_BUFFER_ID: clm_id = 0x0F000003;
pub const CLM_ID_NAMES_STORAGE_ID: clm_id = 0x0F000004;
pub const CLM_ID_VALUES_STORAGE_ID: clm_id = 0x0F000005;

pub const clm_id_status = i32;
pub const CLM_BPA_FAILURE = bpa.CLM_BPA_FAILURE;
pub const CLM_BPA_SUCCESS = bpa.CLM_BPA_SUCCESS;
pub const CLM_BPA_GET_SUCCESS = bpa.CLM_BPA_GET_SUCCESS;
pub const CLM_BPA_SET_SUCCESS = bpa.CLM_BPA_SET_SUCCESS;
const failure = bpa.CLM_BPA_FAILURE;
const success = bpa.CLM_BPA_SUCCESS;
const get_success = bpa.CLM_BPA_GET_SUCCESS;
const set_success = bpa.CLM_BPA_SET_SUCCESS;

const Entry = struct {
    name: []u8,
    value: clm_id,
};

const allocator = std.heap.page_allocator;
var entries: std.ArrayList(Entry) = .empty;
var storage_initialized = false;

/// Global ID-generator and registry state.
pub var clm_id_generator_value: clm_id = 0;
pub var clm_id_next_entry_value: clm_id = 0;
pub var clm_id_active_index: clm_id = ID_STORAGE_ID;
pub var clm_id_generator_exhausted = false;
pub var clm_id_case_sensitive = true;

fn ensureStorage() void {
    storage_initialized = true;
}

fn effectiveName(name: []const u8) []const u8 {
    return std.mem.sliceTo(name, 0);
}

fn validName(name: []const u8) bool {
    var index: usize = 0;
    while (index < name.len) {
        const first = name[index];
        if (first <= 0x7f) {
            index += 1;
        } else if (first >= 0xc2 and first <= 0xdf) {
            if (index + 1 >= name.len or name[index + 1] < 0x80 or name[index + 1] > 0xbf)
                return false;
            index += 2;
        } else if (first >= 0xe0 and first <= 0xef) {
            if (index + 2 >= name.len) return false;
            const second = name[index + 1];
            const third = name[index + 2];
            if (second < 0x80 or second > 0xbf or third < 0x80 or third > 0xbf or
                (first == 0xe0 and second < 0xa0) or
                (first == 0xed and second >= 0xa0))
                return false;
            index += 3;
        } else if (first >= 0xf0 and first <= 0xf4) {
            if (index + 3 >= name.len) return false;
            const second = name[index + 1];
            const third = name[index + 2];
            const fourth = name[index + 3];
            if (second < 0x80 or second > 0xbf or third < 0x80 or third > 0xbf or
                fourth < 0x80 or fourth > 0xbf or
                (first == 0xf0 and second < 0x90) or
                (first == 0xf4 and second >= 0x90))
                return false;
            index += 4;
        } else {
            return false;
        }
    }
    return true;
}

fn foldAscii(character: u8, case_sensitive: bool) u8 {
    if (!case_sensitive and character >= 'A' and character <= 'Z')
        return character + ('a' - 'A');
    return character;
}

/// Compares names exactly, or with ASCII-only case folding when requested.
fn namesEqual(left: []const u8, right: []const u8, case_sensitive: bool) bool {
    if (left.len != right.len) return false;
    for (left, right) |left_byte, right_byte| {
        if (foldAscii(left_byte, case_sensitive) != foldAscii(right_byte, case_sensitive))
            return false;
    }
    return true;
}

fn findName(name: []const u8, case_sensitive: bool) ?usize {
    for (entries.items, 0..) |entry, index| {
        if (namesEqual(entry.name, name, case_sensitive)) return index;
    }
    return null;
}

fn validInput(name: []const u8) ?[]const u8 {
    const text = effectiveName(name);
    if (text.len == 0 or text.len > std.math.maxInt(u32) or !validName(text))
        return null;
    return text;
}

/// Inserts a new, nonempty name mapping and rejects duplicate effective names.
/// Multiple names may map to the same numeric ID.
/// @param name Name to register, compared according to the active case policy.
/// @param id Numeric ID associated with the name.
/// @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid/duplicate input.
pub fn clm_id_register(name: []const u8, id: clm_id) clm_id_status {
    const text = validInput(name) orelse return failure;
    ensureStorage();
    if (findName(text, clm_id_case_sensitive) != null) return failure;
    const owned_name = allocator.dupe(u8, text) catch return failure;
    entries.append(allocator, .{ .name = owned_name, .value = id }) catch {
        allocator.free(owned_name);
        return failure;
    };
    clm_id_next_entry_value +%= 1;
    return set_success;
}

/// Maps a name to an ID; alias for clm_id_register.
/// @param name Nonempty UTF-8 name to register.
/// @param id Numeric ID; multiple distinct names may share it.
/// @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for a duplicate/invalid name.
pub fn clm_id_map(name: []const u8, id: clm_id) clm_id_status {
    return clm_id_register(name, id);
}

/// Resolves a registered name to its numeric ID.
/// @param name Name to look up using the active case policy.
/// @return The mapped ID, or CLM_ID_NOT_FOUND when absent/invalid.
pub fn clm_id_get(name: []const u8) clm_id {
    const text = validInput(name) orelse return CLM_ID_NOT_FOUND;
    ensureStorage();
    const index = findName(text, clm_id_case_sensitive) orelse return CLM_ID_NOT_FOUND;
    return entries.items[index].value;
}

/// Resolves a name and reports success separately from the mapped ID.
/// This permits CLM_ID_NOT_FOUND to be used as an ordinary mapped ID.
/// @param name Name to look up.
/// @param out_id Receives the numeric ID; must not be null.
/// @return CLM_BPA_GET_SUCCESS, or CLM_BPA_FAILURE if absent/invalid.
pub fn clm_id_try_get(name: []const u8, out_id: ?*clm_id) clm_id_status {
    const output = out_id orelse return failure;
    const text = validInput(name) orelse return failure;
    ensureStorage();
    const index = findName(text, clm_id_case_sensitive) orelse return failure;
    output.* = entries.items[index].value;
    return get_success;
}

/// Inserts a mapping or replaces the ID for an existing effective name.
/// Updating retains the originally registered spelling and insertion order.
/// @param name Name to insert or update.
/// @param id New numeric ID.
/// @return CLM_BPA_SET_SUCCESS, or CLM_BPA_FAILURE for invalid input/allocation.
pub fn clm_id_set(name: []const u8, id: clm_id) clm_id_status {
    const text = validInput(name) orelse return failure;
    ensureStorage();
    if (findName(text, clm_id_case_sensitive)) |index| {
        entries.items[index].value = id;
        return set_success;
    }
    return clm_id_register(text, id);
}

fn removeAt(index: usize) void {
    allocator.free(entries.items[index].name);
    _ = entries.orderedRemove(index);
}

/// Removes one mapping by name, leaving mappings for aliases intact.
/// @param name Name to remove under the active case policy.
/// @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if the name is absent/invalid.
pub fn clm_id_remove(name: []const u8) clm_id_status {
    const text = validInput(name) orelse return failure;
    ensureStorage();
    const index = findName(text, clm_id_case_sensitive) orelse return failure;
    removeAt(index);
    return success;
}

/// Removes every registered name that maps to the supplied numeric ID.
/// @param id Numeric ID whose aliases should all be removed.
/// @return CLM_BPA_SUCCESS if at least one mapping was removed; otherwise failure.
pub fn clm_id_unmap(id: clm_id) clm_id_status {
    ensureStorage();
    var removed = false;
    var index: usize = 0;
    while (index < entries.items.len) {
        if (entries.items[index].value == id) {
            removeAt(index);
            removed = true;
        } else {
            index += 1;
        }
    }
    return if (removed) success else failure;
}

/// Removes one mapping by name; equivalent to clm_id_remove.
/// @param name Name to remove.
/// @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if absent/invalid.
pub fn clm_id_unmap_name(name: []const u8) clm_id_status {
    return clm_id_remove(name);
}

/// Returns whether exact-case matching is enabled.
pub fn clm_id_is_case_sensitive() bool {
    return clm_id_case_sensitive;
}

/// Changes name matching policy after validating all existing names.
/// Case-insensitive matching folds ASCII letters; non-ASCII bytes compare exactly.
/// If folding creates a collision, state is unchanged.
/// @param enabled true for exact matching; false for ASCII case-insensitive.
/// @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if validation/allocation fails.
pub fn clm_id_set_case_sensitive(enabled: bool) clm_id_status {
    ensureStorage();
    if (enabled == clm_id_case_sensitive) return success;
    for (entries.items, 0..) |entry, index| {
        for (entries.items[index + 1 ..]) |other| {
            if (namesEqual(entry.name, other.name, enabled)) return failure;
        }
    }
    clm_id_case_sensitive = enabled;
    clm_id_active_index = if (enabled) ID_STORAGE_ID else CLM_ID_INDEX_STORAGE_ID;
    return success;
}

/// Removes all mappings while preserving the active matching policy.
pub fn clm_id_clear() clm_id_status {
    ensureStorage();
    for (entries.items) |entry| allocator.free(entry.name);
    entries.clearRetainingCapacity();
    clm_id_active_index = ID_STORAGE_ID;
    clm_id_next_entry_value = 0;
    return success;
}

/// Releases private registry storage; the next mapping operation recreates it.
/// Registry matching mode and ID generator state are retained.
/// @return CLM_BPA_SUCCESS, or CLM_BPA_FAILURE if storage was not initialized.
pub fn clm_id_shutdown() clm_id_status {
    if (!storage_initialized) return failure;
    for (entries.items) |entry| allocator.free(entry.name);
    entries.deinit(allocator);
    entries = .empty;
    storage_initialized = false;
    clm_id_active_index = ID_STORAGE_ID;
    clm_id_next_entry_value = 0;
    return success;
}

/// Resets sequential ID generation so the next call returns zero.
pub fn clm_id_gen_reset() void {
    clm_id_generator_value = 0;
    clm_id_generator_exhausted = false;
}

/// Returns the next sequential ID, beginning at zero.
/// Generated IDs are not checked against IDs supplied to registration functions.
/// After the available range is exhausted, this returns CLM_ID_NOT_FOUND.
pub fn clm_id_gen_next() clm_id {
    if (clm_id_generator_exhausted) return CLM_ID_NOT_FOUND;
    const result = clm_id_generator_value;
    if (result == CLM_ID_NOT_FOUND - 1) {
        clm_id_generator_exhausted = true;
    } else {
        clm_id_generator_value += 1;
    }
    return result;
}
