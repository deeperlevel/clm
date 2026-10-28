//! Unicode strings and string arrays backed by BPA resources.
//! Text is always validated UTF-8 internally; encodings describe external bytes.
//! Character counts and indices are Unicode code points, not grapheme clusters.

const std = @import("std");
const bpa = @import("clm_bpa");
const allocator = std.heap.page_allocator;
const failure = bpa.CLM_BPA_FAILURE;
const success = bpa.CLM_BPA_SUCCESS;
const max_length = std.math.maxInt(i32);
var random = std.Random.DefaultPrng.init(1);

pub const CLM_STRING_ENCODING_UTF8: u8 = 1;
pub const CLM_STRING_ENCODING_LATIN1: u8 = 2;
pub const CLM_STRING_ENCODING_UTF16LE: u8 = 3;

/// Shared runtime default. Configure before concurrent use; not thread-safe.
/// Existing strings retain their preferred export encoding.
pub var clm_string_global_default_encoding: u8 = CLM_STRING_ENCODING_UTF8;

pub fn clm_string_encoding_supported(encoding: u8) bool {
    return encoding == CLM_STRING_ENCODING_UTF8 or
        encoding == CLM_STRING_ENCODING_LATIN1 or encoding == CLM_STRING_ENCODING_UTF16LE;
}

/// Changes the preferred byte-export encoding for subsequently created strings.
/// Does not change Unicode text or existing strings' preferences.
pub fn clm_string_set_default_encoding(encoding: u8) i32 {
    if (!clm_string_encoding_supported(encoding)) return failure;
    clm_string_global_default_encoding = encoding;
    return success;
}

/// Returns the runtime default byte-export encoding (initially UTF-8).
pub fn clm_string_get_default_encoding() u8 {
    return clm_string_global_default_encoding;
}

fn next(text: []const u8, offset: *usize) ?u21 {
    if (offset.* >= text.len) return null;
    const n = std.unicode.utf8ByteSequenceLength(text[offset.*]) catch return null;
    if (n > text.len - offset.*) return null;
    const cp = std.unicode.utf8Decode(text[offset.*..][0..n]) catch return null;
    offset.* += n;
    return cp;
}

fn measure(text: []const u8) ?i32 {
    if (text.len > max_length) return null;
    var offset: usize = 0;
    var count: i32 = 0;
    while (offset < text.len) {
        const cp = next(text, &offset) orelse return null;
        if (cp == 0) return null;
        count += 1;
    }
    return count;
}

fn byteOffset(text: []const u8, index: i32) ?usize {
    if (index < 0) return null;
    var offset: usize = 0;
    var i: i32 = 0;
    while (i < index) : (i += 1) _ = next(text, &offset) orelse return null;
    return offset;
}

fn range(text: []const u8, start: i32, length: i32) ?[]const u8 {
    const count = measure(text) orelse return null;
    if (start < 0 or length < 0 or start > count or length > count - start) return null;
    const first = byteOffset(text, start) orelse return null;
    const last = byteOffset(text, start + length) orelse return null;
    return text[first..last];
}

const Target = union(enum) {
    block: u64,
    item: struct { id: u64, index: i32 },

    fn text(target: Target) ?[]const u8 {
        const value = switch (target) {
            .block => |id| bpa.clm_bpa_string(id),
            .item => |item| if (arrayNode(item.id) != null) bpa.clm_bpaa_string(item.id, item.index) else null,
        } orelse return null;
        _ = measure(value) orelse return null;
        return value;
    }

    fn set(target: Target, value: []const u8) bool {
        return switch (target) {
            .block => |id| bpa.clm_bpa_set_string_raw(id, value) >= 0,
            .item => |item| bpa.clm_bpaa_set_string_raw(item.id, item.index, value) >= 0,
        };
    }
};

