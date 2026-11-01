#pragma once

#include "bpa.hpp"
#include "string.hpp"

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#include <sdkddkver.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace clm::platform {

#define CLM_STR_INNER(x) #x
#define CLM_STR(x) CLM_STR_INNER(x)

inline constexpr clm_id PLATFORM_BLOCK_OS_VERSION_ID       = (clm_id)0x0f100021u;
inline constexpr clm_id PLATFORM_BLOCK_COMPILER_VERSION_ID = (clm_id)0x0f100022u;
inline constexpr clm_id PLATFORM_BLOCK_NEWLINE_ID          = (clm_id)0x0f100023u;

inline constexpr clm_id PLATFORM_INFO_OS_WINDOWS = (clm_id)0x0f100030u;
inline constexpr clm_id PLATFORM_INFO_OS_MACOS   = (clm_id)0x0f100031u;
inline constexpr clm_id PLATFORM_INFO_OS_LINUX   = (clm_id)0x0f100032u;
inline constexpr clm_id PLATFORM_INFO_OS_BSD     = (clm_id)0x0f100033u;
inline constexpr clm_id PLATFORM_INFO_OS_ANDROID = (clm_id)0x0f100034u;
inline constexpr clm_id PLATFORM_INFO_OS_IOS     = (clm_id)0x0f100035u;
inline constexpr clm_id PLATFORM_INFO_OS_UNKNOWN = (clm_id)0x0f10003fu;

inline constexpr clm_id PLATFORM_INFO_ARCH_X86     = (clm_id)0x0f100040u;
inline constexpr clm_id PLATFORM_INFO_ARCH_X64     = (clm_id)0x0f100041u;
inline constexpr clm_id PLATFORM_INFO_ARCH_ARM32   = (clm_id)0x0f100042u;
inline constexpr clm_id PLATFORM_INFO_ARCH_ARM64   = (clm_id)0x0f100043u;
inline constexpr clm_id PLATFORM_INFO_ARCH_RISCV32 = (clm_id)0x0f100044u;
inline constexpr clm_id PLATFORM_INFO_ARCH_RISCV64 = (clm_id)0x0f100045u;
inline constexpr clm_id PLATFORM_INFO_ARCH_WASM32  = (clm_id)0x0f100046u;
inline constexpr clm_id PLATFORM_INFO_ARCH_WASM64  = (clm_id)0x0f100047u;
inline constexpr clm_id PLATFORM_INFO_ARCH_UNKNOWN = (clm_id)0x0f10004fu;

inline constexpr clm_id PLATFORM_INFO_COMPILER_MSVC    = (clm_id)0x0f100050u;
inline constexpr clm_id PLATFORM_INFO_COMPILER_GCC     = (clm_id)0x0f100051u;
inline constexpr clm_id PLATFORM_INFO_COMPILER_CLANG   = (clm_id)0x0f100052u;
inline constexpr clm_id PLATFORM_INFO_COMPILER_UNKNOWN = (clm_id)0x0f10005fu;

inline constexpr clm_id PLATFORM_INFO_ENDIAN_LITTLE  = (clm_id)0x0f100060u;
inline constexpr clm_id PLATFORM_INFO_ENDIAN_BIG     = (clm_id)0x0f100061u;
inline constexpr clm_id PLATFORM_INFO_ENDIAN_UNKNOWN = (clm_id)0x0f10006fu;

#if defined(_WIN32)
inline constexpr clm_id CLM_PLATFORM_OS_ID = PLATFORM_INFO_OS_WINDOWS;
inline constexpr const char* CLM_PLATFORM_OS_NAME = "Windows";
#if defined(_WIN32_WINNT)
inline constexpr const char* CLM_PLATFORM_OS_VERSION = "Target " CLM_STR(_WIN32_WINNT);
#else
inline constexpr const char* CLM_PLATFORM_OS_VERSION = "Unknown";
#endif
#elif defined(__ANDROID__)
inline constexpr clm_id CLM_PLATFORM_OS_ID = PLATFORM_INFO_OS_ANDROID;
inline constexpr const char* CLM_PLATFORM_OS_NAME = "Android";
#if defined(__ANDROID_API__)
inline constexpr const char* CLM_PLATFORM_OS_VERSION = "API Level " CLM_STR(__ANDROID_API__);
#else
inline constexpr const char* CLM_PLATFORM_OS_VERSION = "Unknown";
#endif
#elif defined(__APPLE__)
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
inline constexpr clm_id CLM_PLATFORM_OS_ID = PLATFORM_INFO_OS_IOS;
inline constexpr const char* CLM_PLATFORM_OS_NAME = "iOS";
#else
inline constexpr clm_id CLM_PLATFORM_OS_ID = PLATFORM_INFO_OS_MACOS;
inline constexpr const char* CLM_PLATFORM_OS_NAME = "macOS";
#endif
inline constexpr const char* CLM_PLATFORM_OS_VERSION = "Unknown";
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
inline constexpr clm_id CLM_PLATFORM_OS_ID = PLATFORM_INFO_OS_BSD;
inline constexpr const char* CLM_PLATFORM_OS_NAME = "BSD";
inline constexpr const char* CLM_PLATFORM_OS_VERSION = "Unknown";
#elif defined(__linux__)
inline constexpr clm_id CLM_PLATFORM_OS_ID = PLATFORM_INFO_OS_LINUX;
inline constexpr const char* CLM_PLATFORM_OS_NAME = "Linux";
inline constexpr const char* CLM_PLATFORM_OS_VERSION = "Unknown";
#else
inline constexpr clm_id CLM_PLATFORM_OS_ID = PLATFORM_INFO_OS_UNKNOWN;
inline constexpr const char* CLM_PLATFORM_OS_NAME = "Unknown";
inline constexpr const char* CLM_PLATFORM_OS_VERSION = "Unknown";
#endif

