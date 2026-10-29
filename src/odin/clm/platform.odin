package clm_platform
import "core:runtime"
import "core:os"
PLATFORM_INFO_OS_WINDOWS :: 0x0f100030
PLATFORM_INFO_OS_MACOS :: 0x0f100031
PLATFORM_INFO_OS_LINUX :: 0x0f100032
PLATFORM_INFO_OS_BSD :: 0x0f100033
PLATFORM_INFO_OS_ANDROID :: 0x0f100034
PLATFORM_INFO_OS_IOS :: 0x0f100035
PLATFORM_INFO_OS_UNKNOWN :: 0x0f10003f
PLATFORM_INFO_ARCH_X86 :: 0x0f100040
PLATFORM_INFO_ARCH_X64 :: 0x0f100041
PLATFORM_INFO_ARCH_ARM32 :: 0x0f100042
PLATFORM_INFO_ARCH_ARM64 :: 0x0f100043
PLATFORM_INFO_ARCH_RISCV32 :: 0x0f100044
PLATFORM_INFO_ARCH_RISCV64 :: 0x0f100045
PLATFORM_INFO_ARCH_WASM32 :: 0x0f100046
PLATFORM_INFO_ARCH_WASM64 :: 0x0f100047
PLATFORM_INFO_ARCH_UNKNOWN :: 0x0f10004f
PLATFORM_INFO_COMPILER_MSVC :: 0x0f100050
PLATFORM_INFO_COMPILER_GCC :: 0x0f100051
PLATFORM_INFO_COMPILER_CLANG :: 0x0f100052
PLATFORM_INFO_COMPILER_UNKNOWN :: 0x0f10005f
PLATFORM_INFO_ENDIAN_LITTLE :: 0x0f100060
PLATFORM_INFO_ENDIAN_BIG :: 0x0f100061
PLATFORM_INFO_ENDIAN_UNKNOWN :: 0x0f10006f
// OS-specific compile target from ODIN_OS.
clm_platform_get_os :: proc() -> int {
 switch ODIN_OS {
 case .Windows: return PLATFORM_INFO_OS_WINDOWS
 case .Linux: return PLATFORM_INFO_OS_LINUX
 case .Darwin: return PLATFORM_INFO_OS_MACOS
 case .FreeBSD, .OpenBSD, .NetBSD: return PLATFORM_INFO_OS_BSD
 default: return PLATFORM_INFO_OS_UNKNOWN
 }
}
clm_platform_get_os_name :: proc() -> string {
 switch clm_platform_get_os() {
 case PLATFORM_INFO_OS_WINDOWS: return "Windows"
 case PLATFORM_INFO_OS_LINUX: return "Linux"
 case PLATFORM_INFO_OS_MACOS: return "macOS"
 case PLATFORM_INFO_OS_BSD: return "BSD"
 case PLATFORM_INFO_OS_ANDROID: return "Android"
 case PLATFORM_INFO_OS_IOS: return "iOS"
 default: return "Unknown"
 }
}
clm_platform_get_os_version :: proc() -> string {return "Unknown (compile-time OS hint unavailable)"}
clm_platform_get_arch :: proc() -> int {
 switch ODIN_ARCH {
 case .amd64: return PLATFORM_INFO_ARCH_X64
 case .i386: return PLATFORM_INFO_ARCH_X86
 case .arm64: return PLATFORM_INFO_ARCH_ARM64
 default: return PLATFORM_INFO_ARCH_UNKNOWN
 }
}
clm_platform_get_arch_name :: proc() -> string {
 switch clm_platform_get_arch() {
 case PLATFORM_INFO_ARCH_X64: return "x64"
 case PLATFORM_INFO_ARCH_X86: return "x86"
 case PLATFORM_INFO_ARCH_ARM64: return "ARM64"
 default: return "Unknown"
 }
}
clm_platform_get_bits :: proc() -> int {return size_of(uintptr)*8}
clm_platform_get_endian :: proc() -> int {return PLATFORM_INFO_ENDIAN_LITTLE} // Target-specific override required for big-endian architectures.
clm_platform_get_compiler :: proc() -> int {return PLATFORM_INFO_COMPILER_UNKNOWN}
clm_platform_get_compiler_name :: proc() -> string {return "Odin compiler"}
clm_platform_get_compiler_version :: proc() -> string {return "Unknown"}
clm_platform_get_path_separator :: proc() -> byte {if clm_platform_is_windows(){return '\\'};return '/'}
clm_platform_get_newline :: proc() -> string {if clm_platform_is_windows(){return "\r\n"};return "\n"}
clm_platform_is_windows :: proc() -> bool {return clm_platform_get_os()==PLATFORM_INFO_OS_WINDOWS}
clm_platform_is_linux :: proc() -> bool {return clm_platform_get_os()==PLATFORM_INFO_OS_LINUX}
clm_platform_is_bsd :: proc() -> bool {return clm_platform_get_os()==PLATFORM_INFO_OS_BSD}
clm_platform_is_macos :: proc() -> bool {return clm_platform_get_os()==PLATFORM_INFO_OS_MACOS}
clm_platform_is_android :: proc() -> bool {return clm_platform_get_os()==PLATFORM_INFO_OS_ANDROID}
clm_platform_is_ios :: proc() -> bool {return clm_platform_get_os()==PLATFORM_INFO_OS_IOS}
clm_platform_is_mobile :: proc() -> bool {return clm_platform_is_android() || clm_platform_is_ios()}
clm_platform_is_desktop :: proc() -> bool {return clm_platform_is_windows() || clm_platform_is_linux() || clm_platform_is_bsd() || clm_platform_is_macos()}