fn splice(target: Target, index: i32, removed: i32, inserted: []const u8, newline: bool) i32 {
    const current = target.text() orelse return failure;
    const inserted_count = measure(inserted) orelse return failure;
    const deleted = range(current, index, removed) orelse return failure;
    const first = byteOffset(current, index) orelse return failure;
    const extra: usize = @intFromBool(newline);
    const base = current.len - deleted.len;
    if (inserted.len > max_length - base or extra > max_length - base - inserted.len) return failure;
    const output = allocator.alloc(u8, base + inserted.len + extra) catch return failure;
    defer allocator.free(output);
    @memcpy(output[0..first], current[0..first]);
    @memcpy(output[first..][0..inserted.len], inserted);
    if (newline) output[first + inserted.len] = '\n';
    @memcpy(output[first + inserted.len + extra ..], current[first + deleted.len ..]);
    if (!target.set(output)) return failure;
    return inserted_count + @as(i32, @intFromBool(newline));
}

fn append(target: Target, suffix: []const u8, newline: bool) i32 {
    const current = target.text() orelse return failure;
    return splice(target, measure(current) orelse return failure, 0, suffix, newline);
}

fn remove(target: Target, index: i32, length: i32) i32 {
    const current = target.text() orelse return failure;
    _ = range(current, index, length) orelse return failure;
    if (length == 0) return 0;
    if (splice(target, index, length, "", false) < 0) return failure;
    return length;
}

fn replace(target: Target, search: []const u8, replacement: []const u8) i32 {
    const current = target.text() orelse return failure;
    if (search.len == 0) return failure;
    _ = measure(search) orelse return failure;
    _ = measure(replacement) orelse return failure;
    var count: usize = 0;
    var offset: usize = 0;
    while (std.mem.indexOfPos(u8, current, offset, search)) |found| {
        count += 1;
        offset = found + search.len;
    }
    if (count == 0) return 0;
    var size = current.len;
    if (replacement.len >= search.len) {
        const growth = replacement.len - search.len;
        if (growth != 0 and count > (max_length - size) / growth) return failure;
        size += count * growth;
    } else size -= count * (search.len - replacement.len);
    const output = allocator.alloc(u8, size) catch return failure;
    defer allocator.free(output);
    offset = 0;
    var written: usize = 0;
    while (std.mem.indexOfPos(u8, current, offset, search)) |found| {
        const prefix = current[offset..found];
        @memcpy(output[written..][0..prefix.len], prefix);
        written += prefix.len;
        @memcpy(output[written..][0..replacement.len], replacement);
        written += replacement.len;
        offset = found + search.len;
    }
    @memcpy(output[written..], current[offset..]);
    if (!target.set(output)) return failure;
    return @intCast(count);
}

/// Creates a UTF-8 string block. Returns false for invalid text, duplicate ID,
/// or allocation failure. Slice inputs exclude the storage terminator.
pub fn clm_string_new(id: u64, text: []const u8) bool {
    _ = measure(text) orelse return false;
    if (std.mem.indexOfScalar(u8, text, 0) != null or
        bpa.clm_bpa_create(id, bpa.CLM_BPA_TYPE_STRING) != success) return false;
    if (bpa.clm_bpa_set_string_raw(id, text) < 0) {
        _ = bpa.clm_bpa_delete(id);
        return false;
    }
    bpa.clm_bpa_find_block(id).?.encoding = clm_string_get_default_encoding();
    return true;
}

/// Deletes only string blocks, preserving blocks of other types.
pub fn clm_string_delete(id: u64) bool {
    const node = bpa.clm_bpa_find_block(id) orelse return false;
    return node.type == bpa.CLM_BPA_TYPE_STRING and bpa.clm_bpa_delete(id) == success;
}

/// Replaces a string's value; returns its code-point count or failure.
pub fn clm_string_set(id: u64, text: []const u8) i32 {
    const node = bpa.clm_bpa_find_block(id) orelse return failure;
    if (node.type != bpa.CLM_BPA_TYPE_STRING or std.mem.indexOfScalar(u8, text, 0) != null) return failure;
    const count = measure(text) orelse return failure;
    return if ((Target{ .block = id }).set(text)) count else failure;
}

