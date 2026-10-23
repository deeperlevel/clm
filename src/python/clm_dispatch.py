"""ID-based async message dispatch using CLM Container and CLM Depend.

Handlers accept (resolved_dependencies, message). Optional loggers accept a
Message envelope. Each public dispatch operation creates a correlation chain;
nested dispatches inherit the correlation using a context variable.
"""
from __future__ import annotations

import inspect
import threading
from contextvars import ContextVar
from dataclasses import dataclass
from typing import Any, Callable
from uuid import uuid4

from clm_container import (
    CONTAINER_SUCCESS, CONTAINER_TYPE_OBJECT, clm_container_add_object,
    clm_container_contains, clm_container_create, clm_container_delete_object,
    clm_container_destroy, clm_container_get_object,
)
from clm_depend import (clm_depend_resolve, clm_depend_get,
    clm_depend_get_info, clm_depend_unreference, DependLifetime)

HANDLER_CONTAINER = 0xFED001
LOGGER_CONTAINER = 0xFED002
COMMAND_LOGGER = 0xFED003
EVENT_LOGGER = 0xFED004
QUERY_LOGGER = 0xFED005
EXCEPTION_LOGGER = 0xFED006


@dataclass(frozen=True)
class Chain:
    """One dispatch hop and its tracing relationship to other hops."""
    id: str
    correlation_id: str
    causation_id: str


@dataclass(frozen=True)
class Message:
    """Envelope sent to a logger: trace information, payload, and dispatch kind."""
    chain: Chain
    message: Any
    context: str


class DispatchError(RuntimeError):
    """Invalid dispatch usage or a missing/duplicate registered handler."""


@dataclass(frozen=True)
class DispatchHandler:
    """A callback and the numeric DI service IDs it consumes."""
    dependencies: tuple[int, ...]
    handler: Callable[[list[Any], Any], Any]


class _DispatchStates:
    """Private process-wide dispatcher registry state."""
    initialized = False
    lock = threading.RLock()


_current_chain: ContextVar[Chain | None] = ContextVar("clm_dispatch_chain", default=None)


def clm_dispatch_start_new() -> Chain:
    """Create a root chain with a new correlation ID."""
    ident = uuid4().hex
    return Chain(id=ident, correlation_id=ident, causation_id="")


def clm_dispatch_next(chain: Chain) -> Chain:
    """Create a child chain preserving correlation and identifying its cause."""
    if not isinstance(chain, Chain):
        raise TypeError("chain must be a Chain")
    return Chain(id=uuid4().hex, correlation_id=chain.correlation_id, causation_id=chain.id)


def clm_dispatch_init() -> None:
    """Initialize handler and logger containers; repeat calls are harmless."""
    with _DispatchStates.lock:
        if _DispatchStates.initialized:
            return
        if clm_container_create(HANDLER_CONTAINER, CONTAINER_TYPE_OBJECT) != CONTAINER_SUCCESS:
            raise DispatchError("Handler container ID is already reserved")
        if clm_container_create(LOGGER_CONTAINER, CONTAINER_TYPE_OBJECT) != CONTAINER_SUCCESS:
            clm_container_destroy(HANDLER_CONTAINER)
            raise DispatchError("Logger container ID is already reserved")
        _DispatchStates.initialized = True


def clm_dispatch_destroy() -> None:
    """Release dispatcher registries; does not destroy DI dependencies."""
    with _DispatchStates.lock:
        if not _DispatchStates.initialized:
            return
        clm_container_destroy(HANDLER_CONTAINER)
        clm_container_destroy(LOGGER_CONTAINER)
        _DispatchStates.initialized = False


def clm_dispatch_register_handler(
    message_id: int,
    dependencies: list[int],
    callback: Callable[[list[Any], Any], Any],
) -> None:
    """Register exactly one handler per message ID; reject duplicate IDs."""
    if type(message_id) is not int or message_id < 0:
        raise ValueError("message_id must be a nonnegative integer")
    if not isinstance(dependencies, (list, tuple)) or any(type(i) is not int or i < 0 for i in dependencies):
        raise ValueError("dependencies must contain nonnegative integer IDs")
    if not callable(callback):
        raise TypeError("callback must be callable")
    clm_dispatch_init()
    with _DispatchStates.lock:
        status = clm_container_add_object(
            HANDLER_CONTAINER, message_id, DispatchHandler(tuple(dependencies), callback)
        )
        if status != CONTAINER_SUCCESS:
            raise DispatchError(f"Handler already registered or cannot be added: {message_id} (status {status})")


def clm_dispatch_unregister_handler(message_id: int) -> None:
    """Remove an existing handler, raising DispatchError if absent."""
    with _DispatchStates.lock:
        if not _DispatchStates.initialized or clm_container_delete_object(HANDLER_CONTAINER, message_id) != CONTAINER_SUCCESS:
            raise DispatchError(f"Handler not registered: {message_id}")


def clm_dispatch_register_logger(logger_id: int, callback: Callable[[Message], Any]) -> None:
    """Register a sync or async envelope logger under one of the four logger IDs."""
    if logger_id not in (COMMAND_LOGGER, EVENT_LOGGER, QUERY_LOGGER, EXCEPTION_LOGGER):
        raise ValueError("Unknown logger ID")
    if not callable(callback):
        raise TypeError("logger must be callable")
    clm_dispatch_init()
    with _DispatchStates.lock:
        status = clm_container_add_object(LOGGER_CONTAINER, logger_id, callback)
        if status != CONTAINER_SUCCESS:
            raise DispatchError(f"Logger already registered: {logger_id}")


