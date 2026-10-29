//! Compile-target information; no runtime OS probing.
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
pub fn clm_platform_get_os() -> u32 {
    if cfg!(target_os="windows") { return PLATFORM_INFO_OS_WINDOWS; }
    if cfg!(target_os="android") { return PLATFORM_INFO_OS_ANDROID; }
    if cfg!(target_os="ios") { return PLATFORM_INFO_OS_IOS; }
    if cfg!(target_os="macos") { return PLATFORM_INFO_OS_MACOS; }
    if cfg!(target_os="linux") { return PLATFORM_INFO_OS_LINUX; }
    if cfg!(target_os="freebsd") { return PLATFORM_INFO_OS_BSD; }
    if cfg!(target_os="netbsd") { return PLATFORM_INFO_OS_BSD; }
    if cfg!(target_os="openbsd") { return PLATFORM_INFO_OS_BSD; }    PLATFORM_INFO_OS_UNKNOWN
}
pub fn clm_platform_get_os_name() -> &'static str { match clm_platform_get_os(){
PLATFORM_INFO_OS_WINDOWS=>"Windows",PLATFORM_INFO_OS_ANDROID=>"Android",PLATFORM_INFO_OS_IOS=>"iOS",PLATFORM_INFO_OS_MACOS=>"macOS",PLATFORM_INFO_OS_LINUX=>"Linux",PLATFORM_INFO_OS_BSD=>"BSD",_=>"Unknown"}}
pub fn clm_platform_get_os_version() -> &'static str {"Unknown (no compile-time OS target version)"}
pub fn clm_platform_get_arch() -> u32 {
    if cfg!(target_arch="x86_64") {return PLATFORM_INFO_ARCH_X64;}
    if cfg!(target_arch="x86") {return PLATFORM_INFO_ARCH_X86;}
    if cfg!(target_arch="aarch64") {return PLATFORM_INFO_ARCH_ARM64;}
    if cfg!(target_arch="arm") {return PLATFORM_INFO_ARCH_ARM32;}
    if cfg!(target_arch="riscv64") {return PLATFORM_INFO_ARCH_RISCV64;}
    if cfg!(target_arch="riscv32") {return PLATFORM_INFO_ARCH_RISCV32;}
    if cfg!(target_arch="wasm32") {return PLATFORM_INFO_ARCH_WASM32;}
    if cfg!(target_arch="wasm64") {return PLATFORM_INFO_ARCH_WASM64;}    PLATFORM_INFO_ARCH_UNKNOWN
}
pub fn clm_platform_get_arch_name() -> &'static str {match clm_platform_get_arch(){
PLATFORM_INFO_ARCH_X64=>"x64",PLATFORM_INFO_ARCH_X86=>"x86",PLATFORM_INFO_ARCH_ARM64=>"ARM64",PLATFORM_INFO_ARCH_ARM32=>"ARM32",PLATFORM_INFO_ARCH_RISCV64=>"RISC-V 64",PLATFORM_INFO_ARCH_RISCV32=>"RISC-V 32",PLATFORM_INFO_ARCH_WASM32=>"WebAssembly 32",PLATFORM_INFO_ARCH_WASM64=>"WebAssembly 64",_=>"Unknown"}}
pub fn clm_platform_get_bits() -> usize {core::mem::size_of::<usize>()*8}
pub fn clm_platform_get_endian() -> u32 {if cfg!(target_endian="little"){PLATFORM_INFO_ENDIAN_LITTLE}else{PLATFORM_INFO_ENDIAN_BIG}}
pub fn clm_platform_get_compiler() -> u32 { PLATFORM_INFO_COMPILER_UNKNOWN }
pub fn clm_platform_get_compiler_name() -> &'static str {"Rust compiler (not GCC/Clang/MSVC)"}
pub fn clm_platform_get_compiler_version() -> &'static str {"Unknown (rustc version not built in)"}
pub fn clm_platform_get_path_separator() -> char {if clm_platform_is_windows(){'\\'}else{'/'}}
pub fn clm_platform_get_newline() -> &'static str {if clm_platform_is_windows(){"\r\n"}else{"\n"}}
pub fn clm_platform_is_windows() -> bool {clm_platform_get_os()==PLATFORM_INFO_OS_WINDOWS}
pub fn clm_platform_is_linux() -> bool {clm_platform_get_os()==PLATFORM_INFO_OS_LINUX}
pub fn clm_platform_is_bsd() -> bool {clm_platform_get_os()==PLATFORM_INFO_OS_BSD}
pub fn clm_platform_is_macos() -> bool {clm_platform_get_os()==PLATFORM_INFO_OS_MACOS}
pub fn clm_platform_is_android() -> bool {clm_platform_get_os()==PLATFORM_INFO_OS_ANDROID}
pub fn clm_platform_is_ios() -> bool {clm_platform_get_os()==PLATFORM_INFO_OS_IOS}
pub fn clm_platform_is_mobile() -> bool {clm_platform_is_android()||clm_platform_is_ios()}
pub fn clm_platform_is_desktop() -> bool {clm_platform_is_windows()||clm_platform_is_linux()||clm_platform_is_bsd()||clm_platform_is_macos()}
pub fn copy_to(text:&str, buffer:&mut [u8])->Result<usize,()> {if buffer.len()<text.len()+1{return Err(())}buffer[..text.len()].copy_from_slice(text.as_bytes());buffer[text.len()]=0;Ok(text.len())}