/// Appends text and returns appended character count, or failure.
pub fn clm_string_append(id: u64, suffix: []const u8) i32 {
    return append(.{ .block = id }, suffix, false);
}

/// Appends text and a newline; returns appended character count or failure.
pub fn clm_string_append_line(id: u64, suffix: []const u8) i32 {
    return append(.{ .block = id }, suffix, true);
}

/// Inserts text at a character index; returns inserted character count or failure.
pub fn clm_string_insert(id: u64, index: i32, inserted: []const u8) i32 {
    return splice(.{ .block = id }, index, 0, inserted, false);
}

/// Removes a strict character range; returns removed character count or failure.
pub fn clm_string_remove(id: u64, index: i32, length: i32) i32 {
    return remove(.{ .block = id }, index, length);
}

/// Returns whether a string contains a valid UTF-8 search string.
pub fn clm_string_contains(id: u64, search: []const u8) bool {
    const text = (Target{ .block = id }).text() orelse return false;
    _ = measure(search) orelse return false;
    return std.mem.indexOf(u8, text, search) != null;
}

/// Returns string length in Unicode code points, or failure.
pub fn clm_string_length(id: u64) i32 {
    return measure((Target{ .block = id }).text() orelse return failure) orelse failure;
}

/// Replaces every non-overlapping occurrence; returns match count or failure.
/// An empty search is invalid; the replacement may be empty.
pub fn clm_string_replace(id: u64, search: []const u8, replacement: []const u8) i32 {
    return replace(.{ .block = id }, search, replacement);
}

fn encodedLength(text: []const u8, encoding: u8) ?usize {
    if (!clm_string_encoding_supported(encoding)) return null;
    var offset: usize = 0;
    var required: usize = 0;
    while (offset < text.len) {
        const cp = next(text, &offset) orelse return null;
        if (cp == 0 or (encoding == CLM_STRING_ENCODING_LATIN1 and cp > 255)) return null;
        const step: usize = switch (encoding) {
            CLM_STRING_ENCODING_UTF8 => std.unicode.utf8CodepointSequenceLength(cp) catch return null,
            CLM_STRING_ENCODING_LATIN1 => 1,
            else => if (cp < 0x10000) 2 else 4,
        };
        if (required > max_length - step) return null;
        required += step;
    }
    return required;
}

fn write16(output: []u8, value: u16) void {
    output[0] = @truncate(value);
    output[1] = @truncate(value >> 8);
}

/// Exports bytes in the string's preferred encoding, without NUL or BOM.
/// Null output queries the byte count; insufficient capacity fails without writing.
pub fn clm_string_get_bytes(id: u64, output: ?[]u8) i32 {
    const node = bpa.clm_bpa_find_block(id) orelse return failure;
    const text = (Target{ .block = id }).text() orelse return failure;
    const required = encodedLength(text, node.encoding) orelse return failure;
    const out = output orelse return @intCast(required);
    if (out.len < required) return failure;
    if (node.encoding == CLM_STRING_ENCODING_UTF8) {
        @memcpy(out[0..required], text);
    } else {
        var offset: usize = 0;
        var written: usize = 0;
        while (offset < text.len) {
            const cp = next(text, &offset).?;
            if (node.encoding == CLM_STRING_ENCODING_LATIN1) {
                out[written] = @intCast(cp);
                written += 1;
            } else if (cp < 0x10000) {
                write16(out[written..], @intCast(cp));
                written += 2;
            } else {
                const scalar = cp - 0x10000;
                write16(out[written..], @intCast(0xd800 | (scalar >> 10)));
                write16(out[written + 2 ..], @intCast(0xdc00 | (scalar & 0x3ff)));
                written += 4;
            }
        }
    }
    return @intCast(required);
}

fn read16(input: []const u8) u21 {
    return @as(u21, input[0]) | (@as(u21, input[1]) << 8);
}

