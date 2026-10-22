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

"""Procedural, ID-based dependency injection built on CLM's object container.

One registry with at most one active scope is supported in this first version.
EAGER and SINGLETON are aliases and construct during registration; LAZY
constructs upon first resolution. Services are owned by their caller unless
an explicit cleanup policy is added by the application.
"""
from dataclasses import dataclass, field
from enum import IntEnum
from threading import RLock
from typing import Any, Callable

from clm_container import (
    CONTAINER_SUCCESS,
    CONTAINER_TYPE_OBJECT,
    clm_container_create,
    clm_container_destroy,
    clm_container_add_object,
    clm_container_get_object,
    clm_container_delete_object
)

DEPEND_CONTAINER_ID = 0xFF01
DEPEND_SCOPED_CONTAINER_ID = 0xFF02


class DependStates:
    """Private module-level registry state; no OOP behavior is required."""
    container_id = DEPEND_CONTAINER_ID
    scoped_container_id = DEPEND_SCOPED_CONTAINER_ID
    sealed = False
    initialized = False
    scope_active = False
    constructing: set[int] = set()
    lock = RLock()


class DependStatus(IntEnum):
    SUCCESS = 0
    ALREADY_REGISTERED = 1
    NOT_REGISTERED = 2
    SEALED = 3
    INVALID_ARGUMENT = 4
    MISSING_DEPENDENCY = 5
    INCOMPATIBLE_LIFETIME = 6
    CIRCULAR_DEPENDENCY = 7
    SCOPE_ALREADY_ACTIVE = 8
    SCOPE_NOT_ACTIVE = 9
    SCOPE_REQUIRED = 10
    CONSTRUCTOR_LOCKED = 11


class DependLifetime(IntEnum):
    EAGER = 0x01
    SINGLETON = 0x01  # An alias for EAGER, as in the original draft.
    LAZY = 0x02
    SCOPED = 0x03
    TRANSIENT = 0x04


DependConstructor = Callable[[list[Any]], Any]
DependRegistration = tuple[int, list[int], DependConstructor]


class DependError(RuntimeError):
    """Resolution failure with a portable status and the failing service ID."""
    def __init__(self, status: DependStatus, service_id: int, message: str = ""):
        self.status = status
        self.service_id = service_id
        super().__init__(message or f"Depend {status.name}: service {service_id}")


@dataclass
class DependItem:
    """Factory metadata and optional cached instance for one service."""
    id: int
    dependencies: tuple[int, ...]
    constructor: DependConstructor
    lifetime: DependLifetime
    use_for: int
    instance: Any = None
    created: bool = False
    lock: RLock = field(default_factory=RLock, repr=False)


def _valid_id(value: Any) -> bool:
    return type(value) is int and value >= 0


def _lookup(service_id: int) -> DependItem:
    status, item = clm_container_get_object(DependStates.container_id, service_id)
    if status != CONTAINER_SUCCESS:
        raise DependError(DependStatus.NOT_REGISTERED, service_id)
    return item


def _resolve(service_id: int, parent_lifetime: DependLifetime | None = None) -> Any:
    item = _lookup(service_id)
    if parent_lifetime in (DependLifetime.EAGER, DependLifetime.LAZY) and item.lifetime == DependLifetime.SCOPED:
        raise DependError(DependStatus.INCOMPATIBLE_LIFETIME, service_id)
    if item.lifetime == DependLifetime.SCOPED and not DependStates.scope_active:
        raise DependError(DependStatus.SCOPE_REQUIRED, service_id)
    if item.lifetime == DependLifetime.SCOPED:
        status, value = clm_container_get_object(DependStates.scoped_container_id, service_id)
        if status == CONTAINER_SUCCESS:
            return value
    elif item.lifetime in (DependLifetime.EAGER, DependLifetime.LAZY) and item.created:
        return item.instance

    if service_id in DependStates.constructing:
        raise DependError(DependStatus.CIRCULAR_DEPENDENCY, service_id)

    DependStates.constructing.add(service_id)
    try:
        args = []
        for dependency_id in item.dependencies:
            try:
                args.append(_resolve(dependency_id, item.lifetime))
            except DependError as exc:
                if exc.status == DependStatus.NOT_REGISTERED:
                    raise DependError(DependStatus.MISSING_DEPENDENCY, dependency_id) from exc
                raise
        value = item.constructor(args)
        if item.lifetime == DependLifetime.SCOPED:
            code = clm_container_add_object(DependStates.scoped_container_id, service_id, value)
            if code != CONTAINER_SUCCESS:
                raise RuntimeError(f"Unable to cache scoped service {service_id}: {code}")
        elif item.lifetime in (DependLifetime.EAGER, DependLifetime.LAZY):
            item.instance = value
            item.created = True
        return value
    finally:
        DependStates.constructing.remove(service_id)


