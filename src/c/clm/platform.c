/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

#include "platform.h"

static clm_id clm_platform_ensure_string(clm_id id, const char *value)
{
	if (!clm_bpa_is_existing(id))
		clm_string_new(id, value);
	return id;
}

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