/// Decodes an exact external byte sequence into an existing or new string.
/// Preserves resource identity on replacement and records the source encoding.
/// Rejects invalid UTF-8/UTF-16, embedded U+0000, and allocation failure.
pub fn clm_string_from_bytes(id: u64, encoding: u8, input: []const u8) i32 {
    if (!clm_string_encoding_supported(encoding) or input.len > max_length or
        (encoding == CLM_STRING_ENCODING_UTF16LE and input.len % 2 != 0)) return failure;
    const allocation = switch (encoding) {
        CLM_STRING_ENCODING_UTF8 => input.len,
        CLM_STRING_ENCODING_LATIN1 => std.math.mul(usize, input.len, 2) catch return failure,
        else => std.math.mul(usize, input.len / 2, 3) catch return failure,
    };
    if (allocation > max_length) return failure;
    const converted = allocator.alloc(u8, allocation) catch return failure;
    defer allocator.free(converted);
    var offset: usize = 0;
    var written: usize = 0;
    while (offset < input.len) {
        var cp: u21 = undefined;
        switch (encoding) {
            CLM_STRING_ENCODING_UTF8 => cp = next(input, &offset) orelse return failure,
            CLM_STRING_ENCODING_LATIN1 => {
                cp = input[offset];
                offset += 1;
            },
            else => {
                cp = read16(input[offset..]);
                offset += 2;
                if (cp >= 0xd800 and cp <= 0xdbff) {
                    if (input.len - offset < 2) return failure;
                    const tail = read16(input[offset..]);
                    if (tail < 0xdc00 or tail > 0xdfff) return failure;
                    cp = 0x10000 + ((cp - 0xd800) << 10) + (tail - 0xdc00);
                    offset += 2;
                } else if (cp >= 0xdc00 and cp <= 0xdfff) return failure;
            },
        }
        if (cp == 0) return failure;
        written += std.unicode.utf8Encode(cp, converted[written..]) catch return failure;
    }
    const text = converted[0..written];
    if (bpa.clm_bpa_find_block(id) != null) {
        if (clm_string_set(id, text) < 0) return failure;
    } else if (!clm_string_new(id, text)) return failure;
    bpa.clm_bpa_find_block(id).?.encoding = encoding;
    return success;
}

/// Changes export preference, not internal UTF-8 text. Fails without changing
/// the preference if the text cannot be represented in the requested encoding.
pub fn clm_string_change_encoding(id: u64, encoding: u8) i32 {
    const node = bpa.clm_bpa_find_block(id) orelse return failure;
    const text = (Target{ .block = id }).text() orelse return failure;
    _ = encodedLength(text, encoding) orelse return failure;
    node.encoding = encoding;
    return success;
}

/// Borrows a UTF-8 byte view of a strict Unicode code-point range.
/// The view is not NUL-terminated and is valid only while the source is live
/// and unmodified. An empty range at the end is valid. Null signals failure.
pub fn clm_string_slice_borrow(id: u64, start: i32, length: i32) ?[]const u8 {
    return range((Target{ .block = id }).text() orelse return null, start, length);
}

/// Creates an independent code-point slice at an unused destination ID.
/// Internal text is UTF-8; the source's preferred export encoding is inherited.
pub fn clm_string_slice(id: u64, start: i32, length: i32, destination: u64) i32 {
    const text = clm_string_slice_borrow(id, start, length) orelse return failure;
    const preference = bpa.clm_bpa_find_block(id).?.encoding;
    if (!clm_string_new(destination, text)) return failure;
    if (clm_string_change_encoding(destination, preference) != success) {
        _ = clm_string_delete(destination);
        return failure;
    }
    return success;
}

fn arrayNode(id: u64) ?*bpa.clm_bpaa_array_node {
    const node = bpa.clm_bpaa_find_array(id) orelse return null;
    if (node.type != bpa.CLM_BPA_TYPE_STRING or node.encoding != CLM_STRING_ENCODING_UTF8) return null;
    return node;
}

/// Creates a string array with canonical UTF-8 internal encoding.
pub fn clm_string_array_new(id: u64, reserved: i32) bool {
    return bpa.clm_bpaa_create(id, bpa.CLM_BPA_TYPE_STRING, reserved, CLM_STRING_ENCODING_UTF8) == success;
}