def clm_depend_initialize() -> None:
    """Initialize the single registry; repeat calls leave existing registrations intact.

    Raise RuntimeError if either reserved container ID belongs to another user.
    """
    with DependStates.lock:
        if DependStates.initialized:
            return
        if clm_container_create(DependStates.container_id, CONTAINER_TYPE_OBJECT) != CONTAINER_SUCCESS:
            raise RuntimeError("Registration container ID is already in use")
        if clm_container_create(DependStates.scoped_container_id, CONTAINER_TYPE_OBJECT) != CONTAINER_SUCCESS:
            clm_container_destroy(DependStates.container_id)
            raise RuntimeError("Scoped container ID is already in use")
        DependStates.sealed = False
        DependStates.scope_active = False
        DependStates.initialized = True


def _require_initialized() -> None:
    if not DependStates.initialized:
        raise RuntimeError("Call clm_depend_initialize() first")


def clm_depend_resolve(service_id: int) -> Any:
    """Resolve a service ID, creating its instance according to its lifetime.

    Raise DependError with an explanatory DependStatus on missing services,
    circular dependencies, invalid lifetimes, or required scopes.
    """
    with DependStates.lock:
        _require_initialized()
        if not _valid_id(service_id):
            raise DependError(DependStatus.INVALID_ARGUMENT, service_id)
        return _resolve(service_id)


def clm_depend_construct(service_id: int, constructor: DependConstructor) -> None:
    """Replace an unconstructed service's factory; raise DependError otherwise.

    An already created singleton or eager service cannot have its constructor
    changed. Use unregister/register before construction instead.
    """
    with DependStates.lock:
        _require_initialized()
        if DependStates.sealed:
            raise DependError(DependStatus.SEALED, service_id)
        if not callable(constructor):
            raise DependError(DependStatus.INVALID_ARGUMENT, service_id)
        item = _lookup(service_id)
        if item.created or service_id in DependStates.constructing:
            raise DependError(DependStatus.CONSTRUCTOR_LOCKED, service_id)
        if item.lifetime == DependLifetime.SCOPED:
            status, _ = clm_container_get_object(DependStates.scoped_container_id, service_id)
            if status == CONTAINER_SUCCESS:
                raise DependError(DependStatus.CONSTRUCTOR_LOCKED, service_id)
        item.constructor = constructor


def clm_depend_register(
    service_id: int,
    dependencies: list[int],
    constructor: DependConstructor,
    lifetime: DependLifetime = DependLifetime.EAGER,
    use_for: int = 0,
) -> int:
    """Register a service and eagerly construct EAGER/SINGLETON registrations.

    `use_for` is retained as metadata without alias-resolution semantics.
    Returns a DependStatus value; constructor errors propagate unchanged.
    EAGER registration rolls back if construction fails.
    """
    with DependStates.lock:
        _require_initialized()
        if DependStates.sealed:
            return DependStatus.SEALED
        if (not _valid_id(service_id) or not _valid_id(use_for) or
            not isinstance(dependencies, (list, tuple)) or
            any(not _valid_id(dep) for dep in dependencies) or
            not callable(constructor)):
            return DependStatus.INVALID_ARGUMENT
        try:
            lifetime = DependLifetime(lifetime)
        except (ValueError, TypeError):
            return DependStatus.INVALID_ARGUMENT
        try:
            _lookup(service_id)
        except DependError as exc:
            if exc.status != DependStatus.NOT_REGISTERED:
                raise
        else:
            return DependStatus.ALREADY_REGISTERED
        item = DependItem(service_id, tuple(dependencies), constructor, lifetime, use_for)
        status = clm_container_add_object(DependStates.container_id, service_id, item)
        if status != CONTAINER_SUCCESS:
            raise RuntimeError(f"Cannot register service: container error {status}")
        if lifetime == DependLifetime.EAGER:
            try:
                _resolve(service_id)
            except DependError as exc:
                clm_container_delete_object(DependStates.container_id, service_id)
                return exc.status
            except BaseException:
                clm_container_delete_object(DependStates.container_id, service_id)
                raise
        return DependStatus.SUCCESS


