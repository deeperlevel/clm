# =============================================================================
# CLM — Cross Language Modules
#
# An initiative by Deeperlevel, developed with community contributions.
# https://github.com/deeperlevel/clm
# Contact: deeparchive@outlook.com
#
# Public Domain — CC0 1.0 Universal
# https://creativecommons.org/publicdomain/zero/1.0/
#
# Free to use, modify, redistribute, and bundle for any purpose.
# Attribution and retention of this notice are appreciated, not required.
#
# Provided AS IS, without warranty or liability.
#
# Don't start from scratch, bro.
# =============================================================================

"""Small procedural host-platform identification and environment access.

OS/device identification is best-effort. In particular, generic Python has no
portable means of identifying game-console hardware. No hardware monitoring or
performance metrics are included.
"""

import os
import platform
import sys

__all__ = [
    "clm_platform_get_os_name",
    "clm_platform_get_os_version",
    "clm_platform_is_mobile",
    "clm_platform_is_console",
    "clm_platform_is_pc",
    "clm_platform_is_linux",
    "clm_platform_is_windows",
    "clm_platform_get_environment_variable",
    "clm_platform_get_environment_variables",
]


def _os_name() -> str:
    """Identify OS family without treating Android as generic Linux."""
    system = platform.system().lower()
    target = sys.platform.lower()
    android_api = getattr(sys, "getandroidapilevel", None)

    if target == "android" or callable(android_api) or system == "android":
        return "Android"
    if target == "ios" or system in ("ios", "ipados"):
        return "iOS"
    if system == "windows" or target == "win32":
        return "Windows"
    if system == "darwin" or target == "darwin":
        return "macOS"
    if system == "linux" or target.startswith("linux"):
        return "Linux"
    if system == "freebsd" or target.startswith("freebsd"):
        return "FreeBSD"
    if system == "openbsd" or target.startswith("openbsd"):
        return "OpenBSD"
    if system == "netbsd" or target.startswith("netbsd"):
        return "NetBSD"
    return "Unknown"


def clm_platform_get_os_name() -> str:
    """Return a recognizable host OS family name, or 'Unknown'."""
    return _os_name()


# noinspection calling-non-callable
def clm_platform_get_os_version() -> str:
    """Return the OS-reported version string, or 'Unknown' when unavailable.

    This is not necessarily a marketing version, Linux distribution version,
    or the actual host OS version when running inside a container.
    """
    try:
        name = _os_name()
        if name == "Android":
            android_api = getattr(sys, "getandroidapilevel", None)
            if callable(android_api):
                api = android_api()
                if api is not None:
                    return str(api)
        version = platform.version()
        if isinstance(version, str) and version.strip():
            return version
        release = platform.release()
        if isinstance(release, str) and release.strip():
            return release
    except (OSError, ValueError, RuntimeError):
        pass
    return "Unknown"


def clm_platform_is_mobile() -> bool:
    """True when the current runtime identifies as Android or iOS."""
    return _os_name() in ("Android", "iOS")


def clm_platform_is_console() -> bool:
    """Best-effort console indicator.

    Standard Python provides no reliable cross-platform game-console detector.
    Returns False on ordinary supported Python hosts; False does NOT prove
    the hardware is not a console.
    """
    return False


def clm_platform_is_pc() -> bool:
    """True for recognized desktop-class OS families, not hardware proof."""
    return _os_name() in ("Windows", "macOS", "Linux", "FreeBSD", "OpenBSD", "NetBSD")


def clm_platform_is_linux() -> bool:
    """True for generic Linux OS identification (Android excluded)."""
    return _os_name() == "Linux"


def clm_platform_is_windows() -> bool:
    """True for Windows OS identification."""
    return _os_name() == "Windows"


def clm_platform_get_environment_variable(name: str, default: str | None = None) -> str | None:
    """Get an environment variable; return default when it is absent."""
    if not isinstance(name, str) or not name or "=" in name or "\x00" in name:
        raise ValueError("name must be a nonempty environment variable name without '=' or NUL")
    if default is not None and not isinstance(default, str):
        raise TypeError("default must be a string or None")
    return os.environ.get(name, default)


def clm_platform_get_environment_variables() -> dict[str, str]:
    """Return a detached snapshot of current process environment variables."""
    return dict(os.environ)
