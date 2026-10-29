//! CLM Zig directory port. Experimental subset (Zig 0.13-style).
const std = @import("std");
pub const SUCCESS: i32 = 0;
pub const FAILURE: i32 = -1;
pub const UNSUPPORTED: i32 = -2;
pub fn exists_as(path: []const u8) bool { var dir = std.fs.cwd().openDir(path, .{}) catch return false; dir.close(); return true; }
pub fn create_as(path: []const u8) i32 { std.fs.cwd().makeDir(path) catch return FAILURE; return SUCCESS; }
pub fn create_recursive_as(path: []const u8) i32 { std.fs.cwd().makePath(path) catch return FAILURE; return SUCCESS; }
pub fn delete_as(path: []const u8) i32 { std.fs.cwd().deleteDir(path) catch return FAILURE; return SUCCESS; }
pub fn find_entries_as(allocator: std.mem.Allocator, path: []const u8) !std.ArrayList([]u8) {
 var result = std.ArrayList([]u8).init(allocator); errdefer { for (result.items) |p| allocator.free(p); result.deinit(); }
 var dir = try std.fs.cwd().openDir(path, .{ .iterate = true }); defer dir.close();
 var it = dir.iterate(); while (try it.next()) |entry| { const name = try std.fs.path.join(allocator, &.{path, entry.name}); try result.append(name); }
 return result;
}
