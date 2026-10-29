#pragma once
#include <cstdint>
#include <climits>
#include <string_view>
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
namespace clm::platform {
inline constexpr int PLATFORM_INFO_OS_WINDOWS = 252706864;
inline constexpr int PLATFORM_INFO_OS_MACOS = 252706865;
inline constexpr int PLATFORM_INFO_OS_LINUX = 252706866;
inline constexpr int PLATFORM_INFO_OS_BSD = 252706867;
inline constexpr int PLATFORM_INFO_OS_ANDROID = 252706868;
inline constexpr int PLATFORM_INFO_OS_IOS = 252706869;
inline constexpr int PLATFORM_INFO_OS_UNKNOWN = 252706879;
inline constexpr int PLATFORM_INFO_ARCH_X86 = 252706880;
inline constexpr int PLATFORM_INFO_ARCH_X64 = 252706881;
inline constexpr int PLATFORM_INFO_ARCH_ARM32 = 252706882;
inline constexpr int PLATFORM_INFO_ARCH_ARM64 = 252706883;
inline constexpr int PLATFORM_INFO_ARCH_RISCV32 = 252706884;
inline constexpr int PLATFORM_INFO_ARCH_RISCV64 = 252706885;
inline constexpr int PLATFORM_INFO_ARCH_WASM32 = 252706886;
inline constexpr int PLATFORM_INFO_ARCH_WASM64 = 252706887;
inline constexpr int PLATFORM_INFO_ARCH_UNKNOWN = 252706895;
inline constexpr int PLATFORM_INFO_COMPILER_MSVC = 252706896;
inline constexpr int PLATFORM_INFO_COMPILER_GCC = 252706897;
inline constexpr int PLATFORM_INFO_COMPILER_CLANG = 252706898;
inline constexpr int PLATFORM_INFO_COMPILER_UNKNOWN = 252706911;
inline constexpr int PLATFORM_INFO_ENDIAN_LITTLE = 252706912;
inline constexpr int PLATFORM_INFO_ENDIAN_BIG = 252706913;
inline constexpr int PLATFORM_INFO_ENDIAN_ENDIAN_UNKNOWN = 252706927;
#if defined(_WIN32)
inline constexpr int os=PLATFORM_INFO_OS_WINDOWS;inline constexpr auto os_name="Windows";
#elif defined(__ANDROID__)
inline constexpr int os=PLATFORM_INFO_OS_ANDROID;inline constexpr auto os_name="Android";
#elif defined(__APPLE__)
#if defined(TARGET_OS_TV) && TARGET_OS_TV || defined(TARGET_OS_WATCH) && TARGET_OS_WATCH || defined(TARGET_OS_VISION) && TARGET_OS_VISION
inline constexpr int os=PLATFORM_INFO_OS_UNKNOWN;inline constexpr auto os_name="Unknown Apple OS";
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
inline constexpr int os=PLATFORM_INFO_OS_IOS;inline constexpr auto os_name="iOS";
#else
inline constexpr int os=PLATFORM_INFO_OS_MACOS;inline constexpr auto os_name="macOS";
#endif
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
inline constexpr int os=PLATFORM_INFO_OS_BSD;inline constexpr auto os_name="BSD";
#elif defined(__linux__)
inline constexpr int os=PLATFORM_INFO_OS_LINUX;inline constexpr auto os_name="Linux";
#else
inline constexpr int os=PLATFORM_INFO_OS_UNKNOWN;inline constexpr auto os_name="Unknown";
#endif
#if defined(__wasm64__)
inline constexpr int arch=PLATFORM_INFO_ARCH_WASM64;inline constexpr auto arch_name="WebAssembly 64";
#elif defined(__wasm32__)
inline constexpr int arch=PLATFORM_INFO_ARCH_WASM32;inline constexpr auto arch_name="WebAssembly 32";
#elif defined(__x86_64__) || defined(_M_X64)
inline constexpr int arch=PLATFORM_INFO_ARCH_X64;inline constexpr auto arch_name="x64";
#elif defined(__i386__) || defined(_M_IX86)
inline constexpr int arch=PLATFORM_INFO_ARCH_X86;inline constexpr auto arch_name="x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
inline constexpr int arch=PLATFORM_INFO_ARCH_ARM64;inline constexpr auto arch_name="ARM64";
#elif defined(__arm__) || defined(_M_ARM)
inline constexpr int arch=PLATFORM_INFO_ARCH_ARM32;inline constexpr auto arch_name="ARM32";
#elif defined(__riscv) && __riscv_xlen==64
inline constexpr int arch=PLATFORM_INFO_ARCH_RISCV64;inline constexpr auto arch_name="RISC-V 64";
#elif defined(__riscv) && __riscv_xlen==32
inline constexpr int arch=PLATFORM_INFO_ARCH_RISCV32;inline constexpr auto arch_name="RISC-V 32";
#else
inline constexpr int arch=PLATFORM_INFO_ARCH_UNKNOWN;inline constexpr auto arch_name="Unknown";
#endif
#define CLM_STRING_INNER(x) #x
#define CLM_STRING(x) CLM_STRING_INNER(x)
#if defined(__clang__)
inline constexpr int compiler=PLATFORM_INFO_COMPILER_CLANG;inline constexpr auto compiler_name="Clang";inline constexpr auto compiler_version=CLM_STRING(__clang_major__) "." CLM_STRING(__clang_minor__) "." CLM_STRING(__clang_patchlevel__);
#elif defined(_MSC_VER)
inline constexpr int compiler=PLATFORM_INFO_COMPILER_MSVC;inline constexpr auto compiler_name="MSVC";inline constexpr auto compiler_version=CLM_STRING(_MSC_VER);
#elif defined(__GNUC__)
inline constexpr int compiler=PLATFORM_INFO_COMPILER_GCC;inline constexpr auto compiler_name="GCC";inline constexpr auto compiler_version=CLM_STRING(__GNUC__) "." CLM_STRING(__GNUC_MINOR__) "." CLM_STRING(__GNUC_PATCHLEVEL__);
#else
inline constexpr int compiler=PLATFORM_INFO_COMPILER_UNKNOWN;inline constexpr auto compiler_name="Unknown";inline constexpr auto compiler_version="Unknown";
#endif
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__==__ORDER_BIG_ENDIAN__
inline constexpr int endian=PLATFORM_INFO_ENDIAN_BIG;
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__ || defined(_WIN32)
inline constexpr int endian=PLATFORM_INFO_ENDIAN_LITTLE;
#else
inline constexpr int endian=PLATFORM_INFO_ENDIAN_UNKNOWN;
#endif
inline constexpr int clm_platform_get_os(){return os;}
inline constexpr auto clm_platform_get_os_name(){return os_name;}
inline constexpr auto clm_platform_get_os_version(){return "Unknown (compile-time target hint unavailable)";}
inline constexpr int clm_platform_get_arch(){return arch;}
inline constexpr auto clm_platform_get_arch_name(){return arch_name;}
inline constexpr int clm_platform_get_bits(){return int(sizeof(void*)*CHAR_BIT);}
inline constexpr int clm_platform_get_endian(){return endian;}
inline constexpr int clm_platform_get_compiler(){return compiler;}
inline constexpr auto clm_platform_get_compiler_name(){return compiler_name;}
inline constexpr auto clm_platform_get_compiler_version(){return compiler_version;}
inline constexpr char clm_platform_get_path_separator(){return os==PLATFORM_INFO_OS_WINDOWS?'\\':'/';}
inline constexpr auto clm_platform_get_newline(){return os==PLATFORM_INFO_OS_WINDOWS?"\r\n":"\n";}
inline constexpr bool clm_platform_is_windows(){return os==PLATFORM_INFO_OS_WINDOWS;}
inline constexpr bool clm_platform_is_linux(){return os==PLATFORM_INFO_OS_LINUX;}
inline constexpr bool clm_platform_is_bsd(){return os==PLATFORM_INFO_OS_BSD;}
inline constexpr bool clm_platform_is_macos(){return os==PLATFORM_INFO_OS_MACOS;}
inline constexpr bool clm_platform_is_android(){return os==PLATFORM_INFO_OS_ANDROID;}
inline constexpr bool clm_platform_is_ios(){return os==PLATFORM_INFO_OS_IOS;}
inline constexpr bool clm_platform_is_mobile(){return clm_platform_is_android()||clm_platform_is_ios();}
inline constexpr bool clm_platform_is_desktop(){return clm_platform_is_windows()||clm_platform_is_linux()||clm_platform_is_bsd()||clm_platform_is_macos();}
inline int copy_to(std::string_view text,char* dst,std::size_t capacity){if(!dst||capacity<text.size()+1)return -1;for(std::size_t i=0;i<text.size();++i)dst[i]=text[i];dst[text.size()]=0;return int(text.size());}
}
