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

"""Opt-in scheduler coordinating CLM Depend, Container and Storage disposal."""
from dataclasses import dataclass
from enum import IntEnum
from threading import Event, RLock, Thread, current_thread
from time import monotonic
from typing import Any

from clm_container import CONTAINER_SUCCESS, clm_container_dispose, clm_container_pending_count
from clm_storage import STORAGE_SUCCESS, clm_storage_dispose, clm_storage_pending_count
from clm_depend import clm_depend_dispose, clm_depend_pending_count


class ManagedAlgorithm(IntEnum):
    MANUAL = 0x01
    INTERVAL = 0x02
    THRESHOLD = 0x03
    HYBRID = 0x04


class ManagedThread(IntEnum):
    CALLER = 0x01
    WORKER = 0x02


class ManagedStatus(IntEnum):
    SUCCESS = 0
    NOT_INITIALIZED = 1
    INVALID_ARGUMENT = 2
    ACTIVE = 3


@dataclass(frozen=True)
class ManagedStats:
    cycles: int = 0
    disposed_transients: int = 0
    disposed_containers: int = 0
    disposed_storages: int = 0
    failures: int = 0
    last_error: str | None = None


_lock = RLock()
_stop = Event()
_initialized = False
_enabled = False
_algorithm = ManagedAlgorithm.MANUAL
_thread_mode = ManagedThread.CALLER
_interval_ms = 5000
_threshold = 10
_worker: Thread | None = None
_last_collection = 0.0
_stats = ManagedStats()


def clm_managed_initialize() -> ManagedStatus:
    global _initialized
    with _lock:
        _initialized = True
    return ManagedStatus.SUCCESS


def _worker_loop():
    while True:
        with _lock:
            active = _initialized and _enabled and _algorithm != ManagedAlgorithm.MANUAL and _thread_mode == ManagedThread.WORKER
            interval = _interval_ms / 1000
        if not active or _stop.wait(interval):
            return
        try:
            clm_managed_poll(force_worker=True)
        except Exception:
            # Error captured by collect() in ManagedStats; worker remains alive.
            pass


def _start_worker_if_needed():
    global _worker
    if (_initialized and _enabled and _algorithm != ManagedAlgorithm.MANUAL and
            _thread_mode == ManagedThread.WORKER and (_worker is None or not _worker.is_alive())):
        _stop.clear()
        _worker = Thread(target=_worker_loop, name='CLM-Managed', daemon=True)
        _worker.start()


def clm_managed_enable() -> ManagedStatus:
    global _enabled, _last_collection
    with _lock:
        if not _initialized:
            return ManagedStatus.NOT_INITIALIZED
        _enabled = True
        _last_collection = monotonic()
        _start_worker_if_needed()
    return ManagedStatus.SUCCESS


def clm_managed_disable() -> ManagedStatus:
    global _enabled, _worker
    with _lock:
        if not _initialized:
            return ManagedStatus.NOT_INITIALIZED
        _enabled = False
        _stop.set()
        worker = _worker
    if worker is not None and worker is not current_thread():
        worker.join()
    with _lock:
        if _worker is worker:
            _worker = None
    return ManagedStatus.SUCCESS


def clm_managed_set_algorithm(algorithm: int) -> ManagedStatus:
    global _algorithm
    try:
        selected = ManagedAlgorithm(algorithm)
    except (ValueError, TypeError):
        return ManagedStatus.INVALID_ARGUMENT
    with _lock:
        if not _initialized:
            return ManagedStatus.NOT_INITIALIZED
        if _enabled:
            return ManagedStatus.ACTIVE  # Configure while disabled to avoid scheduler races.
        _algorithm = selected
    return ManagedStatus.SUCCESS


def clm_managed_set_interval(milliseconds: int) -> ManagedStatus:
    global _interval_ms
    if type(milliseconds) is not int or milliseconds < 1:
        return ManagedStatus.INVALID_ARGUMENT
    with _lock:
        if not _initialized:
            return ManagedStatus.NOT_INITIALIZED
        if _enabled:
            return ManagedStatus.ACTIVE
        _interval_ms = milliseconds
    return ManagedStatus.SUCCESS


