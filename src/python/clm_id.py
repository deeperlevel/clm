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

"""Lightweight procedural name-to-integer ID registry.

Names are case-sensitive by default. Set case sensitivity with
``clm_id_set_case_sensitive(False)`` to perform Unicode case-insensitive
lookups using :meth:`str.casefold`.

The module maintains one process-local registry. It is not synchronized for
concurrent mutation; coordinate updates externally when using threads.
"""


class _IdRegistry:
    """Private shared state; public operations are module-level functions."""

    registry: dict[str, int] = {}
    lookup: dict[str, str] = {}
    case_sensitive: bool = True


def _clm_id_normalize(name: str) -> str:
    """Convert a name into its lookup key for the current mode."""
    return name if _IdRegistry.case_sensitive else name.casefold()


def _clm_id_validate(name: str, identifier: int | None = None) -> None:
    """Validate the name and optional numeric ID."""
    if not isinstance(name, str) or not name:
        raise ValueError("name must be a non-empty string")
    if identifier is not None and (type(identifier) is not int):
        raise TypeError("identifier must be an integer")


def clm_id_register(name: str, identifier: int) -> None:
    """Register a new name-to-ID mapping.

    Args:
        name: Non-empty identifier name.
        identifier: Integer ID to associate with the name.

    Raises:
        ValueError: If the name is empty or not a string.
        TypeError: If ``identifier`` is not an integer.
        KeyError: If a matching name is already registered. In
            case-insensitive mode, differently cased spellings also match.

    Multiple different names may intentionally share the same numeric ID.
    """
    _clm_id_validate(name, identifier)
    key = _clm_id_normalize(name)
    if key in _IdRegistry.lookup:
        raise KeyError(f"ID name already registered: {name!r}")
    _IdRegistry.registry[name] = identifier
    _IdRegistry.lookup[key] = name


def clm_id_get(name: str) -> int:
    """Return the integer ID mapped to ``name``.

    Raises:
        ValueError: If ``name`` is empty or not a string.
        KeyError: If the name has not been registered.
    """
    _clm_id_validate(name)
    original = _IdRegistry.lookup[_clm_id_normalize(name)]
    return _IdRegistry.registry[original]


def clm_id_set(name: str, identifier: int) -> None:
    """Insert a name or replace its registered ID.

    In case-insensitive mode, updating an existing name with different
    capitalization preserves the spelling of its first registration.

    Raises:
        ValueError: If ``name`` is empty or not a string.
        TypeError: If ``identifier`` is not an integer.
    """
    _clm_id_validate(name, identifier)
    key = _clm_id_normalize(name)
    original = _IdRegistry.lookup.get(key)
    if original is None:
        _IdRegistry.registry[name] = identifier
        _IdRegistry.lookup[key] = name
    else:
        _IdRegistry.registry[original] = identifier


def clm_id_remove(name: str) -> None:
    """Remove a registered name.

    Raises:
        ValueError: If ``name`` is empty or not a string.
        KeyError: If the name has not been registered.
    """
    _clm_id_validate(name)
    key = _clm_id_normalize(name)
    original = _IdRegistry.lookup.pop(key)
    del _IdRegistry.registry[original]


def clm_id_set_case_sensitive(enabled: bool) -> None:
    """Select case-sensitive (default) or case-insensitive name matching.

    Rebuilds the lookup index without discarding the registered spellings.
    Changing to case-insensitive mode raises ``ValueError`` if existing
    names differ only by case (e.g. ``Host`` and ``host``). On failure,
    the registry and previous matching mode are unchanged.

    This affects registration, lookups, updates and removal.

    Args:
        enabled: ``True`` for exact-case matching; ``False`` for Unicode
            case-insensitive matching using :meth:`str.casefold`.

    Raises:
        TypeError: If ``enabled`` is not a boolean.
        ValueError: If names collide under the requested matching mode.
    """
    if type(enabled) is not bool:
        raise TypeError("enabled must be a bool")
    if _IdRegistry.case_sensitive == enabled:
        return

    new_lookup: dict[str, str] = {}
    for name in _IdRegistry.registry:
        key = name if enabled else name.casefold()
        if key in new_lookup:
            raise ValueError(
                f"Cannot disable case sensitivity: {new_lookup[key]!r} "
                f"and {name!r} would collide"
            )
        new_lookup[key] = name

    _IdRegistry.lookup = new_lookup
    _IdRegistry.case_sensitive = enabled


def clm_id_is_case_sensitive() -> bool:
    """Return ``True`` when registered names require matching letter case."""
    return _IdRegistry.case_sensitive


def clm_id_clear() -> None:
    """Remove all mappings without changing the case-sensitivity setting."""
    _IdRegistry.registry.clear()
    _IdRegistry.lookup.clear()


class IdRegistry:
    """Static convenience facade over the procedural ID registry API."""

    register = staticmethod(clm_id_register)
    get = staticmethod(clm_id_get)
    set = staticmethod(clm_id_set)
    remove = staticmethod(clm_id_remove)
    set_case_sensitive = staticmethod(clm_id_set_case_sensitive)
    is_case_sensitive = staticmethod(clm_id_is_case_sensitive)
    clear = staticmethod(clm_id_clear)


__all__ = [
    "clm_id_register", "clm_id_get", "clm_id_set", "clm_id_remove",
    "clm_id_set_case_sensitive", "clm_id_is_case_sensitive", "clm_id_clear",
    "IdRegistry",
]
