#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#include "bpa.h"
#include "string.h"  /* CLM string API, not the C standard library header. */

/* All platform information describes the compiled target, not the host.
 * OS version is only a compile-time SDK/header/target hint. */
#define CLM_STR_INNER(x) #x
#define CLM_STR(x) CLM_STR_INNER(x)

/* Fixed BPA blocks for properties without a dedicated identity. */
#define PLATFORM_BLOCK_OS_VERSION_ID       ((clm_id)0x0f100021u)
#define PLATFORM_BLOCK_COMPILER_VERSION_ID ((clm_id)0x0f100022u)
#define PLATFORM_BLOCK_NEWLINE_ID          ((clm_id)0x0f100023u)

/* OS IDs double as BPA IDs for their names. */
#define PLATFORM_INFO_OS_WINDOWS ((clm_id)0x0f100030u)
#define PLATFORM_INFO_OS_MACOS   ((clm_id)0x0f100031u)
#define PLATFORM_INFO_OS_LINUX   ((clm_id)0x0f100032u)
#define PLATFORM_INFO_OS_BSD     ((clm_id)0x0f100033u)
#define PLATFORM_INFO_OS_ANDROID ((clm_id)0x0f100034u)
#define PLATFORM_INFO_OS_IOS     ((clm_id)0x0f100035u)
#define PLATFORM_INFO_OS_UNKNOWN ((clm_id)0x0f10003fu)

/* Architecture IDs double as BPA IDs for their names. */
#define PLATFORM_INFO_ARCH_X86     ((clm_id)0x0f100040u)
#define PLATFORM_INFO_ARCH_X64     ((clm_id)0x0f100041u)
#define PLATFORM_INFO_ARCH_ARM32   ((clm_id)0x0f100042u)
#define PLATFORM_INFO_ARCH_ARM64   ((clm_id)0x0f100043u)
#define PLATFORM_INFO_ARCH_RISCV32 ((clm_id)0x0f100044u)
#define PLATFORM_INFO_ARCH_RISCV64 ((clm_id)0x0f100045u)
#define PLATFORM_INFO_ARCH_WASM32  ((clm_id)0x0f100046u)
#define PLATFORM_INFO_ARCH_WASM64  ((clm_id)0x0f100047u)
#define PLATFORM_INFO_ARCH_UNKNOWN ((clm_id)0x0f10004fu)

/* Compiler IDs double as BPA IDs for their names. */
#define PLATFORM_INFO_COMPILER_MSVC    ((clm_id)0x0f100050u)
#define PLATFORM_INFO_COMPILER_GCC     ((clm_id)0x0f100051u)
#define PLATFORM_INFO_COMPILER_CLANG   ((clm_id)0x0f100052u)
#define PLATFORM_INFO_COMPILER_UNKNOWN ((clm_id)0x0f10005fu)

#define PLATFORM_INFO_ENDIAN_LITTLE  ((clm_id)0x0f100060u)
#define PLATFORM_INFO_ENDIAN_BIG     ((clm_id)0x0f100061u)
#define PLATFORM_INFO_ENDIAN_UNKNOWN ((clm_id)0x0f10006fu)

/* OS: test Android before Linux, and Apple device targets before macOS. */
#if defined(_WIN32)
    #include <sdkddkver.h>
    #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_WINDOWS
    #define CLM_PLATFORM_OS_NAME "Windows"
    #if defined(_WIN32_WINNT)
        #define CLM_PLATFORM_OS_VERSION "Target " CLM_STR(_WIN32_WINNT)
    #else
        #define CLM_PLATFORM_OS_VERSION "Unknown"
    #endif