def clm_managed_set_threshold(count: int) -> ManagedStatus:
    """Set minimum number of eligible resources that triggers threshold collection."""
    global _threshold
    if type(count) is not int or count < 1:
        return ManagedStatus.INVALID_ARGUMENT
    with _lock:
        if not _initialized: return ManagedStatus.NOT_INITIALIZED
        if _enabled: return ManagedStatus.ACTIVE
        _threshold = count
    return ManagedStatus.SUCCESS


def clm_managed_set_thread(mode: int) -> ManagedStatus:
    global _thread_mode
    try:
        selected = ManagedThread(mode)
    except (ValueError, TypeError):
        return ManagedStatus.INVALID_ARGUMENT
    with _lock:
        if not _initialized:
            return ManagedStatus.NOT_INITIALIZED
        if _enabled:
            return ManagedStatus.ACTIVE
        _thread_mode = selected
    return ManagedStatus.SUCCESS


def clm_managed_collect() -> tuple[int, int, int]:
    """Immediately sweep in Depend -> Container -> Storage order, even if disabled.

    Returns (disposed_transients, disposed_containers, disposed_storages).
    Exceptions propagate after recording a failure; earlier sweeps may have succeeded.
    """
    global _stats, _last_collection
    with _lock:
        if not _initialized:
            raise RuntimeError('Call clm_managed_initialize() first')
        try:
            depend_count = clm_depend_dispose()
            cs, container_count = clm_container_dispose()
            if cs != CONTAINER_SUCCESS:
                raise RuntimeError(f'Container dispose failed: {cs}')
            ss, storage_count = clm_storage_dispose()
            if ss != STORAGE_SUCCESS:
                raise RuntimeError(f'Storage dispose failed: {ss}')
        except Exception as exc:
            _stats = ManagedStats(_stats.cycles, _stats.disposed_transients,
                _stats.disposed_containers, _stats.disposed_storages,
                _stats.failures + 1, str(exc))
            raise
        _stats = ManagedStats(_stats.cycles + 1,
            _stats.disposed_transients + depend_count,
            _stats.disposed_containers + container_count,
            _stats.disposed_storages + storage_count, _stats.failures, None)
        _last_collection = monotonic()
        return depend_count, container_count, storage_count


def clm_managed_poll(force_worker: bool = False) -> bool:
    """Run one collection if selected time/eligible-resource threshold is reached.

    Caller-mode calls are inert in worker mode. Worker calls are internal only.
    """
    with _lock:
        if not (_initialized and _enabled): return False
        if force_worker != (_thread_mode == ManagedThread.WORKER): return False
        time_due = monotonic() - _last_collection >= _interval_ms / 1000
        if _algorithm == ManagedAlgorithm.MANUAL: return False
        threshold_due = False
        if _algorithm in (ManagedAlgorithm.THRESHOLD, ManagedAlgorithm.HYBRID):
            threshold_due = (clm_depend_pending_count() + clm_container_pending_count() +
                             clm_storage_pending_count()) >= _threshold
        due = ((time_due if _algorithm in (ManagedAlgorithm.INTERVAL, ManagedAlgorithm.HYBRID) else False)
               or threshold_due)
        if due:
            clm_managed_collect()
        return due


def clm_managed_get_stats() -> ManagedStats:
    with _lock:
        return _stats


def clm_managed_is_enabled() -> bool:
    with _lock:
        return _enabled


def clm_managed_destroy() -> None:
    global _initialized, _enabled, _algorithm, _thread_mode, _interval_ms, _threshold, _stats, _last_collection
    clm_managed_disable() if _initialized else None
    with _lock:
        _initialized = False
        _enabled = False
        _algorithm = ManagedAlgorithm.MANUAL
        _thread_mode = ManagedThread.CALLER
        _interval_ms = 5000
        _threshold = 10
        _last_collection = 0.0
        _stats = ManagedStats()


class Managed:
    initialize = staticmethod(clm_managed_initialize)
    enable = staticmethod(clm_managed_enable)
    disable = staticmethod(clm_managed_disable)
    set_algorithm = staticmethod(clm_managed_set_algorithm)
    set_interval = staticmethod(clm_managed_set_interval)
    set_thread = staticmethod(clm_managed_set_thread)
    set_threshold = staticmethod(clm_managed_set_threshold)
    collect = staticmethod(clm_managed_collect)
    poll = staticmethod(clm_managed_poll)
    get_stats = staticmethod(clm_managed_get_stats)
    is_enabled = staticmethod(clm_managed_is_enabled)
    destroy = staticmethod(clm_managed_destroy)