#if defined(__i386__) || defined(_M_IX86)
inline constexpr clm_id CLM_PLATFORM_ARCH_ID = PLATFORM_INFO_ARCH_X86;
inline constexpr const char* CLM_PLATFORM_ARCH_NAME = "x86";
#elif defined(__x86_64__) || defined(_M_X64)
inline constexpr clm_id CLM_PLATFORM_ARCH_ID = PLATFORM_INFO_ARCH_X64;
inline constexpr const char* CLM_PLATFORM_ARCH_NAME = "x64";
#elif defined(__arm__) || defined(_M_ARM)
inline constexpr clm_id CLM_PLATFORM_ARCH_ID = PLATFORM_INFO_ARCH_ARM32;
inline constexpr const char* CLM_PLATFORM_ARCH_NAME = "ARM32";
#elif defined(__aarch64__) || defined(_M_ARM64)
inline constexpr clm_id CLM_PLATFORM_ARCH_ID = PLATFORM_INFO_ARCH_ARM64;
inline constexpr const char* CLM_PLATFORM_ARCH_NAME = "ARM64";
#elif defined(__riscv) && __riscv_xlen == 32
inline constexpr clm_id CLM_PLATFORM_ARCH_ID = PLATFORM_INFO_ARCH_RISCV32;
inline constexpr const char* CLM_PLATFORM_ARCH_NAME = "RISC-V 32";
#elif defined(__riscv) && __riscv_xlen == 64
inline constexpr clm_id CLM_PLATFORM_ARCH_ID = PLATFORM_INFO_ARCH_RISCV64;
inline constexpr const char* CLM_PLATFORM_ARCH_NAME = "RISC-V 64";
#elif defined(__wasm32__)
inline constexpr clm_id CLM_PLATFORM_ARCH_ID = PLATFORM_INFO_ARCH_WASM32;
inline constexpr const char* CLM_PLATFORM_ARCH_NAME = "WebAssembly 32";
#elif defined(__wasm64__)
inline constexpr clm_id CLM_PLATFORM_ARCH_ID = PLATFORM_INFO_ARCH_WASM64;
inline constexpr const char* CLM_PLATFORM_ARCH_NAME = "WebAssembly 64";
#else
inline constexpr clm_id CLM_PLATFORM_ARCH_ID = PLATFORM_INFO_ARCH_UNKNOWN;
inline constexpr const char* CLM_PLATFORM_ARCH_NAME = "Unknown";
#endif

#if defined(_MSC_VER)
inline constexpr clm_id CLM_PLATFORM_COMPILER_ID = PLATFORM_INFO_COMPILER_MSVC;
inline constexpr const char* CLM_PLATFORM_COMPILER_NAME = "MSVC";
inline constexpr const char* CLM_PLATFORM_COMPILER_VERSION = CLM_STR(_MSC_VER);
#elif defined(__clang__)
inline constexpr clm_id CLM_PLATFORM_COMPILER_ID = PLATFORM_INFO_COMPILER_CLANG;
inline constexpr const char* CLM_PLATFORM_COMPILER_NAME = "Clang";
inline constexpr const char* CLM_PLATFORM_COMPILER_VERSION = CLM_STR(__clang_major__) "." CLM_STR(__clang_minor__) "." CLM_STR(__clang_patchlevel__);
#elif defined(__GNUC__)
inline constexpr clm_id CLM_PLATFORM_COMPILER_ID = PLATFORM_INFO_COMPILER_GCC;
inline constexpr const char* CLM_PLATFORM_COMPILER_NAME = "GCC";
inline constexpr const char* CLM_PLATFORM_COMPILER_VERSION = CLM_STR(__GNUC__) "." CLM_STR(__GNUC_MINOR__) "." CLM_STR(__GNUC_PATCHLEVEL__);
#else
inline constexpr clm_id CLM_PLATFORM_COMPILER_ID = PLATFORM_INFO_COMPILER_UNKNOWN;
inline constexpr const char* CLM_PLATFORM_COMPILER_NAME = "Unknown";
inline constexpr const char* CLM_PLATFORM_COMPILER_VERSION = "Unknown";
#endif