#elif defined(__ANDROID__)
    #include <android/api-level.h>
    #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_ANDROID
    #define CLM_PLATFORM_OS_NAME "Android"
    #if defined(__ANDROID_API__)
        #define CLM_PLATFORM_OS_VERSION "API Level " CLM_STR(__ANDROID_API__)
    #else
        #define CLM_PLATFORM_OS_VERSION "Unknown"
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #include <AvailabilityMacros.h>
    #if defined(TARGET_OS_TV) && TARGET_OS_TV
        #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_UNKNOWN
        #define CLM_PLATFORM_OS_NAME "tvOS (unsupported ID)"
        #define CLM_PLATFORM_OS_VERSION "Unknown"
    #elif defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
        #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_UNKNOWN
        #define CLM_PLATFORM_OS_NAME "watchOS (unsupported ID)"
        #define CLM_PLATFORM_OS_VERSION "Unknown"
    #elif defined(TARGET_OS_VISION) && TARGET_OS_VISION
        #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_UNKNOWN
        #define CLM_PLATFORM_OS_NAME "visionOS (unsupported ID)"
        #define CLM_PLATFORM_OS_VERSION "Unknown"
    #elif (defined(TARGET_OS_IOS) && TARGET_OS_IOS) || \
          (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
        #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_IOS
        #define CLM_PLATFORM_OS_NAME "iOS"
        #if defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
            #define CLM_PLATFORM_OS_VERSION "Target " CLM_STR(__IPHONE_OS_VERSION_MIN_REQUIRED)
        #else
            #define CLM_PLATFORM_OS_VERSION "Unknown"
        #endif
    #elif defined(TARGET_OS_OSX) && TARGET_OS_OSX
        #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_MACOS
        #define CLM_PLATFORM_OS_NAME "macOS"
        #if defined(MAC_OS_X_VERSION_MIN_REQUIRED)
            #define CLM_PLATFORM_OS_VERSION "Target " CLM_STR(MAC_OS_X_VERSION_MIN_REQUIRED)
        #else
            #define CLM_PLATFORM_OS_VERSION "Unknown"
        #endif
    #else
        #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_UNKNOWN
        #define CLM_PLATFORM_OS_NAME "Unknown Apple OS"
        #define CLM_PLATFORM_OS_VERSION "Unknown"
    #endif
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    #include <sys/param.h>
    #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_BSD
    #define CLM_PLATFORM_OS_NAME "BSD"
    #if defined(__FreeBSD_version)
        #define CLM_PLATFORM_OS_VERSION "FreeBSD Header " CLM_STR(__FreeBSD_version)
    #elif defined(__NetBSD_Version__)
        #define CLM_PLATFORM_OS_VERSION "NetBSD Header " CLM_STR(__NetBSD_Version__)
    #else
        #define CLM_PLATFORM_OS_VERSION "Unknown"
    #endif
#elif defined(__linux__)
    #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_LINUX
    #define CLM_PLATFORM_OS_NAME "Linux"
    #if defined(__has_include)
        #if __has_include(<linux/version.h>)
            #include <linux/version.h>
        #endif
    #endif
    #if defined(LINUX_VERSION_CODE)
        #define CLM_PLATFORM_OS_VERSION "Kernel Header Code " CLM_STR(LINUX_VERSION_CODE)
    #else
        #define CLM_PLATFORM_OS_VERSION "Unknown"
    #endif
#else
    #define CLM_PLATFORM_OS_ID PLATFORM_INFO_OS_UNKNOWN
    #define CLM_PLATFORM_OS_NAME "Unknown"
    #define CLM_PLATFORM_OS_VERSION "Unknown"
#endif

/* Architecture: WebAssembly must precede architecture compatibility macros. */
#if defined(__wasm64__)
    #define CLM_PLATFORM_ARCH_ID PLATFORM_INFO_ARCH_WASM64
    #define CLM_PLATFORM_ARCH_NAME "WebAssembly 64"
#elif defined(__wasm32__)
    #define CLM_PLATFORM_ARCH_ID PLATFORM_INFO_ARCH_WASM32
    #define CLM_PLATFORM_ARCH_NAME "WebAssembly 32"
#elif defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
    #define CLM_PLATFORM_ARCH_ID PLATFORM_INFO_ARCH_X64
    #define CLM_PLATFORM_ARCH_NAME "x64"
#elif defined(__i386__) || defined(_M_IX86)
    #define CLM_PLATFORM_ARCH_ID PLATFORM_INFO_ARCH_X86
    #define CLM_PLATFORM_ARCH_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
    #define CLM_PLATFORM_ARCH_ID PLATFORM_INFO_ARCH_ARM64
    #define CLM_PLATFORM_ARCH_NAME "ARM64"
#elif defined(__arm__) || defined(_M_ARM)
    #define CLM_PLATFORM_ARCH_ID PLATFORM_INFO_ARCH_ARM32
    #define CLM_PLATFORM_ARCH_NAME "ARM32"
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
    #define CLM_PLATFORM_ARCH_ID PLATFORM_INFO_ARCH_RISCV64
    #define CLM_PLATFORM_ARCH_NAME "RISC-V 64"
#elif defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 32)
    #define CLM_PLATFORM_ARCH_ID PLATFORM_INFO_ARCH_RISCV32
    #define CLM_PLATFORM_ARCH_NAME "RISC-V 32"
#else
    #define CLM_PLATFORM_ARCH_ID PLATFORM_INFO_ARCH_UNKNOWN
    #define CLM_PLATFORM_ARCH_NAME "Unknown"
#endif

/* Compiler: Clang is tested first (clang-cl also defines _MSC_VER). */
#if defined(__clang__)
    #define CLM_PLATFORM_COMPILER_ID PLATFORM_INFO_COMPILER_CLANG
    #define CLM_PLATFORM_COMPILER_NAME "Clang"
    #define CLM_PLATFORM_COMPILER_VERSION CLM_STR(__clang_major__) "." CLM_STR(__clang_minor__) "." CLM_STR(__clang_patchlevel__)
#elif defined(_MSC_VER)
    #define CLM_PLATFORM_COMPILER_ID PLATFORM_INFO_COMPILER_MSVC
    #define CLM_PLATFORM_COMPILER_NAME "MSVC"
    #define CLM_PLATFORM_COMPILER_VERSION CLM_STR(_MSC_VER)
#elif defined(__GNUC__)
    #define CLM_PLATFORM_COMPILER_ID PLATFORM_INFO_COMPILER_GCC
    #define CLM_PLATFORM_COMPILER_NAME "GCC"
    #define CLM_PLATFORM_COMPILER_VERSION CLM_STR(__GNUC__) "." CLM_STR(__GNUC_MINOR__) "." CLM_STR(__GNUC_PATCHLEVEL__)
#else
    #define CLM_PLATFORM_COMPILER_ID PLATFORM_INFO_COMPILER_UNKNOWN
    #define CLM_PLATFORM_COMPILER_NAME "Unknown"
    #define CLM_PLATFORM_COMPILER_VERSION "Unknown"
#endif

/* Endianness describes the compiled target. */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
    #define CLM_PLATFORM_ENDIAN_ID PLATFORM_INFO_ENDIAN_LITTLE
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
    #define CLM_PLATFORM_ENDIAN_ID PLATFORM_INFO_ENDIAN_BIG
#elif defined(_WIN32)
    /* All Windows targets supported above use little-endian byte ordering. */
    #define CLM_PLATFORM_ENDIAN_ID PLATFORM_INFO_ENDIAN_LITTLE
#else
    #define CLM_PLATFORM_ENDIAN_ID PLATFORM_INFO_ENDIAN_UNKNOWN
#endif

/* Platform-conventional separators. Neither controls CLM write_line APIs. */
#if defined(_WIN32)
    #define CLM_PLATFORM_PATH_SEPARATOR '\\'
    #define CLM_PLATFORM_NEWLINE "\r\n"
#else
    #define CLM_PLATFORM_PATH_SEPARATOR '/'
    #define CLM_PLATFORM_NEWLINE "\n"
#endif

/* BPA name/property initialization. BPA should ensure safe concurrent creation. */
static clm_id clm_platform_ensure_string(clm_id id, const char *value)
{
    if (!clm_bpa_is_existing(id))
    {
        clm_string_new(id, value);
    }
    return id;
}

/* Identity and descriptive properties. IDs are returned as int for compatibility. */
int clm_platform_get_os(void) { return (int)CLM_PLATFORM_OS_ID; }
int clm_platform_get_os_name(void)
{
    return (int)clm_platform_ensure_string(CLM_PLATFORM_OS_ID, CLM_PLATFORM_OS_NAME);
}
int clm_platform_get_os_name_to(char *dest, int dest_size)
{
    clm_platform_get_os_name();
    return clm_bpa_get_string_to(CLM_PLATFORM_OS_ID, &dest, dest_size);
}
int clm_platform_get_os_version(void)
{
    return (int)clm_platform_ensure_string(PLATFORM_BLOCK_OS_VERSION_ID, CLM_PLATFORM_OS_VERSION);
}
int clm_platform_get_os_version_to(char *dest, int dest_size)
{
    clm_platform_get_os_version();
    return clm_bpa_get_string_to(PLATFORM_BLOCK_OS_VERSION_ID, &dest, dest_size);
}

int clm_platform_get_arch(void) { return (int)CLM_PLATFORM_ARCH_ID; }
int clm_platform_get_arch_name(void)
{
    return (int)clm_platform_ensure_string(CLM_PLATFORM_ARCH_ID, CLM_PLATFORM_ARCH_NAME);
}
int clm_platform_get_arch_name_to(char *dest, int dest_size)
{
    clm_platform_get_arch_name();
    return clm_bpa_get_string_to(CLM_PLATFORM_ARCH_ID, &dest, dest_size);
}
int clm_platform_get_bits(void) { return (int)(sizeof(void *) * 8u); }
int clm_platform_get_endian(void) { return (int)CLM_PLATFORM_ENDIAN_ID; }

int clm_platform_get_compiler(void) { return (int)CLM_PLATFORM_COMPILER_ID; }
int clm_platform_get_compiler_name(void)
{
    return (int)clm_platform_ensure_string(CLM_PLATFORM_COMPILER_ID, CLM_PLATFORM_COMPILER_NAME);
}
int clm_platform_get_compiler_name_to(char *dest, int dest_size)
{
    clm_platform_get_compiler_name();
    return clm_bpa_get_string_to(CLM_PLATFORM_COMPILER_ID, &dest, dest_size);
}
int clm_platform_get_compiler_version(void)
{
    return (int)clm_platform_ensure_string(PLATFORM_BLOCK_COMPILER_VERSION_ID, CLM_PLATFORM_COMPILER_VERSION);
}
int clm_platform_get_compiler_version_to(char *dest, int dest_size)
{
    clm_platform_get_compiler_version();
    return clm_bpa_get_string_to(PLATFORM_BLOCK_COMPILER_VERSION_ID, &dest, dest_size);
}

char clm_platform_get_path_separator(void) { return CLM_PLATFORM_PATH_SEPARATOR; }
int clm_platform_get_newline(void)
{
    return (int)clm_platform_ensure_string(PLATFORM_BLOCK_NEWLINE_ID, CLM_PLATFORM_NEWLINE);
}
int clm_platform_get_newline_to(char *dest, int dest_size)
{
    clm_platform_get_newline();
    return clm_bpa_get_string_to(PLATFORM_BLOCK_NEWLINE_ID, &dest, dest_size);
}

int clm_platform_is_windows(void) { return clm_platform_get_os() == (int)PLATFORM_INFO_OS_WINDOWS; }
int clm_platform_is_linux(void)   { return clm_platform_get_os() == (int)PLATFORM_INFO_OS_LINUX; }
int clm_platform_is_bsd(void)     { return clm_platform_get_os() == (int)PLATFORM_INFO_OS_BSD; }
int clm_platform_is_macos(void)   { return clm_platform_get_os() == (int)PLATFORM_INFO_OS_MACOS; }
int clm_platform_is_android(void) { return clm_platform_get_os() == (int)PLATFORM_INFO_OS_ANDROID; }
int clm_platform_is_ios(void)     { return clm_platform_get_os() == (int)PLATFORM_INFO_OS_IOS; }

int clm_platform_is_mobile(void)
{
    return clm_platform_is_android() || clm_platform_is_ios();
}

int clm_platform_is_desktop(void)
{
    return clm_platform_is_windows() || clm_platform_is_linux() ||
           clm_platform_is_bsd() || clm_platform_is_macos();
}
