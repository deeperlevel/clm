"""CLM platform IDs. Python reports interpreter target, not a C compilation target."""

import os, sys, platform as _p, struct

PLATFORM_INFO_OS_WINDOWS = 252706864
PLATFORM_INFO_OS_MACOS = 252706865
PLATFORM_INFO_OS_LINUX = 252706866
PLATFORM_INFO_OS_BSD = 252706867
PLATFORM_INFO_OS_ANDROID = 252706868
PLATFORM_INFO_OS_IOS = 252706869
PLATFORM_INFO_OS_UNKNOWN = 252706879
PLATFORM_INFO_ARCH_X86 = 252706880
PLATFORM_INFO_ARCH_X64 = 252706881
PLATFORM_INFO_ARCH_ARM32 = 252706882
PLATFORM_INFO_ARCH_ARM64 = 252706883
PLATFORM_INFO_ARCH_RISCV32 = 252706884
PLATFORM_INFO_ARCH_RISCV64 = 252706885
PLATFORM_INFO_ARCH_WASM32 = 252706886
PLATFORM_INFO_ARCH_WASM64 = 252706887
PLATFORM_INFO_ARCH_UNKNOWN = 252706895
PLATFORM_INFO_COMPILER_MSVC = 252706896
PLATFORM_INFO_COMPILER_GCC = 252706897
PLATFORM_INFO_COMPILER_CLANG = 252706898
PLATFORM_INFO_COMPILER_UNKNOWN = 252706911
PLATFORM_INFO_ENDIAN_LITTLE = 252706912
PLATFORM_INFO_ENDIAN_BIG = 252706913
PLATFORM_INFO_ENDIAN_ENDIAN_UNKNOWN = 252706927

def clm_platform_get_os():
    s=sys.platform
    return (PLATFORM_INFO_OS_ANDROID if 'ANDROID_ROOT' in os.environ and s.startswith('linux') else
            PLATFORM_INFO_OS_IOS if s.startswith('ios') else
            PLATFORM_INFO_OS_WINDOWS if s=='win32' else
            PLATFORM_INFO_OS_MACOS if s=='darwin' else
            PLATFORM_INFO_OS_BSD if 'bsd' in s else
            PLATFORM_INFO_OS_LINUX if s.startswith('linux') else PLATFORM_INFO_OS_UNKNOWN)

def clm_platform_get_os_name():
    return {PLATFORM_INFO_OS_WINDOWS:'Windows', PLATFORM_INFO_OS_MACOS:'macOS', PLATFORM_INFO_OS_LINUX:'Linux', PLATFORM_INFO_OS_BSD:'BSD', PLATFORM_INFO_OS_ANDROID:'Android', PLATFORM_INFO_OS_IOS:'iOS'}.get(clm_platform_get_os(),'Unknown')

def clm_platform_get_os_version(): 
    return 'Unknown (runtime interpreter; no compile-time OS target hint)'

def clm_platform_get_arch_name():
    m=_p.machine().lower()
    return {'amd64':'x64','x86_64':'x64','i386':'x86','i686':'x86','aarch64':'ARM64','arm64':'ARM64','armv7l':'ARM32','riscv64':'RISC-V 64','riscv32':'RISC-V 32','wasm32':'WebAssembly 32','wasm64':'WebAssembly 64'}.get(m,'Unknown')

def clm_platform_get_arch():
    return {'x64':PLATFORM_INFO_ARCH_X64,'x86':PLATFORM_INFO_ARCH_X86,'ARM64':PLATFORM_INFO_ARCH_ARM64,'ARM32':PLATFORM_INFO_ARCH_ARM32,'RISC-V 64':PLATFORM_INFO_ARCH_RISCV64,'RISC-V 32':PLATFORM_INFO_ARCH_RISCV32,'WebAssembly 32':PLATFORM_INFO_ARCH_WASM32,'WebAssembly 64':PLATFORM_INFO_ARCH_WASM64}.get(clm_platform_get_arch_name(),PLATFORM_INFO_ARCH_UNKNOWN)

def clm_platform_get_bits(): 
    return struct.calcsize('P')*8

def clm_platform_get_endian(): 
    return PLATFORM_INFO_ENDIAN_LITTLE if sys.byteorder=='little' else PLATFORM_INFO_ENDIAN_BIG

def clm_platform_get_compiler_name(): 
    return 'Unknown (Python runtime)'

def clm_platform_get_compiler(): 
    return PLATFORM_INFO_COMPILER_UNKNOWN

def clm_platform_get_compiler_version(): 
    return _p.python_compiler()

def clm_platform_get_path_separator(): 
    return '\\' if clm_platform_is_windows() else '/'

def clm_platform_get_newline(): 
    return '\r\n' if clm_platform_is_windows() else '\n'

def clm_platform_is_windows(): 
    return clm_platform_get_os() == PLATFORM_INFO_OS_WINDOWS

def clm_platform_is_linux(): 
    return clm_platform_get_os() == PLATFORM_INFO_OS_LINUX

def clm_platform_is_bsd(): 
    return clm_platform_get_os() == PLATFORM_INFO_OS_BSD

def clm_platform_is_macos():
    return clm_platform_get_os() == PLATFORM_INFO_OS_MACOS

def clm_platform_is_android(): return clm_platform_get_os() == PLATFORM_INFO_OS_ANDROID

def clm_platform_is_ios(): 
    return clm_platform_get_os() == PLATFORM_INFO_OS_IOS

def clm_platform_is_mobile():
    return clm_platform_is_android() or clm_platform_is_ios()

def clm_platform_is_desktop(): 
    return any((clm_platform_is_windows(), clm_platform_is_linux(), clm_platform_is_bsd(), clm_platform_is_macos()))