#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
inline constexpr clm_id CLM_PLATFORM_ENDIAN_ID = PLATFORM_INFO_ENDIAN_BIG;
#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
inline constexpr clm_id CLM_PLATFORM_ENDIAN_ID = PLATFORM_INFO_ENDIAN_LITTLE;
#elif defined(_WIN32)
inline constexpr clm_id CLM_PLATFORM_ENDIAN_ID = PLATFORM_INFO_ENDIAN_LITTLE;
#else
inline constexpr clm_id CLM_PLATFORM_ENDIAN_ID = PLATFORM_INFO_ENDIAN_UNKNOWN;
#endif

#if defined(_WIN32)
inline constexpr char CLM_PLATFORM_PATH_SEPARATOR = '\\';
inline constexpr const char* CLM_PLATFORM_NEWLINE = "\r\n";
#else
inline constexpr char CLM_PLATFORM_PATH_SEPARATOR = '/';
inline constexpr const char* CLM_PLATFORM_NEWLINE = "\n";
#endif

static inline clm_id ensure_string(const clm_id id, const char* value)
{
    if (clm::string::value(id) == nullptr)
        clm::string::create(id, value);
    return id;
}

} // namespace clm::platform

static inline int clm_platform_get_os(void) { return (int)clm::platform::CLM_PLATFORM_OS_ID; }
static inline int clm_platform_get_os_name(void) { return (int)clm::platform::ensure_string(clm::platform::CLM_PLATFORM_OS_ID, clm::platform::CLM_PLATFORM_OS_NAME); }
static inline int clm_platform_get_os_version(void) { return (int)clm::platform::ensure_string(clm::platform::PLATFORM_BLOCK_OS_VERSION_ID, clm::platform::CLM_PLATFORM_OS_VERSION); }
static inline int clm_platform_get_arch(void) { return (int)clm::platform::CLM_PLATFORM_ARCH_ID; }
static inline int clm_platform_get_arch_name(void) { return (int)clm::platform::ensure_string(clm::platform::CLM_PLATFORM_ARCH_ID, clm::platform::CLM_PLATFORM_ARCH_NAME); }
static inline int clm_platform_get_bits(void) { return (int)(sizeof(void*) * 8u); }
static inline int clm_platform_get_endian(void) { return (int)clm::platform::CLM_PLATFORM_ENDIAN_ID; }
static inline int clm_platform_get_compiler(void) { return (int)clm::platform::CLM_PLATFORM_COMPILER_ID; }
static inline int clm_platform_get_compiler_name(void) { return (int)clm::platform::ensure_string(clm::platform::CLM_PLATFORM_COMPILER_ID, clm::platform::CLM_PLATFORM_COMPILER_NAME); }
static inline int clm_platform_get_compiler_version(void) { return (int)clm::platform::ensure_string(clm::platform::PLATFORM_BLOCK_COMPILER_VERSION_ID, clm::platform::CLM_PLATFORM_COMPILER_VERSION); }
static inline char clm_platform_get_path_separator(void) { return clm::platform::CLM_PLATFORM_PATH_SEPARATOR; }
static inline int clm_platform_get_newline(void) { return (int)clm::platform::ensure_string(clm::platform::PLATFORM_BLOCK_NEWLINE_ID, clm::platform::CLM_PLATFORM_NEWLINE); }
static inline int clm_platform_is_windows(void) { return clm_platform_get_os() == (int)clm::platform::PLATFORM_INFO_OS_WINDOWS; }
static inline int clm_platform_is_linux(void) { return clm_platform_get_os() == (int)clm::platform::PLATFORM_INFO_OS_LINUX; }
static inline int clm_platform_is_bsd(void) { return clm_platform_get_os() == (int)clm::platform::PLATFORM_INFO_OS_BSD; }
static inline int clm_platform_is_macos(void) { return clm_platform_get_os() == (int)clm::platform::PLATFORM_INFO_OS_MACOS; }
static inline int clm_platform_is_android(void) { return clm_platform_get_os() == (int)clm::platform::PLATFORM_INFO_OS_ANDROID; }
static inline int clm_platform_is_ios(void) { return clm_platform_get_os() == (int)clm::platform::PLATFORM_INFO_OS_IOS; }
static inline int clm_platform_is_mobile(void) { return clm_platform_is_android() || clm_platform_is_ios(); }
static inline int clm_platform_is_desktop(void) { return clm_platform_is_windows() || clm_platform_is_linux() || clm_platform_is_bsd() || clm_platform_is_macos(); }
