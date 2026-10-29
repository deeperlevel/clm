package clm
// Native Odin directory port: experimental subset.
import "core:os"
DIRECTORY_SUCCESS :: 0
DIRECTORY_FAILURE :: -1
DIRECTORY_UNSUPPORTED :: -2
directory_exists_as :: proc(path: string) -> bool { return os.is_dir(path) }
directory_create_as :: proc(path: string) -> bool { return os.make_directory(path) }
