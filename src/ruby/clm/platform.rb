# CLM platform; interpreter host detection, not compile target.
require 'rbconfig'
module CLMPlatform
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
  module_function
  def clm_platform_get_os
    s=RbConfig::CONFIG['host_os'].downcase
    return PLATFORM_INFO_OS_ANDROID if s.include?('android')
    return PLATFORM_INFO_OS_IOS if s.include?('ios')
    return PLATFORM_INFO_OS_WINDOWS if s.match?(/mswin|mingw|cygwin/)
    return PLATFORM_INFO_OS_MACOS if s.include?('darwin')
    return PLATFORM_INFO_OS_BSD if s.include?('bsd')
    return PLATFORM_INFO_OS_LINUX if s.include?('linux')
    PLATFORM_INFO_OS_UNKNOWN
  end
  def clm_platform_get_os_name
    {PLATFORM_INFO_OS_WINDOWS=>'Windows',PLATFORM_INFO_OS_MACOS=>'macOS',PLATFORM_INFO_OS_LINUX=>'Linux',PLATFORM_INFO_OS_BSD=>'BSD',PLATFORM_INFO_OS_ANDROID=>'Android',PLATFORM_INFO_OS_IOS=>'iOS'}.fetch(clm_platform_get_os,'Unknown')
  end
  def clm_platform_get_os_version; 'Unknown (interpreter host)';end
  def clm_platform_get_arch_name
    s=RbConfig::CONFIG['host_cpu'].downcase
    {'x86_64'=>'x64','amd64'=>'x64','i386'=>'x86','i686'=>'x86','aarch64'=>'ARM64','arm64'=>'ARM64','riscv64'=>'RISC-V 64','wasm32'=>'WebAssembly 32','wasm64'=>'WebAssembly 64'}.fetch(s,'Unknown')
  end
  def clm_platform_get_arch
    {'x64'=>PLATFORM_INFO_ARCH_X64,'x86'=>PLATFORM_INFO_ARCH_X86,'ARM64'=>PLATFORM_INFO_ARCH_ARM64,'RISC-V 64'=>PLATFORM_INFO_ARCH_RISCV64,'WebAssembly 32'=>PLATFORM_INFO_ARCH_WASM32,'WebAssembly 64'=>PLATFORM_INFO_ARCH_WASM64}.fetch(clm_platform_get_arch_name,PLATFORM_INFO_ARCH_UNKNOWN)
  end
  def clm_platform_get_bits; (RbConfig::CONFIG['SIZEOF_VOIDP'] || (RbConfig::CONFIG['host_cpu'].include?('64') ? '8':'4')).to_i*8;end
  def clm_platform_get_endian; [1].pack('S')=="\x01\x00".b ? PLATFORM_INFO_ENDIAN_LITTLE : PLATFORM_INFO_ENDIAN_BIG;end
  def clm_platform_get_compiler;PLATFORM_INFO_COMPILER_UNKNOWN;end
  def clm_platform_get_compiler_name;'Unknown (Ruby runtime)';end
  def clm_platform_get_compiler_version;RUBY_DESCRIPTION;end
  def clm_platform_get_path_separator;clm_platform_is_windows ? '\\' : '/';end
  def clm_platform_get_newline;clm_platform_is_windows ? "\r\n" : "\n";end
  def clm_platform_is_windows;clm_platform_get_os == PLATFORM_INFO_OS_WINDOWS;end
  def clm_platform_is_linux;clm_platform_get_os == PLATFORM_INFO_OS_LINUX;end
  def clm_platform_is_bsd;clm_platform_get_os == PLATFORM_INFO_OS_BSD;end
  def clm_platform_is_macos;clm_platform_get_os == PLATFORM_INFO_OS_MACOS;end
  def clm_platform_is_android;clm_platform_get_os == PLATFORM_INFO_OS_ANDROID;end
  def clm_platform_is_ios;clm_platform_get_os == PLATFORM_INFO_OS_IOS;end
  def clm_platform_is_mobile;clm_platform_is_android || clm_platform_is_ios;end
  def clm_platform_is_desktop;[PLATFORM_INFO_OS_WINDOWS,PLATFORM_INFO_OS_LINUX,PLATFORM_INFO_OS_BSD,PLATFORM_INFO_OS_MACOS].include?(clm_platform_get_os);end
end
