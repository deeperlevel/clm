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

"""ID-based entity/component composition and system queries using clm_depend."""
from dataclasses import dataclass
from enum import IntEnum
from threading import RLock
from typing import Any, Callable

from clm_depend import (
    DependError, DependLifetime, DependStatus, clm_depend_register,
    clm_depend_resolve, clm_depend_get, clm_depend_get_info,
    clm_depend_reference, clm_depend_unreference, clm_depend_dispose,
    clm_depend_attach, clm_depend_detach,
)


class ECSStatus(IntEnum):
    SUCCESS = 0
    INVALID_ARGUMENT = 1
    ALREADY_REGISTERED = 2
    NOT_REGISTERED = 3
    NOT_ENTITY = 4
    NOT_COMPONENT = 5
    ENTITY_IN_USE = 6


class ECSError(RuntimeError):
    def __init__(self, status: ECSStatus, identifier: int):
        self.status = status
        self.identifier = identifier
        super().__init__(f'ECS {status.name}: ID {identifier}')


@dataclass(frozen=True)
class ECSSystemInfo:
    id: int
    components: tuple[int, ...]


_lock = RLock()
_components: set[int] = set()
_entities: dict[int, tuple[int, ...]] = {}
_instances: set[int] = set()
_systems: dict[int, tuple[tuple[int, ...], Callable[[int, dict[int, int]], Any]]] = {}


def _valid_id(value):
    return type(value) is int and value >= 0


def _valid_component_ids(values):
    return (isinstance(values, (list, tuple)) and
            all(_valid_id(x) for x in values) and len(values) == len(set(values)))


def clm_ecs_register_component(component_id: int, constructor: Callable, *, destructor=None) -> ECSStatus:
    """Register a transient component factory; constructor receives [] of dependency IDs."""
    with _lock:
        if not _valid_id(component_id) or not callable(constructor):
            return ECSStatus.INVALID_ARGUMENT
        if component_id in _components or component_id in _entities:
            return ECSStatus.ALREADY_REGISTERED
        status = clm_depend_register(component_id, [], constructor,
                                     DependLifetime.TRANSIENT, destructor=destructor)
        if status == DependStatus.SUCCESS:
            _components.add(component_id)
            return ECSStatus.SUCCESS
        if status == DependStatus.ALREADY_REGISTERED:
            return ECSStatus.ALREADY_REGISTERED
        return ECSStatus.INVALID_ARGUMENT


def clm_ecs_register_entity(entity_id: int, components: list[int], constructor: Callable,
                            *, destructor=None) -> ECSStatus:
    """Register transient entity blueprint and its ordered component dependencies."""
    with _lock:
        if not _valid_id(entity_id) or not _valid_component_ids(components) or not callable(constructor):
            return ECSStatus.INVALID_ARGUMENT
        if entity_id in _entities or entity_id in _components:
            return ECSStatus.ALREADY_REGISTERED
        if any(cid not in _components for cid in components):
            return ECSStatus.NOT_COMPONENT
        status = clm_depend_register(entity_id, components, constructor,
                                     DependLifetime.TRANSIENT, destructor=destructor)
        if status == DependStatus.SUCCESS:
            _entities[entity_id] = tuple(components)
            return ECSStatus.SUCCESS
        if status == DependStatus.ALREADY_REGISTERED:
            return ECSStatus.ALREADY_REGISTERED
        return ECSStatus.INVALID_ARGUMENT


def clm_ecs_create_entity(entity_id: int) -> int:
    """Create entity plus component instances; return the entity instance ID."""
    with _lock:
        if entity_id not in _entities:
            raise ECSError(ECSStatus.NOT_ENTITY, entity_id)
        instance_id = clm_depend_resolve(entity_id)
        _instances.add(instance_id)
        return instance_id


def clm_ecs_get_entity(instance_id: int) -> Any:
    """Return entity instance value; raises when not a registered ECS entity."""
    with _lock:
        _require_entity_instance(instance_id)
        return clm_depend_get(instance_id)


def _require_entity_instance(instance_id):
    if instance_id not in _instances:
        raise ECSError(ECSStatus.NOT_ENTITY, instance_id)
    try:
        info = clm_depend_get_info(instance_id)
    except DependError as exc:
        _instances.discard(instance_id)
        raise ECSError(ECSStatus.NOT_ENTITY, instance_id) from exc
    if info.service_id not in _entities:
        raise ECSError(ECSStatus.NOT_ENTITY, instance_id)
    return info


def clm_ecs_get_components(entity_instance_id: int) -> dict[int, int]:
    """Map component registration IDs to the entity's component instance IDs."""
    with _lock:
        info = _require_entity_instance(entity_instance_id)
        return {clm_depend_get_info(iid).service_id: iid for iid in info.dependencies}


def clm_ecs_attach_component(entity_instance_id: int, component_id: int) -> int:
    """Attach a newly created component to an existing entity; return instance ID."""
    with _lock:
        _require_entity_instance(entity_instance_id)
        if component_id not in _components:
            raise ECSError(ECSStatus.NOT_COMPONENT, component_id)
        if component_id in clm_ecs_get_components(entity_instance_id):
            raise ECSError(ECSStatus.ALREADY_REGISTERED, component_id)
        return clm_depend_attach(entity_instance_id, component_id)