/// Returns the indexed string's code-point count, or failure.
pub fn clm_string_array_length_at(id: u64, index: i32) i32 {
    return measure((Target{ .item = .{ .id = id, .index = index } }).text() orelse return failure) orelse failure;
}

/// Returns whether the indexed string contains a valid UTF-8 search string.
pub fn clm_string_array_contains_at(id: u64, index: i32, search: []const u8) bool {
    const text = (Target{ .item = .{ .id = id, .index = index } }).text() orelse return false;
    _ = measure(search) orelse return false;
    return std.mem.indexOf(u8, text, search) != null;
}

/// Appends text to one array string; returns appended code-point count.
pub fn clm_string_array_append_at(id: u64, index: i32, suffix: []const u8) i32 {
    return append(.{ .item = .{ .id = id, .index = index } }, suffix, false);
}

/// Appends text and a newline to one array string.
pub fn clm_string_array_append_line_at(id: u64, index: i32, suffix: []const u8) i32 {
    return append(.{ .item = .{ .id = id, .index = index } }, suffix, true);
}

/// Inserts text at a code-point index in one array string.
pub fn clm_string_array_insert_at(id: u64, index: i32, character_index: i32, inserted: []const u8) i32 {
    return splice(.{ .item = .{ .id = id, .index = index } }, character_index, 0, inserted, false);
}

/// Removes a strict code-point range in one array string.
pub fn clm_string_array_remove_at(id: u64, index: i32, character_index: i32, length: i32) i32 {
    return remove(.{ .item = .{ .id = id, .index = index } }, character_index, length);
}

/// Replaces non-overlapping matches in one array string; returns match count.
pub fn clm_string_array_replace_at(id: u64, index: i32, search: []const u8, replacement: []const u8) i32 {
    return replace(.{ .item = .{ .id = id, .index = index } }, search, replacement);
}

/// Deletes only UTF-8 string arrays, preserving other resource types.
pub fn clm_string_array_delete(id: u64) bool {
    return arrayNode(id) != null and bpa.clm_bpaa_delete(id) == success;
}

/// Returns item count, or failure for a missing or oversized array.
pub fn clm_string_array_count(id: u64) i32 {
    const node = arrayNode(id) orelse return failure;
    return std.math.cast(i32, node.count) orelse failure;
}

/// Copies an array item into an existing string BPA block.
pub fn clm_string_array_get(id: u64, index: i32, destination: u64) i32 {
    const node = arrayNode(id) orelse return failure;
    const dest = bpa.clm_bpa_find_block(destination) orelse return failure;
    if (dest.type != bpa.CLM_BPA_TYPE_STRING) return failure;
    if (bpa.clm_bpaa_get_string(id, index, destination) != bpa.CLM_BPA_GET_SUCCESS) return failure;
    dest.encoding = node.encoding;
    return bpa.CLM_BPA_GET_SUCCESS;
}

/// Replaces an array item with an existing valid string block's value.
pub fn clm_string_array_set(id: u64, index: i32, source: u64) i32 {
    if (arrayNode(id) == null or (Target{ .block = source }).text() == null) return failure;
    return bpa.clm_bpaa_set_string(id, index, source);
}

/// Inserts a valid string block's value at an array index.
pub fn clm_string_array_insert(id: u64, index: i32, source: u64) i32 {
    if (arrayNode(id) == null or (Target{ .block = source }).text() == null) return failure;
    return bpa.clm_bpaa_insert_string(id, index, source);
}

/// Appends a valid string block's value to an array.
pub fn clm_string_array_append(id: u64, source: u64) i32 {
    if (arrayNode(id) == null or (Target{ .block = source }).text() == null) return failure;
    return bpa.clm_bpaa_append_string(id, source);
}

/// Removes an array item by zero-based index.
pub fn clm_string_array_remove(id: u64, index: i32) i32 {
    return if (arrayNode(id) != null) bpa.clm_bpaa_remove(id, index) else failure;
}

