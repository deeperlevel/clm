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

"""Lightweight, dependency-free User-Agent parsing for CLM.

This is heuristic identification for logging and coarse analytics, not a reliable
security signal or a complete parser of every browser/device token.
"""

import re
from dataclasses import dataclass
UNKNOWN = "Unknown"

# More specific browser tokens must precede Chrome and Safari, because many
# Chromium browsers include Chrome/ and iOS browsers include Safari/.
_BROWSER_RULES: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("Edge", re.compile(r"(?:Edg|Edge|EdgA|EdgiOS)/([0-9.]+)", re.I)),
    ("Opera", re.compile(r"(?:OPR|Opera)/([0-9.]+)", re.I)),
    ("Samsung Internet", re.compile(r"SamsungBrowser/([0-9.]+)", re.I)),
    ("Firefox", re.compile(r"(?:Firefox|FxiOS)/([0-9.]+)", re.I)),
    ("Chrome", re.compile(r"(?:Chrome|CriOS)/([0-9.]+)", re.I)),
    ("Safari", re.compile(r"Version/([0-9.]+).*Safari/", re.I)),
)

# Android must precede Linux, iOS before macOS because of platform tokens.
_OS_RULES: tuple[tuple[str, re.Pattern[str]], ...] = (
    ("Windows", re.compile(r"Windows NT ([0-9.]+)", re.I)),
    ("iOS", re.compile(r"(?:iPhone OS|CPU(?: iPhone)? OS) ([0-9_]+)", re.I)),
    ("Android", re.compile(r"Android[ /]([0-9.]+)", re.I)),
    ("macOS", re.compile(r"Mac OS X ([0-9_.]+)", re.I)),
    ("Linux", re.compile(r"Linux", re.I)),
)

_BOT_PATTERNS = ("bot", "crawler", "spider", "slurp", "bingpreview")


@dataclass(frozen=True, slots=True)
class UserAgent:
    """Parsed User-Agent result with read-only fields.

    Values are heuristic and may be spoofed. Unknown names/versions use
    ``'Unknown'``. Device categories are ``Desktop``, ``Mobile``, ``Tablet``.

    ``UserAgent.parse(text)`` is the convenience facade for ``clm_ua_parse``.
    """

    raw: str
    browser: str = UNKNOWN
    browser_version: str = UNKNOWN
    os: str = UNKNOWN
    os_version: str = UNKNOWN
    device_type: str = "Desktop"
    is_mobile: bool = False
    is_tablet: bool = False
    is_desktop: bool = True
    is_bot: bool = False

    # Assign below the function declaration, so there is only one parser.


def clm_ua_browser(raw: str | None) -> tuple[str, str]:
    """Return ``(browser_name, version)`` from a User-Agent string.

    Checks specific browser tokens first to avoid identifying Edge or Opera
    as Chrome. Returns ``('Unknown', 'Unknown')`` if no rule matches.
    """
    value = raw or ""
    for name, pattern in _BROWSER_RULES:
        match = pattern.search(value)
        if match:
            return name, match.group(1)
    return UNKNOWN, UNKNOWN


def clm_ua_os(raw: str | None) -> tuple[str, str]:
    """Return ``(operating_system, version)`` from a User-Agent string.

    Replace underscores in versions with periods. Linux has no version rule.
    """
    value = raw or ""
    for name, pattern in _OS_RULES:
        match = pattern.search(value)
        if match:
            version = match.group(1).replace("_", ".") if match.lastindex else UNKNOWN
            return name, version
    return UNKNOWN, UNKNOWN


def clm_ua_is_bot(raw: str | None) -> bool:
    """Heuristically identify common crawler tokens (not bot verification)."""
    lowered = (raw or "").lower()
    return any(token in lowered for token in _BOT_PATTERNS)


def clm_ua_device_type(raw: str | None) -> str:
    """Estimate ``Tablet``, ``Mobile``, or ``Desktop`` from UA tokens.

    Android without a ``Mobile`` token is generally a tablet. Some newer
    iPads use desktop-style strings and cannot be identified reliably.
    """
    lowered = (raw or "").lower()
    if "ipad" in lowered or "tablet" in lowered or "kindle" in lowered or "silk/" in lowered:
        return "Tablet"
    if "android" in lowered:
        return "Mobile" if "mobile" in lowered else "Tablet"
    if "mobi" in lowered or "iphone" in lowered or "ipod" in lowered:
        return "Mobile"
    return "Desktop"


def clm_ua_parse(ua_string: str | None) -> UserAgent:
    """Parse a User-Agent string into a :class:`UserAgent` result.

    ``None`` becomes an empty string. Browser, OS, device, and bot detection
    are approximate and intended for request logging and coarse analytics.

    Args:
        ua_string: Raw HTTP User-Agent value, or ``None``.

    Returns:
        Immutable ``UserAgent`` with detected fields and flags.
    """
    if ua_string is not None and not isinstance(ua_string, str):
        raise TypeError("ua_string must be a string or None")
    raw = ua_string or ""
    browser, browser_version = clm_ua_browser(raw)
    os_name, os_version = clm_ua_os(raw)
    device_type = clm_ua_device_type(raw)
    return UserAgent(
        raw=raw,
        browser=browser,
        browser_version=browser_version,
        os=os_name,
        os_version=os_version,
        device_type=device_type,
        is_mobile=device_type == "Mobile",
        is_tablet=device_type == "Tablet",
        is_desktop=device_type == "Desktop",
        is_bot=clm_ua_is_bot(raw)
    )

__all__ = [
    "UserAgent",
    "clm_ua_parse",
    "clm_ua_browser",
    "clm_ua_os",
    "clm_ua_device_type",
    "clm_ua_is_bot"
]
