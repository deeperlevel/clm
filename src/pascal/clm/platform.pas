unit clm_platform;
{$mode objfpc}{$H+}
interface
uses SysUtils;
const
  PLATFORM_INFO_OS_WINDOWS = $0F100030;
  PLATFORM_INFO_OS_MACOS = $0F100031;
  PLATFORM_INFO_OS_LINUX = $0F100032;
  PLATFORM_INFO_OS_BSD = $0F100033;
  PLATFORM_INFO_OS_ANDROID = $0F100034;
  PLATFORM_INFO_OS_IOS = $0F100035;
  PLATFORM_INFO_OS_UNKNOWN = $0F10003F;
  PLATFORM_INFO_ARCH_X86 = $0F100040;
  PLATFORM_INFO_ARCH_X64 = $0F100041;
  PLATFORM_INFO_ARCH_ARM32 = $0F100042;
  PLATFORM_INFO_ARCH_ARM64 = $0F100043;
  PLATFORM_INFO_ARCH_RISCV32 = $0F100044;
  PLATFORM_INFO_ARCH_RISCV64 = $0F100045;
  PLATFORM_INFO_ARCH_WASM32 = $0F100046;
  PLATFORM_INFO_ARCH_WASM64 = $0F100047;
  PLATFORM_INFO_ARCH_UNKNOWN = $0F10004F;
  PLATFORM_INFO_COMPILER_MSVC = $0F100050;
  PLATFORM_INFO_COMPILER_GCC = $0F100051;
  PLATFORM_INFO_COMPILER_CLANG = $0F100052;
  PLATFORM_INFO_COMPILER_UNKNOWN = $0F10005F;
  PLATFORM_INFO_ENDIAN_LITTLE = $0F100060;
  PLATFORM_INFO_ENDIAN_BIG = $0F100061;
  PLATFORM_INFO_ENDIAN_UNKNOWN = $0F10006F;
function clm_platform_get_os: LongInt;
function clm_platform_get_os_name: string;
function clm_platform_get_os_version: string;
function clm_platform_get_arch: LongInt;
function clm_platform_get_arch_name: string;
function clm_platform_get_bits: LongInt;
function clm_platform_get_endian: LongInt;
function clm_platform_get_compiler: LongInt;
function clm_platform_get_compiler_name: string;
function clm_platform_get_compiler_version: string;
function clm_platform_get_path_separator: Char;
function clm_platform_get_newline: string;
function clm_platform_is_windows: Boolean;
function clm_platform_is_linux: Boolean;
function clm_platform_is_bsd: Boolean;
function clm_platform_is_macos: Boolean;
function clm_platform_is_android: Boolean;
function clm_platform_is_ios: Boolean;
function clm_platform_is_mobile: Boolean;
function clm_platform_is_desktop: Boolean;
implementation
function clm_platform_get_os: LongInt;
begin
{$IFDEF ANDROID} Result:=PLATFORM_INFO_OS_ANDROID;
{$ELSEIF defined(IOS)} Result:=PLATFORM_INFO_OS_IOS;
{$ELSEIF defined(WINDOWS)} Result:=PLATFORM_INFO_OS_WINDOWS;
{$ELSEIF defined(DARWIN)} Result:=PLATFORM_INFO_OS_MACOS;
{$ELSEIF defined(FREEBSD) or defined(NETBSD) or defined(OPENBSD)} Result:=PLATFORM_INFO_OS_BSD;
{$ELSEIF defined(LINUX)} Result:=PLATFORM_INFO_OS_LINUX;
{$ELSE} Result:=PLATFORM_INFO_OS_UNKNOWN;
{$ENDIF}
end;
function clm_platform_get_os_name: string;
begin
 case clm_platform_get_os of
 PLATFORM_INFO_OS_WINDOWS:Result:='Windows'; PLATFORM_INFO_OS_LINUX:Result:='Linux'; PLATFORM_INFO_OS_MACOS:Result:='macOS'; PLATFORM_INFO_OS_BSD:Result:='BSD'; PLATFORM_INFO_OS_ANDROID:Result:='Android'; PLATFORM_INFO_OS_IOS:Result:='iOS'; else Result:='Unknown';end;
end;
function clm_platform_get_os_version: string;begin Result:='Unknown (compile-time OS hint unavailable)';end;
function clm_platform_get_arch: LongInt;
begin
{$IFDEF CPUX86_64}Result:=PLATFORM_INFO_ARCH_X64;
{$ELSEIF defined(CPUI386)}Result:=PLATFORM_INFO_ARCH_X86;
{$ELSEIF defined(CPUAARCH64)}Result:=PLATFORM_INFO_ARCH_ARM64;
{$ELSEIF defined(CPUARM)}Result:=PLATFORM_INFO_ARCH_ARM32;
{$ELSE}Result:=PLATFORM_INFO_ARCH_UNKNOWN;
{$ENDIF}
end;
function clm_platform_get_arch_name: string;
begin case clm_platform_get_arch of PLATFORM_INFO_ARCH_X64:Result:='x64';PLATFORM_INFO_ARCH_X86:Result:='x86';PLATFORM_INFO_ARCH_ARM64:Result:='ARM64';PLATFORM_INFO_ARCH_ARM32:Result:='ARM32';else Result:='Unknown';end;end;
function clm_platform_get_bits: LongInt;begin Result:=SizeOf(Pointer)*8;end;
function clm_platform_get_endian: LongInt;
begin {$IFDEF ENDIAN_BIG} Result:=PLATFORM_INFO_ENDIAN_BIG; {$ELSE} Result:=PLATFORM_INFO_ENDIAN_LITTLE; {$ENDIF} end;
function clm_platform_get_compiler: LongInt;begin Result:=PLATFORM_INFO_COMPILER_UNKNOWN;end;
function clm_platform_get_compiler_name: string;begin Result:='Free Pascal compiler';end;
function clm_platform_get_compiler_version: string;begin Result:='Unknown';end;
function clm_platform_get_path_separator: Char;begin if clm_platform_get_os=PLATFORM_INFO_OS_WINDOWS then Result:=#92 else Result:='/';end;
function clm_platform_get_newline: string;begin if clm_platform_get_os=PLATFORM_INFO_OS_WINDOWS then Result:=#13#10 else Result:=#10;end;
function clm_platform_is_windows: Boolean;begin Result:=clm_platform_get_os=PLATFORM_INFO_OS_WINDOWS;end;
function clm_platform_is_linux: Boolean;begin Result:=clm_platform_get_os=PLATFORM_INFO_OS_LINUX;end;
function clm_platform_is_bsd: Boolean;begin Result:=clm_platform_get_os=PLATFORM_INFO_OS_BSD;end;
function clm_platform_is_macos: Boolean;begin Result:=clm_platform_get_os=PLATFORM_INFO_OS_MACOS;end;
function clm_platform_is_android: Boolean;begin Result:=clm_platform_get_os=PLATFORM_INFO_OS_ANDROID;end;
function clm_platform_is_ios: Boolean;begin Result:=clm_platform_get_os=PLATFORM_INFO_OS_IOS;end;
function clm_platform_is_mobile: Boolean;begin Result:=clm_platform_is_android or clm_platform_is_ios;end;
function clm_platform_is_desktop: Boolean;begin Result:=clm_platform_is_windows or clm_platform_is_linux or clm_platform_is_bsd or clm_platform_is_macos;end;
end.