def clm_depend_unregister(service_id: int) -> None:
    """Remove a service's registration and cached instance (without disposing it)."""
    with DependStates.lock:
        _require_initialized()
        if DependStates.sealed:
            raise DependError(DependStatus.SEALED, service_id)
        if service_id in DependStates.constructing:
            raise DependError(DependStatus.CONSTRUCTOR_LOCKED, service_id)
        _lookup(service_id)
        clm_container_delete_object(DependStates.container_id, service_id)
        clm_container_delete_object(DependStates.scoped_container_id, service_id)


def clm_depend_invoke(services: list[int], func: Callable[..., Any], *args: Any) -> Any:
    """Resolve registered services and invoke func with dependencies then extra args.

    The callback runs outside the registry lock to avoid deadlocks when it
    calls Depend or Dispatch again. Returns the callback result unchanged.
    """
    with DependStates.lock:
        _require_initialized()
        if not callable(func) or not isinstance(services, (list, tuple)):
            raise DependError(DependStatus.INVALID_ARGUMENT, -1)
        resolved = [clm_depend_resolve(service_id) for service_id in services]
    return func(*resolved, *args)


async def clm_depend_invoke_async(
    services: list[int], func: Callable[..., Any], *args: Any
) -> Any:
    """Resolve services, call an async function, and return its awaited result.

    The result must be awaitable. Dependency factories remain synchronous.
    """
    import inspect

    if not callable(func) or not inspect.iscoroutinefunction(func):
        raise DependError(DependStatus.INVALID_ARGUMENT, -1)
    result = clm_depend_invoke(services, func, *args)
    if not inspect.isawaitable(result):
        raise DependError(DependStatus.INVALID_ARGUMENT, -1)
    return await result


def clm_depend_begin_scope() -> None:
    """Begin the one active scope; scoped instances cache until end_scope()."""
    with DependStates.lock:
        _require_initialized()
        if DependStates.scope_active:
            raise DependError(DependStatus.SCOPE_ALREADY_ACTIVE, -1)
        DependStates.scope_active = True


def clm_depend_end_scope() -> None:
    """End the current scope and discard cached references to scoped services."""
    with DependStates.lock:
        _require_initialized()
        if not DependStates.scope_active:
            raise DependError(DependStatus.SCOPE_NOT_ACTIVE, -1)
        from clm_container import clm_container_clear
        clm_container_clear(DependStates.scoped_container_id)
        DependStates.scope_active = False


def clm_depend_seal() -> None:
    """Prevent subsequent registrations, unregistrations, and factory changes."""
    with DependStates.lock:
        _require_initialized()
        DependStates.sealed = True


def clm_depend_destroy() -> None:
    """Clear both internal containers and reset the registry to uninitialized.

    Does not invoke arbitrary cleanup methods on registered services.
    """
    with DependStates.lock:
        if not DependStates.initialized:
            return
        if DependStates.constructing:
            raise RuntimeError("Cannot destroy Depend while constructing a service")
        clm_container_destroy(DependStates.scoped_container_id)
        clm_container_destroy(DependStates.container_id)
        DependStates.initialized = False
        DependStates.scope_active = False
        DependStates.sealed = False
        DependStates.constructing.clear()


class Depend:
    """Optional static facade over the single procedural registry."""
    initialize = staticmethod(clm_depend_initialize)
    resolve = staticmethod(clm_depend_resolve)
    construct = staticmethod(clm_depend_construct)
    register = staticmethod(clm_depend_register)
    unregister = staticmethod(clm_depend_unregister)
    invoke = staticmethod(clm_depend_invoke)
    invoke_async = staticmethod(clm_depend_invoke_async)
    begin_scope = staticmethod(clm_depend_begin_scope)
    end_scope = staticmethod(clm_depend_end_scope)
    seal = staticmethod(clm_depend_seal)
    destroy = staticmethod(clm_depend_destroy)