/// Clears items while preserving reserved capacity.
pub fn clm_string_array_clear(id: u64) i32 {
    return if (arrayNode(id) != null) bpa.clm_bpaa_clear(id) else failure;
}

/// Finds the first exact string match; returns its index or failure.
pub fn clm_string_array_find(id: u64, source: u64) i32 {
    const node = arrayNode(id) orelse return failure;
    const text = (Target{ .block = source }).text() orelse return failure;
    var index: u32 = 0;
    while (index < node.count and index <= max_length) : (index += 1) {
        if (std.mem.eql(u8, bpa.clm_bpaa_string(id, @intCast(index)) orelse return failure, text)) return @intCast(index);
    }
    return failure;
}

/// Sorts strings lexicographically by encoded bytes; resets cursor and deck.
pub fn clm_string_array_sort(id: u64) i32 {
    const node = arrayNode(id) orelse return failure;
    const Order = struct {
        fn less(_: void, left: []u8, right: []u8) bool {
            return std.mem.order(u8, left, right) == .lt;
        }
    };
    std.mem.sort([]u8, node.items.strings[0..node.count], {}, Order.less);
    node.index = 0;
    node.deck_size = node.count;
    return success;
}

/// Randomly shuffles string items and resets cursor and draw deck.
pub fn clm_string_array_shuffle(id: u64) i32 {
    const node = arrayNode(id) orelse return failure;
    var index = node.count;
    while (index > 1) : (index -= 1) {
        const other = random.random().uintLessThan(u32, index);
        std.mem.swap([]u8, &node.items.strings[index - 1], &node.items.strings[other]);
    }
    node.index = 0;
    node.deck_size = node.count;
    return success;
}

/// Moves the array cursor to the first item.
pub fn clm_string_array_move_first(id: u64) i32 {
    return if (arrayNode(id) != null) bpa.clm_bpaa_move_first(id) else failure;
}

/// Moves the array cursor to the previous item.
pub fn clm_string_array_move_previous(id: u64) i32 {
    return if (arrayNode(id) != null) bpa.clm_bpaa_move_previous(id) else failure;
}

/// Moves the array cursor to the next item.
pub fn clm_string_array_move_next(id: u64) i32 {
    return if (arrayNode(id) != null) bpa.clm_bpaa_move_next(id) else failure;
}

/// Moves the array cursor to the last item.
pub fn clm_string_array_move_last(id: u64) i32 {
    return if (arrayNode(id) != null) bpa.clm_bpaa_move_last(id) else failure;
}

/// Moves the array cursor to a specified item.
pub fn clm_string_array_move_to(id: u64, index: i32) i32 {
    return if (arrayNode(id) != null) bpa.clm_bpaa_move_to(id, index) else failure;
}

/// Copies the current item into an existing string block.
pub fn clm_string_array_get_current(id: u64, destination: u64) i32 {
    const node = arrayNode(id) orelse return failure;
    return clm_string_array_get(id, std.math.cast(i32, node.index) orelse return failure, destination);
}

/// Draws an indexed item from the active deck into an existing string block.
pub fn clm_string_array_draw_at(id: u64, index: i32, destination: u64) i32 {
    const node = arrayNode(id) orelse return failure;
    if (index < 0 or @as(u32, @intCast(index)) >= node.deck_size) return failure;
    if (clm_string_array_get(id, index, destination) != bpa.CLM_BPA_GET_SUCCESS) return failure;
    const last = node.deck_size - 1;
    std.mem.swap([]u8, &node.items.strings[@intCast(index)], &node.items.strings[last]);
    node.deck_size -= 1;
    return bpa.CLM_BPA_GET_SUCCESS;
}

/// Draws the next item, shuffling again when the active deck is exhausted.
pub fn clm_string_array_draw(id: u64, destination: u64) i32 {
    const node = arrayNode(id) orelse return failure;
    if (node.deck_size == 0 and clm_string_array_shuffle(id) != success) return failure;
    return clm_string_array_draw_at(id, 0, destination);
}
