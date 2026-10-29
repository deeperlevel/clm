package clm
import ("runtime";"strconv";"unsafe")
const (
 PlatformInfoWindows = 0x0f100030
 PlatformInfoMacos = 0x0f100031
 PlatformInfoLinux = 0x0f100032
 PlatformInfoBsd = 0x0f100033
 PlatformInfoAndroid = 0x0f100034
 PlatformInfoIos = 0x0f100035
 PlatformInfoUnknown = 0x0f10003f
 PlatformInfoX86 = 0x0f100040
 PlatformInfoX64 = 0x0f100041
 PlatformInfoArm32 = 0x0f100042
 PlatformInfoArm64 = 0x0f100043
 PlatformInfoRiscv32 = 0x0f100044
 PlatformInfoRiscv64 = 0x0f100045
 PlatformInfoWasm32 = 0x0f100046
 PlatformInfoWasm64 = 0x0f100047
 PlatformInfoArchUnknown = 0x0f10004f
 PlatformInfoMsvc = 0x0f100050
 PlatformInfoGcc = 0x0f100051
 PlatformInfoClang = 0x0f100052
 PlatformInfoCompilerUnknown = 0x0f10005f
 PlatformInfoLittle = 0x0f100060
 PlatformInfoBig = 0x0f100061
 PlatformInfoEndianUnknown = 0x0f10006f
)
func ClmPlatformGetOS() int {switch runtime.GOOS {case "windows":return PlatformInfoWindows;case "darwin":return PlatformInfoMacos;case "linux":return PlatformInfoLinux;case "android":return PlatformInfoAndroid;case "ios":return PlatformInfoIos;case "freebsd","netbsd","openbsd","dragonfly":return PlatformInfoBsd};return PlatformInfoUnknown}
func ClmPlatformGetOSName() string {switch ClmPlatformGetOS(){case PlatformInfoWindows:return "Windows";case PlatformInfoMacos:return "macOS";case PlatformInfoLinux:return "Linux";case PlatformInfoAndroid:return "Android";case PlatformInfoIos:return "iOS";case PlatformInfoBsd:return "BSD"};return "Unknown"}
func ClmPlatformGetOSVersion() string {return "Unknown (compile-time OS version not provided)"}
func ClmPlatformGetArch() int {switch runtime.GOARCH {case "amd64":return PlatformInfoX64;case "386":return PlatformInfoX86;case "arm":return PlatformInfoArm32;case "arm64":return PlatformInfoArm64;case "riscv64":return PlatformInfoRiscv64;case "wasm":return PlatformInfoWasm32};return PlatformInfoArchUnknown}
func ClmPlatformGetArchName() string {switch ClmPlatformGetArch(){case PlatformInfoX64:return "x64";case PlatformInfoX86:return "x86";case PlatformInfoArm32:return "ARM32";case PlatformInfoArm64:return "ARM64";case PlatformInfoRiscv64:return "RISC-V 64";case PlatformInfoWasm32:return "WebAssembly 32"};return "Unknown"}
func ClmPlatformGetBits() int {return int(unsafe.Sizeof(uintptr(0))*8)}
func ClmPlatformGetEndian() int {var x uint16=1;if *(*byte)(unsafe.Pointer(&x))==1{return PlatformInfoLittle};return PlatformInfoBig}
func ClmPlatformGetCompiler() int {return PlatformInfoCompilerUnknown}
func ClmPlatformGetCompilerName() string {return "Go compiler (not a C compiler)"}
func ClmPlatformGetCompilerVersion() string {return runtime.Version()}
func ClmPlatformGetPathSeparator() byte {if ClmPlatformIsWindows(){return '\\'};return '/'}
func ClmPlatformGetNewline() string {if ClmPlatformIsWindows(){return "\r\n"};return "\n"}
func ClmPlatformIsWindows() bool {return ClmPlatformGetOS()==PlatformInfoWindows}
func ClmPlatformIsLinux() bool {return ClmPlatformGetOS()==PlatformInfoLinux}
func ClmPlatformIsBsd() bool {return ClmPlatformGetOS()==PlatformInfoBsd}
func ClmPlatformIsMacos() bool {return ClmPlatformGetOS()==PlatformInfoMacos}
func ClmPlatformIsAndroid() bool {return ClmPlatformGetOS()==PlatformInfoAndroid}
func ClmPlatformIsIos() bool {return ClmPlatformGetOS()==PlatformInfoIos}
func ClmPlatformIsMobile() bool {return ClmPlatformIsAndroid()||ClmPlatformIsIos()}
func ClmPlatformIsDesktop() bool {return ClmPlatformIsWindows()||ClmPlatformIsLinux()||ClmPlatformIsBsd()||ClmPlatformIsMacos()}
// _to copies a NUL-terminated string; return count excluding NUL, or -1 for short buffers.
func ClmPlatformCopyTo(s string, dst []byte) int {if len(dst)<len(s)+1{return -1};copy(dst,s);dst[len(s)]=0;return len(s)}
var _ = strconv.IntSize
