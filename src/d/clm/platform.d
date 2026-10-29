module clm.platform;
import core.stdc.stdint;
enum PLATFORM_INFO_OS_WINDOWS = 0x0f100030;
enum PLATFORM_INFO_OS_MACOS = 0x0f100031;
enum PLATFORM_INFO_OS_LINUX = 0x0f100032;
enum PLATFORM_INFO_OS_BSD = 0x0f100033;
enum PLATFORM_INFO_OS_ANDROID = 0x0f100034;
enum PLATFORM_INFO_OS_IOS = 0x0f100035;
enum PLATFORM_INFO_OS_UNKNOWN = 0x0f10003f;
enum PLATFORM_INFO_ARCH_X86 = 0x0f100040;
enum PLATFORM_INFO_ARCH_X64 = 0x0f100041;
enum PLATFORM_INFO_ARCH_ARM32 = 0x0f100042;
enum PLATFORM_INFO_ARCH_ARM64 = 0x0f100043;
enum PLATFORM_INFO_ARCH_RISCV32 = 0x0f100044;
enum PLATFORM_INFO_ARCH_RISCV64 = 0x0f100045;
enum PLATFORM_INFO_ARCH_WASM32 = 0x0f100046;
enum PLATFORM_INFO_ARCH_WASM64 = 0x0f100047;
enum PLATFORM_INFO_ARCH_UNKNOWN = 0x0f10004f;
enum PLATFORM_INFO_COMPILER_MSVC = 0x0f100050;
enum PLATFORM_INFO_COMPILER_GCC = 0x0f100051;
enum PLATFORM_INFO_COMPILER_CLANG = 0x0f100052;
enum PLATFORM_INFO_COMPILER_UNKNOWN = 0x0f10005f;
enum PLATFORM_INFO_ENDIAN_LITTLE = 0x0f100060;
enum PLATFORM_INFO_ENDIAN_BIG = 0x0f100061;
enum PLATFORM_INFO_ENDIAN_UNKNOWN = 0x0f10006f;
version(Windows){enum OS_ID=PLATFORM_INFO_OS_WINDOWS;enum OS_NAME="Windows";}
else version(Android){enum OS_ID=PLATFORM_INFO_OS_ANDROID;enum OS_NAME="Android";}
else version(iOS){enum OS_ID=PLATFORM_INFO_OS_IOS;enum OS_NAME="iOS";}
else version(OSX){enum OS_ID=PLATFORM_INFO_OS_MACOS;enum OS_NAME="macOS";}
else version(FreeBSD){enum OS_ID=PLATFORM_INFO_OS_BSD;enum OS_NAME="BSD";}
else version(NetBSD){enum OS_ID=PLATFORM_INFO_OS_BSD;enum OS_NAME="BSD";}
else version(OpenBSD){enum OS_ID=PLATFORM_INFO_OS_BSD;enum OS_NAME="BSD";}
else version(linux){enum OS_ID=PLATFORM_INFO_OS_LINUX;enum OS_NAME="Linux";}
else {enum OS_ID=PLATFORM_INFO_OS_UNKNOWN;enum OS_NAME="Unknown";}
version(X86_64){enum ARCH_ID=PLATFORM_INFO_ARCH_X64;enum ARCH_NAME="x64";}
else version(X86){enum ARCH_ID=PLATFORM_INFO_ARCH_X86;enum ARCH_NAME="x86";}
else version(AArch64){enum ARCH_ID=PLATFORM_INFO_ARCH_ARM64;enum ARCH_NAME="ARM64";}
else version(ARM){enum ARCH_ID=PLATFORM_INFO_ARCH_ARM32;enum ARCH_NAME="ARM32";}
else {enum ARCH_ID=PLATFORM_INFO_ARCH_UNKNOWN;enum ARCH_NAME="Unknown";}
version(BigEndian){enum ENDIAN_ID=PLATFORM_INFO_ENDIAN_BIG;}
else {enum ENDIAN_ID=PLATFORM_INFO_ENDIAN_LITTLE;}
int clm_platform_get_os(){return OS_ID;}
string clm_platform_get_os_name(){return OS_NAME;}
string clm_platform_get_os_version(){return "Unknown (compile-time OS hint unavailable)";}
int clm_platform_get_arch(){return ARCH_ID;}
string clm_platform_get_arch_name(){return ARCH_NAME;}
int clm_platform_get_bits(){return (void*).sizeof*8;}
int clm_platform_get_endian(){return ENDIAN_ID;}
int clm_platform_get_compiler(){return PLATFORM_INFO_COMPILER_UNKNOWN;}
string clm_platform_get_compiler_name(){return "D compiler (not C toolchain)";}
string clm_platform_get_compiler_version(){return "Unknown";}
char clm_platform_get_path_separator(){return OS_ID==PLATFORM_INFO_OS_WINDOWS?'\\':'/';}
string clm_platform_get_newline(){return OS_ID==PLATFORM_INFO_OS_WINDOWS?"\r\n":"\n";}
bool clm_platform_is_windows(){return OS_ID==PLATFORM_INFO_OS_WINDOWS;}
bool clm_platform_is_linux(){return OS_ID==PLATFORM_INFO_OS_LINUX;}
bool clm_platform_is_bsd(){return OS_ID==PLATFORM_INFO_OS_BSD;}
bool clm_platform_is_macos(){return OS_ID==PLATFORM_INFO_OS_MACOS;}
bool clm_platform_is_android(){return OS_ID==PLATFORM_INFO_OS_ANDROID;}
bool clm_platform_is_ios(){return OS_ID==PLATFORM_INFO_OS_IOS;}
bool clm_platform_is_mobile(){return clm_platform_is_android()||clm_platform_is_ios();}
bool clm_platform_is_desktop(){return clm_platform_is_windows()||clm_platform_is_linux()||clm_platform_is_bsd()||clm_platform_is_macos();}
int copy_to(string s, ref ubyte[] dst){if(dst.length<s.length+1)return -1;dst[0..s.length]=cast(ubyte[])s.dup;dst[s.length]=0;return cast(int)s.length;}
