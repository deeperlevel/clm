package clm
// Native Odin file port: experimental subset, simple path operations.
import "core:os"
FILE_SUCCESS :: 0
FILE_FAILURE :: -1
FILE_READ :: 1
FILE_WRITE :: 2
FILE_APPEND :: 4
FILE_CREATE :: 8
FILE_TRUNCATE :: 16
FILE_EXCLUSIVE :: 32
// An open-file ID registry will need integration with the Odin BPA implementation.
file_exists_as :: proc(path: string) -> bool { return os.is_file(path) }
file_read_all_to :: proc(path: string) -> ([]u8, bool) {
 data, ok := os.read_entire_file(path)
 return data, ok
}
file_write_all_from :: proc(path: string, data: []u8) -> bool {
 return os.write_entire_file(path, data)
}