def clm_ecs_remove_component(entity_instance_id: int, component_id: int) -> int:
    """Detach one component; it remains allocated until Depend's next disposal."""
    with _lock:
        _require_entity_instance(entity_instance_id)
        if component_id not in clm_ecs_get_components(entity_instance_id):
            raise ECSError(ECSStatus.NOT_COMPONENT, component_id)
        return clm_depend_detach(entity_instance_id, component_id)


def clm_ecs_get_component(component_instance_id: int) -> Any:
    """Retrieve a component value by instance ID, without acquiring a reference."""
    with _lock:
        info = clm_depend_get_info(component_instance_id)
        if info.service_id not in _components:
            raise ECSError(ECSStatus.NOT_COMPONENT, component_instance_id)
        return clm_depend_get(component_instance_id)


def clm_ecs_reference(instance_id: int):
    return clm_depend_reference(instance_id)


def clm_ecs_unreference(instance_id: int):
    return clm_depend_unreference(instance_id)


def clm_ecs_dispose() -> int:
    """Run Depend's transient sweep and prune stale ECS entity-instance IDs."""
    with _lock:
        count = clm_depend_dispose()
        for iid in tuple(_instances):
            try:
                clm_depend_get_info(iid)
            except DependError as exc:
                if exc.status == DependStatus.INSTANCE_NOT_FOUND:
                    _instances.discard(iid)
                else:
                    raise
        return count


def clm_ecs_register_system(system_id: int, components: list[int], callback: Callable) -> ECSStatus:
    """Register an explicit-query system; callback(entity_id, component_id_map)."""
    with _lock:
        if not _valid_id(system_id) or not _valid_component_ids(components) or not callable(callback):
            return ECSStatus.INVALID_ARGUMENT
        if system_id in _systems:
            return ECSStatus.ALREADY_REGISTERED
        if any(cid not in _components for cid in components):
            return ECSStatus.NOT_COMPONENT
        _systems[system_id] = (tuple(components), callback)
        return ECSStatus.SUCCESS


def clm_ecs_query(components: list[int]) -> list[tuple[int, dict[int, int]]]:
    """Return stable snapshot of live matching entities and requested component IDs.

    This is a linear query, not an indexed or archetype-optimized ECS query.
    """
    with _lock:
        if not _valid_component_ids(components) or any(cid not in _components for cid in components):
            raise ECSError(ECSStatus.INVALID_ARGUMENT, -1)
        result = []
        for iid in sorted(tuple(_instances)):
            try:
                info = _require_entity_instance(iid)
            except ECSError:
                continue
            if info.references == 0:
                continue  # Released instances are pending cleanup, not active entities.
            mapping = {clm_depend_get_info(did).service_id: did for did in info.dependencies}
            if all(cid in mapping for cid in components):
                result.append((iid, {cid: mapping[cid] for cid in components}))
        return result


def clm_ecs_run_system(system_id: int) -> int:
    """Invoke a system for matching entities, returning number processed."""
    with _lock:
        if system_id not in _systems:
            raise ECSError(ECSStatus.NOT_REGISTERED, system_id)
        components, callback = _systems[system_id]
        matches = clm_ecs_query(list(components))
        # Pin entities during callback to prevent explicit disposal in the same call.
        for iid, mapping in matches:
            clm_depend_reference(iid)
    processed = 0
    try:
        for iid, mapping in matches:
            callback(iid, mapping)
            processed += 1
    finally:
        with _lock:
            for iid, _ in matches:
                clm_depend_unreference(iid)
    return processed


def clm_ecs_get_system_info(system_id: int) -> ECSSystemInfo:
    with _lock:
        if system_id not in _systems:
            raise ECSError(ECSStatus.NOT_REGISTERED, system_id)
        return ECSSystemInfo(system_id, _systems[system_id][0])


def clm_ecs_reset() -> None:
    """Clear ECS metadata only; DOES NOT destroy Depend registrations or instances.

    Call after Depend shutdown, or only when application guarantees no ECS objects remain.
    """
    with _lock:
        _components.clear()
        _entities.clear()
        _instances.clear()
        _systems.clear()


class ECS:
    attach_component = staticmethod(clm_ecs_attach_component)
    remove_component = staticmethod(clm_ecs_remove_component)
    register_component = staticmethod(clm_ecs_register_component)
    register_entity = staticmethod(clm_ecs_register_entity)
    create_entity = staticmethod(clm_ecs_create_entity)
    get_entity = staticmethod(clm_ecs_get_entity)
    get_components = staticmethod(clm_ecs_get_components)
    get_component = staticmethod(clm_ecs_get_component)
    reference = staticmethod(clm_ecs_reference)
    unreference = staticmethod(clm_ecs_unreference)
    dispose = staticmethod(clm_ecs_dispose)
    register_system = staticmethod(clm_ecs_register_system)
    query = staticmethod(clm_ecs_query)
    run_system = staticmethod(clm_ecs_run_system)
    get_system_info = staticmethod(clm_ecs_get_system_info)
    reset = staticmethod(clm_ecs_reset)
