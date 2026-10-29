const builtin = @import("builtin");
pub const PLATFORM_INFO_OS_WINDOWS: u32 = 0x0f100030;
pub const PLATFORM_INFO_OS_MACOS: u32 = 0x0f100031;
pub const PLATFORM_INFO_OS_LINUX: u32 = 0x0f100032;
pub const PLATFORM_INFO_OS_BSD: u32 = 0x0f100033;
pub const PLATFORM_INFO_OS_ANDROID: u32 = 0x0f100034;
pub const PLATFORM_INFO_OS_IOS: u32 = 0x0f100035;
pub const PLATFORM_INFO_OS_UNKNOWN: u32 = 0x0f10003f;
pub const PLATFORM_INFO_ARCH_X86: u32 = 0x0f100040;
pub const PLATFORM_INFO_ARCH_X64: u32 = 0x0f100041;
pub const PLATFORM_INFO_ARCH_ARM32: u32 = 0x0f100042;
pub const PLATFORM_INFO_ARCH_ARM64: u32 = 0x0f100043;
pub const PLATFORM_INFO_ARCH_RISCV32: u32 = 0x0f100044;
pub const PLATFORM_INFO_ARCH_RISCV64: u32 = 0x0f100045;
pub const PLATFORM_INFO_ARCH_WASM32: u32 = 0x0f100046;
pub const PLATFORM_INFO_ARCH_WASM64: u32 = 0x0f100047;
pub const PLATFORM_INFO_ARCH_UNKNOWN: u32 = 0x0f10004f;
pub const PLATFORM_INFO_COMPILER_MSVC: u32 = 0x0f100050;
pub const PLATFORM_INFO_COMPILER_GCC: u32 = 0x0f100051;
pub const PLATFORM_INFO_COMPILER_CLANG: u32 = 0x0f100052;
pub const PLATFORM_INFO_COMPILER_UNKNOWN: u32 = 0x0f10005f;
pub const PLATFORM_INFO_ENDIAN_LITTLE: u32 = 0x0f100060;
pub const PLATFORM_INFO_ENDIAN_BIG: u32 = 0x0f100061;
pub const PLATFORM_INFO_ENDIAN_UNKNOWN: u32 = 0x0f10006f;
pub fn clm_platform_get_os() u32 {return switch(builtin.target.os.tag){.windows=>PLATFORM_INFO_OS_WINDOWS,.macos=>PLATFORM_INFO_OS_MACOS,.linux=>PLATFORM_INFO_OS_LINUX,.android=>PLATFORM_INFO_OS_ANDROID,.ios=>PLATFORM_INFO_OS_IOS,.freebsd,.netbsd,.openbsd=>PLATFORM_INFO_OS_BSD,else=>PLATFORM_INFO_OS_UNKNOWN};}
pub fn clm_platform_get_os_name() []const u8 {return switch(clm_platform_get_os()){PLATFORM_INFO_OS_WINDOWS=>"Windows",PLATFORM_INFO_OS_MACOS=>"macOS",PLATFORM_INFO_OS_LINUX=>"Linux",PLATFORM_INFO_OS_BSD=>"BSD",PLATFORM_INFO_OS_ANDROID=>"Android",PLATFORM_INFO_OS_IOS=>"iOS",else=>"Unknown"};}
pub fn clm_platform_get_os_version() []const u8 {return "Unknown (compile-time OS hint unavailable)";}
pub fn clm_platform_get_arch() u32 {return switch(builtin.target.cpu.arch){.x86=>PLATFORM_INFO_ARCH_X86,.x86_64=>PLATFORM_INFO_ARCH_X64,.arm=>PLATFORM_INFO_ARCH_ARM32,.aarch64=>PLATFORM_INFO_ARCH_ARM64,.riscv32=>PLATFORM_INFO_ARCH_RISCV32,.riscv64=>PLATFORM_INFO_ARCH_RISCV64,.wasm32=>PLATFORM_INFO_ARCH_WASM32,.wasm64=>PLATFORM_INFO_ARCH_WASM64,else=>PLATFORM_INFO_ARCH_UNKNOWN};}
pub fn clm_platform_get_arch_name() []const u8 {return switch(clm_platform_get_arch()){PLATFORM_INFO_ARCH_X86=>"x86",PLATFORM_INFO_ARCH_X64=>"x64",PLATFORM_INFO_ARCH_ARM32=>"ARM32",PLATFORM_INFO_ARCH_ARM64=>"ARM64",PLATFORM_INFO_ARCH_RISCV32=>"RISC-V 32",PLATFORM_INFO_ARCH_RISCV64=>"RISC-V 64",PLATFORM_INFO_ARCH_WASM32=>"WebAssembly 32",PLATFORM_INFO_ARCH_WASM64=>"WebAssembly 64",else=>"Unknown"};}
pub fn clm_platform_get_bits() u32 {return @bitSizeOf(usize);}
pub fn clm_platform_get_endian() u32 {return if(builtin.target.cpu.arch.endian()==.little) PLATFORM_INFO_ENDIAN_LITTLE else PLATFORM_INFO_ENDIAN_BIG;}
pub fn clm_platform_get_compiler() u32 {return PLATFORM_INFO_COMPILER_UNKNOWN;}
pub fn clm_platform_get_compiler_name() []const u8 {return "Zig (not GCC/Clang/MSVC)";}
pub fn clm_platform_get_compiler_version() []const u8 {return "Unknown";}
pub fn clm_platform_get_path_separator() u8 {return if(clm_platform_is_windows()) '\\' else '/';}
pub fn clm_platform_get_newline() []const u8 {return if(clm_platform_is_windows()) "\r\n" else "\n";}
pub fn clm_platform_is_windows() bool {return clm_platform_get_os()==PLATFORM_INFO_OS_WINDOWS;}
pub fn clm_platform_is_linux() bool {return clm_platform_get_os()==PLATFORM_INFO_OS_LINUX;}
pub fn clm_platform_is_bsd() bool {return clm_platform_get_os()==PLATFORM_INFO_OS_BSD;}
pub fn clm_platform_is_macos() bool {return clm_platform_get_os()==PLATFORM_INFO_OS_MACOS;}
pub fn clm_platform_is_android() bool {return clm_platform_get_os()==PLATFORM_INFO_OS_ANDROID;}
pub fn clm_platform_is_ios() bool {return clm_platform_get_os()==PLATFORM_INFO_OS_IOS;}
pub fn clm_platform_is_mobile() bool {return clm_platform_is_android() or clm_platform_is_ios();}
pub fn clm_platform_is_desktop() bool {return clm_platform_is_windows() or clm_platform_is_linux() or clm_platform_is_bsd() or clm_platform_is_macos();}
pub fn copy_to(s:[]const u8,buf:[]u8) bool {if(buf.len<s.len+1)return false;@memcpy(buf[0..s.len],s);buf[s.len]=0;return true;}