def clm_dispatch_unregister_logger(logger_id: int) -> None:
    """Remove an existing logger, raising DispatchError if absent."""
    with _DispatchStates.lock:
        if not _DispatchStates.initialized or clm_container_delete_object(LOGGER_CONTAINER, logger_id) != CONTAINER_SUCCESS:
            raise DispatchError(f"Logger not registered: {logger_id}")


async def _await_if_needed(value: Any) -> Any:
    """Accept ordinary or awaitable results from handlers and loggers."""
    if inspect.isawaitable(value):
        return await value
    return value


async def clm_dispatch_invoke(message_id: int, message: Any) -> Any:
    """Find a handler, resolve its DI dependencies, and call it with the payload."""
    if not _DispatchStates.initialized:
        clm_dispatch_init()
    status, handler = clm_container_get_object(HANDLER_CONTAINER, message_id)
    if status != CONTAINER_SUCCESS:
        raise DispatchError(f"{message_id} has no handler")
    # Do not hold dispatch registry locks across DI resolution or awaits.
    # Resolve one at a time so partial failures do not leak transient references.
    # Handler receives VALUES (historic Dispatch contract), not Depend IDs.
    acquired = []
    try:
        for service_id in handler.dependencies:
            iid = clm_depend_resolve(service_id)
            acquired.append(iid)
        values = [clm_depend_get(iid) for iid in acquired]
        return await _await_if_needed(handler.handler(values, message))
    finally:
        # Depend caches own their lifetimes; only transient resolves acquire an
        # invocation-owned reference. Run this on exception and cancellation too.
        for iid in reversed(acquired):
            if clm_depend_get_info(iid).lifetime == DependLifetime.TRANSIENT:
                clm_depend_unreference(iid)


async def _dispatch(
    message_id: int, message: Any, logger_id: int, context: str,
    log: bool = True, *, chain: Chain | None = None,
) -> Any:
    """Dispatch with optional best-effort logging and isolated trace context.

    Logger lookup and callback exceptions are silently ignored; handler and
    dependency errors still propagate. Async task cancellation is not suppressed.
    """
    if chain is not None and not isinstance(chain, Chain):
        raise TypeError("chain must be a Chain or None")
    parent = chain if chain is not None else _current_chain.get()
    current = clm_dispatch_next(parent) if parent is not None else clm_dispatch_start_new()
    token = _current_chain.set(current)
    try:
        if not _DispatchStates.initialized:
            clm_dispatch_init()
        if log:
            # Logging is best-effort: lookup and callback failures must not
            # interfere with the actual handler or dependency resolution.
            try:
                status, logger = clm_container_get_object(LOGGER_CONTAINER, logger_id)
                if status == CONTAINER_SUCCESS:
                    await _await_if_needed(logger(Message(current, message, context)))
            except Exception:
                pass  # Intentionally ignore optional logging failures.
        return await clm_dispatch_invoke(message_id, message)
    finally:
        _current_chain.reset(token)


async def clm_dispatch_execute(message_id: int, message: Any, log: bool = True, *, chain: Chain | None = None) -> Any:
    """Dispatch a command; optionally log a COMMAND envelope before invoking."""
    return await _dispatch(message_id, message, COMMAND_LOGGER, "command", log, chain=chain)


async def clm_dispatch_emit(message_id: int, message: Any, log: bool = True, *, chain: Chain | None = None) -> Any:
    """Dispatch an event to its single registered handler (not fan-out)."""
    return await _dispatch(message_id, message, EVENT_LOGGER, "event", log, chain=chain)


async def clm_dispatch_query(message_id: int, message: Any, log: bool = True, *, chain: Chain | None = None) -> Any:
    """Dispatch a query and return the handler's result."""
    return await _dispatch(message_id, message, QUERY_LOGGER, "query", log, chain=chain)


async def clm_dispatch_caught(message_id: int, message: Any, log: bool = True, *, chain: Chain | None = None) -> Any:
    """Dispatch an exception-category message; not an automatic catch-all."""
    return await _dispatch(message_id, message, EXCEPTION_LOGGER, "exception", log, chain=chain)


class Dispatch:
    """Optional convenience facade over the procedural dispatcher."""
    start_new = staticmethod(clm_dispatch_start_new)
    next = staticmethod(clm_dispatch_next)
    init = staticmethod(clm_dispatch_init)
    destroy = staticmethod(clm_dispatch_destroy)
    register_handler = staticmethod(clm_dispatch_register_handler)
    unregister_handler = staticmethod(clm_dispatch_unregister_handler)
    register_logger = staticmethod(clm_dispatch_register_logger)
    unregister_logger = staticmethod(clm_dispatch_unregister_logger)
    invoke = staticmethod(clm_dispatch_invoke)
    execute = staticmethod(clm_dispatch_execute)
    emit = staticmethod(clm_dispatch_emit)
    query = staticmethod(clm_dispatch_query)
    caught = staticmethod(clm_dispatch_caught)
