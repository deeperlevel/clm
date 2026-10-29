//! CLM Zig file port. Experimental subset, native std.fs API (Zig 0.13-style).
const std = @import("std");
pub const SUCCESS: i32 = 0;
pub const FAILURE: i32 = -1;
pub const UNSUPPORTED: i32 = -2;
pub const READ: u32 = 1;
pub const WRITE: u32 = 2;
pub const APPEND: u32 = 4;
pub const CREATE: u32 = 8;
pub const TRUNCATE: u32 = 16;
pub const EXCLUSIVE: u32 = 32;
pub const Registry = struct {
    allocator: std.mem.Allocator,
    open: std.AutoHashMap(u64, std.fs.File),
    retired: std.AutoHashMap(u64, void),
    pub fn init(allocator: std.mem.Allocator) Registry { return .{ .allocator = allocator, .open = std.AutoHashMap(u64, std.fs.File).init(allocator), .retired = std.AutoHashMap(u64, void).init(allocator) }; }
    pub fn deinit(self: *Registry) void { var it = self.open.valueIterator(); while (it.next()) |file| file.close(); self.open.deinit(); self.retired.deinit(); }
    pub fn open_as(self: *Registry, id: u64, path: []const u8, mode: u32) i32 {
        if (self.open.contains(id) or self.retired.contains(id) or path.len == 0 or mode & 7 == 0 or mode & ~@as(u32, 63) != 0) return FAILURE;
        if ((mode & EXCLUSIVE != 0 and mode & CREATE == 0) or (mode & TRUNCATE != 0 and (mode & WRITE == 0 or mode & APPEND != 0))) return FAILURE;
        const cwd = std.fs.cwd();
        const file = if (mode & CREATE != 0) cwd.createFile(path, .{ .read = mode & READ != 0, .truncate = mode & TRUNCATE != 0, .exclusive = mode & EXCLUSIVE != 0 }) catch return FAILURE else cwd.openFile(path, .{ .mode = if (mode & (WRITE | APPEND) != 0) .read_write else .read_only }) catch return FAILURE;
        self.open.put(id, file) catch { file.close(); return FAILURE; };
        return SUCCESS;
    }
    pub fn close(self: *Registry, id: u64) i32 { const entry = self.open.fetchRemove(id) orelse return FAILURE; entry.value.close(); self.retired.put(id, {}) catch return FAILURE; return SUCCESS; }
    pub fn read_to(self: *Registry, id: u64, buffer: []u8) ?usize { const file = self.open.getPtr(id) orelse return null; return file.read(buffer) catch null; }
    pub fn write_from(self: *Registry, id: u64, buffer: []const u8) ?usize { const file = self.open.getPtr(id) orelse return null; return file.write(buffer) catch null; }
};
pub fn read_all_to(allocator: std.mem.Allocator, path: []const u8) ![]u8 { return try std.fs.cwd().readFileAlloc(allocator, path, std.math.maxInt(usize)); }
pub fn write_all_from(path: []const u8, data: []const u8) !void { const f = try std.fs.cwd().createFile(path, .{}); defer f.close(); try f.writeAll(data); }
