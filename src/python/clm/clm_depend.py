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

"""Procedural ID-based factories with tracked instances and deferred transient disposal."""
from dataclasses import dataclass, field
from enum import IntEnum
from threading import RLock
from typing import Any, Callable
import inspect

from clm_container import (CONTAINER_SUCCESS, CONTAINER_TYPE_OBJECT,
    clm_container_create, clm_container_destroy, clm_container_clear,
    clm_container_add_object, clm_container_get_object, clm_container_delete_object)

DEPEND_CONTAINER_ID = 0xFF01
DEPEND_SCOPED_CONTAINER_ID = 0xFF02
DEPEND_INSTANCE_CONTAINER_ID = 0xFF03

class DependStatus(IntEnum):
    SUCCESS=0; ALREADY_REGISTERED=1; NOT_REGISTERED=2; SEALED=3
    INVALID_ARGUMENT=4; MISSING_DEPENDENCY=5; INCOMPATIBLE_LIFETIME=6
    CIRCULAR_DEPENDENCY=7; SCOPE_ALREADY_ACTIVE=8; SCOPE_NOT_ACTIVE=9
    SCOPE_REQUIRED=10; CONSTRUCTOR_LOCKED=11
    INSTANCE_NOT_FOUND=12; REFERENCE_UNDERFLOW=13; INSTANCE_IN_USE=14

class DependLifetime(IntEnum):
    EAGER=0x01; SINGLETON=0x01; LAZY=0x02; SCOPED=0x03; TRANSIENT=0x04

DependConstructor = Callable[[list[int]], Any]
DependDestructor = Callable[[Any], None]

class DependError(RuntimeError):
    def __init__(self, status: DependStatus, service_id: int, message: str = ''):
        self.status=status; self.service_id=service_id
        super().__init__(message or f'Depend {status.name}: ID {service_id}')

@dataclass
class DependItem:
    id: int
    dependencies: tuple[int, ...]
    constructor: DependConstructor
    lifetime: DependLifetime
    use_for: int=0
    destructor: DependDestructor | None=None
    instance_id: int | None=None
    created: bool=False

@dataclass
class DependInstanceInfo:
    id: int
    service_id: int
    lifetime: DependLifetime
    references: int
    dependencies: tuple[int, ...]

@dataclass
class _Instance:
    id: int
    service_id: int
    lifetime: DependLifetime
    value: Any
    dependencies: tuple[int, ...]
    destructor: DependDestructor | None
    references: int=1

class DependStates:
    container_id=DEPEND_CONTAINER_ID
    scoped_container_id=DEPEND_SCOPED_CONTAINER_ID
    instance_container_id=DEPEND_INSTANCE_CONTAINER_ID
    initialized=False; sealed=False; scope_active=False
    constructing: set[int]=set()
    next_instance_id=0x100000000
    lock=RLock()

def _valid_id(x): return type(x) is int and x >= 0

def _require():
    if not DependStates.initialized: raise RuntimeError('Call clm_depend_initialize() first')

def _item(sid):
    status, obj=clm_container_get_object(DEPEND_CONTAINER_ID, sid)
    if status != CONTAINER_SUCCESS: raise DependError(DependStatus.NOT_REGISTERED, sid)
    return obj

def _instance(iid):
    status, obj=clm_container_get_object(DEPEND_INSTANCE_CONTAINER_ID, iid)
    if status != CONTAINER_SUCCESS: raise DependError(DependStatus.INSTANCE_NOT_FOUND, iid)
    return obj

def _reference(iid):
    inst=_instance(iid); inst.references+=1

def _unreference(iid):
    inst=_instance(iid)
    if inst.references == 0: raise DependError(DependStatus.REFERENCE_UNDERFLOW, iid)
    inst.references-=1

def _cleanup_failed(dependencies):
    for iid in reversed(dependencies):
        _unreference(iid)

def _resolve(sid, parent=None):
    item=_item(sid)
    if parent in (DependLifetime.EAGER, DependLifetime.LAZY) and item.lifetime == DependLifetime.SCOPED:
        raise DependError(DependStatus.INCOMPATIBLE_LIFETIME, sid)
    if item.lifetime == DependLifetime.SCOPED and not DependStates.scope_active:
        raise DependError(DependStatus.SCOPE_REQUIRED, sid)
    if item.lifetime != DependLifetime.TRANSIENT and item.instance_id is not None:
        return item.instance_id
    if sid in DependStates.constructing:
        raise DependError(DependStatus.CIRCULAR_DEPENDENCY, sid)
    DependStates.constructing.add(sid)
    deps=[]
    try:
        for dep in item.dependencies:
            try: did=_resolve(dep, item.lifetime)
            except DependError as e:
                if e.status == DependStatus.NOT_REGISTERED:
                    raise DependError(DependStatus.MISSING_DEPENDENCY, dep) from e
                raise
            # Each dependency resolution obtains one ownership reference.
            # New transients already start with one; cached instances need an extra one.
            if _instance(did).lifetime != DependLifetime.TRANSIENT:
                _reference(did)
            deps.append(did)
        result=item.constructor(list(deps))
        iid=DependStates.next_instance_id
        DependStates.next_instance_id+=1  # Never reuse instance IDs during this process.
        inst=_Instance(iid,sid,item.lifetime,result,tuple(deps),item.destructor)
        if clm_container_add_object(DEPEND_INSTANCE_CONTAINER_ID,iid,inst) != CONTAINER_SUCCESS:
            raise RuntimeError('Cannot track Depend instance')
        if item.lifetime != DependLifetime.TRANSIENT:
            item.instance_id=iid; item.created=True
            if item.lifetime == DependLifetime.SCOPED:
                code=clm_container_add_object(DEPEND_SCOPED_CONTAINER_ID,sid,iid)
                if code != CONTAINER_SUCCESS:
                    item.instance_id=None; item.created=False
                    clm_container_delete_object(DEPEND_INSTANCE_CONTAINER_ID,iid)
                    raise RuntimeError('Cannot cache scope instance')
        return iid
    except BaseException:
        _cleanup_failed(deps)
        raise
    finally:
        DependStates.constructing.remove(sid)

def clm_depend_initialize():
    with DependStates.lock:
        if DependStates.initialized: return
        created=[]
        for cid in (DEPEND_CONTAINER_ID,DEPEND_SCOPED_CONTAINER_ID,DEPEND_INSTANCE_CONTAINER_ID):
            if clm_container_create(cid,CONTAINER_TYPE_OBJECT) != CONTAINER_SUCCESS:
                for prev in reversed(created): clm_container_destroy(prev)
                raise RuntimeError(f'Reserved Depend container {cid} is already in use')
            created.append(cid)
        DependStates.initialized=True; DependStates.sealed=False; DependStates.scope_active=False

def clm_depend_register(service_id, dependencies, constructor, lifetime=DependLifetime.EAGER, use_for=0, *, destructor=None):
    with DependStates.lock:
        _require()
        if DependStates.sealed: return DependStatus.SEALED
        if (not _valid_id(service_id) or not _valid_id(use_for) or
            not isinstance(dependencies,(tuple,list)) or any(not _valid_id(d) for d in dependencies) or
            not callable(constructor) or (destructor is not None and not callable(destructor))):
            return DependStatus.INVALID_ARGUMENT
        try: lifetime=DependLifetime(lifetime)
        except (ValueError,TypeError): return DependStatus.INVALID_ARGUMENT
        try: _item(service_id)
        except DependError as exc:
            if exc.status != DependStatus.NOT_REGISTERED: raise
        else: return DependStatus.ALREADY_REGISTERED
        item=DependItem(service_id,tuple(dependencies),constructor,lifetime,use_for,destructor)
        if clm_container_add_object(DEPEND_CONTAINER_ID,service_id,item)!=CONTAINER_SUCCESS:
            raise RuntimeError('Cannot register service')
        if lifetime == DependLifetime.EAGER:
            try: _resolve(service_id)
            except DependError as exc:
                clm_container_delete_object(DEPEND_CONTAINER_ID,service_id)
                return exc.status
            except BaseException:
                clm_container_delete_object(DEPEND_CONTAINER_ID,service_id)
                raise
        return DependStatus.SUCCESS

def clm_depend_resolve(service_id):
    with DependStates.lock:
        _require()
        if not _valid_id(service_id): raise DependError(DependStatus.INVALID_ARGUMENT,service_id)
        return _resolve(service_id)

def clm_depend_get(instance_id):
    with DependStates.lock:
        _require(); return _instance(instance_id).value

def clm_depend_get_info(instance_id):
    with DependStates.lock:
        _require(); a=_instance(instance_id)
        return DependInstanceInfo(a.id,a.service_id,a.lifetime,a.references,a.dependencies)

def clm_depend_reference(instance_id):
    with DependStates.lock:
        _require(); _reference(instance_id); return DependStatus.SUCCESS

def clm_depend_unreference(instance_id):
    with DependStates.lock:
        _require(); _unreference(instance_id); return DependStatus.SUCCESS

def clm_depend_attach(instance_id, service_id):
    """Attach a new transient dependency instance to an existing transient parent.

    The parent takes the returned child reference; disposal releases it.
    """
    with DependStates.lock:
        _require()
        parent = _instance(instance_id)
        if parent.lifetime != DependLifetime.TRANSIENT or parent.references == 0:
            raise DependError(DependStatus.INSTANCE_IN_USE, instance_id)
        if any(_instance(i).service_id == service_id for i in parent.dependencies):
            raise DependError(DependStatus.ALREADY_REGISTERED, service_id)
        child = _item(service_id)
        if child.lifetime != DependLifetime.TRANSIENT:
            raise DependError(DependStatus.INCOMPATIBLE_LIFETIME, service_id)
        iid = _resolve(service_id, parent.lifetime)
        parent.dependencies = (*parent.dependencies, iid)
        return iid


def clm_depend_detach(instance_id, service_id):
    """Detach dependency by service ID, releasing the parent's owned reference."""
    with DependStates.lock:
        _require()
        parent = _instance(instance_id)
        if parent.lifetime != DependLifetime.TRANSIENT or parent.references == 0:
            raise DependError(DependStatus.INSTANCE_IN_USE, instance_id)
        matches = [i for i in parent.dependencies if _instance(i).service_id == service_id]
        if not matches: raise DependError(DependStatus.MISSING_DEPENDENCY, service_id)
        iid = matches[0]
        parent.dependencies = tuple(i for i in parent.dependencies if i != iid)
        _unreference(iid)
        return iid


def clm_depend_pending_count():
    """Number of zero-reference transient instances awaiting disposal."""
    with DependStates.lock:
        _require()
        from clm_container import clm_container_first, clm_container_next
        count = 0
        status, key = clm_container_first(DEPEND_INSTANCE_CONTAINER_ID)
        while status == CONTAINER_SUCCESS:
            inst = _instance(key)
            count += inst.lifetime == DependLifetime.TRANSIENT and inst.references == 0
            status, key = clm_container_next(DEPEND_INSTANCE_CONTAINER_ID)
        return count


def _dispose_one(iid, force=False):
    inst=_instance(iid)
    if not force and (inst.lifetime != DependLifetime.TRANSIENT or inst.references != 0):
        return False
    # Destructor failure leaves record and dependency references intact for retry.
    if inst.destructor is not None: inst.destructor(inst.value)
    clm_container_delete_object(DEPEND_INSTANCE_CONTAINER_ID,iid)
    for did in reversed(inst.dependencies): _unreference(did)
    return True

def clm_depend_dispose():
    """Sweep unreferenced TRANSIENT instances; repeat to reclaim their dependencies."""
    with DependStates.lock:
        _require(); total=0
        from clm_container import clm_container_first, clm_container_next
        while True:
            # Snapshot instances: constructors/destructors must not invalidate cursor.
            from clm_container import clm_container_get_object as read
            from clm_container import clm_container_get_info
            _, meta=clm_container_get_info(DEPEND_INSTANCE_CONTAINER_ID)
            keys=[]
            if meta.committed:
                status,key=clm_container_first(DEPEND_INSTANCE_CONTAINER_ID)
                while status == CONTAINER_SUCCESS:
                    keys.append(key)
                    status,key=clm_container_next(DEPEND_INSTANCE_CONTAINER_ID)
            eligible=[iid for iid in keys if (lambda a: a.lifetime==DependLifetime.TRANSIENT and a.references==0)(_instance(iid))]
            if not eligible: break
            for iid in eligible:
                if _dispose_one(iid): total+=1
        return total

def clm_depend_construct(service_id, constructor):
    with DependStates.lock:
        _require()
        if DependStates.sealed: raise DependError(DependStatus.SEALED,service_id)
        if not callable(constructor): raise DependError(DependStatus.INVALID_ARGUMENT,service_id)
        item=_item(service_id)
        if item.created or service_id in DependStates.constructing:
            raise DependError(DependStatus.CONSTRUCTOR_LOCKED,service_id)
        item.constructor=constructor

def clm_depend_unregister(service_id):
    with DependStates.lock:
        _require()
        if DependStates.sealed: raise DependError(DependStatus.SEALED,service_id)
        item=_item(service_id)
        if service_id in DependStates.constructing: raise DependError(DependStatus.CONSTRUCTOR_LOCKED,service_id)
        # Active instances may outlive registrations; each instance retains its destructor.
        if item.instance_id is not None:
            inst=_instance(item.instance_id)
            if inst.references > 1: raise DependError(DependStatus.INSTANCE_IN_USE,service_id)
            _dispose_one(item.instance_id,force=True)
        clm_container_delete_object(DEPEND_SCOPED_CONTAINER_ID,service_id)
        clm_container_delete_object(DEPEND_CONTAINER_ID,service_id)

def clm_depend_begin_scope():
    with DependStates.lock:
        _require()
        if DependStates.scope_active: raise DependError(DependStatus.SCOPE_ALREADY_ACTIVE,-1)
        DependStates.scope_active=True

def clm_depend_end_scope():
    with DependStates.lock:
        _require()
        if not DependStates.scope_active: raise DependError(DependStatus.SCOPE_NOT_ACTIVE,-1)
        from clm_container import clm_container_first,clm_container_next
        ids=[]; status,key=clm_container_first(DEPEND_SCOPED_CONTAINER_ID)
        while status == CONTAINER_SUCCESS:
            _,iid=clm_container_get_object(DEPEND_SCOPED_CONTAINER_ID,key)
            ids.append((key,iid)); status,key=clm_container_next(DEPEND_SCOPED_CONTAINER_ID)
        for sid,iid in ids:
            if _instance(iid).references>1: raise DependError(DependStatus.INSTANCE_IN_USE,sid)
        for sid,iid in reversed(ids):
            _dispose_one(iid,force=True)
            try: item=_item(sid); item.instance_id=None; item.created=False
            except DependError: pass
        clm_container_clear(DEPEND_SCOPED_CONTAINER_ID)
        DependStates.scope_active=False

def clm_depend_seal():
    with DependStates.lock:
        _require(); DependStates.sealed=True

def clm_depend_invoke(services,func,*args):
    with DependStates.lock:
        _require()
        if not callable(func) or not isinstance(services,(tuple,list)):
            raise DependError(DependStatus.INVALID_ARGUMENT,-1)
        ids=[]
        try:
            for sid in services:
                iid=_resolve(sid)
                if _instance(iid).lifetime != DependLifetime.TRANSIENT: _reference(iid)
                ids.append(iid)
        except BaseException:
            for iid in ids: _unreference(iid)
            raise
    try: return func(*ids,*args)
    finally:
        with DependStates.lock:
            for iid in ids: _unreference(iid)

async def clm_depend_invoke_async(services,func,*args):
    if not inspect.iscoroutinefunction(func): raise DependError(DependStatus.INVALID_ARGUMENT,-1)
    with DependStates.lock:
        _require()
        if not isinstance(services,(list,tuple)): raise DependError(DependStatus.INVALID_ARGUMENT,-1)
        ids=[]
        try:
            for sid in services:
                iid=_resolve(sid)
                if _instance(iid).lifetime != DependLifetime.TRANSIENT: _reference(iid)
                ids.append(iid)
        except BaseException:
            for iid in ids: _unreference(iid)
            raise
    try: return await func(*ids,*args)
    finally:
        with DependStates.lock:
            for iid in ids: _unreference(iid)

def clm_depend_destroy():
    with DependStates.lock:
        if not DependStates.initialized: return
        if DependStates.constructing: raise RuntimeError('Cannot destroy while constructing')
        # Explicit shutdown is distinct from routine transient-only collection.
        from clm_container import clm_container_first,clm_container_next
        ids=[]; status,key=clm_container_first(DEPEND_INSTANCE_CONTAINER_ID)
        while status == CONTAINER_SUCCESS:
            ids.append(key); status,key=clm_container_next(DEPEND_INSTANCE_CONTAINER_ID)
        # Force disposal from dependents toward their dependencies.
        pending=set(ids)
        while pending:
            owned={d for iid in pending for d in _instance(iid).dependencies if d in pending}
            candidates=pending-owned
            iid=min(candidates or pending)
            _dispose_one(iid,force=True)
            pending.remove(iid)
        for cid in (DEPEND_INSTANCE_CONTAINER_ID,DEPEND_SCOPED_CONTAINER_ID,DEPEND_CONTAINER_ID):
            clm_container_destroy(cid)
        DependStates.initialized=False; DependStates.scope_active=False; DependStates.sealed=False
        DependStates.constructing.clear()

class Depend:
    attach = staticmethod(clm_depend_attach)
    detach = staticmethod(clm_depend_detach)
    pending_count = staticmethod(clm_depend_pending_count)
    initialize=staticmethod(clm_depend_initialize)
    register=staticmethod(clm_depend_register)
    resolve=staticmethod(clm_depend_resolve)
    get=staticmethod(clm_depend_get)
    get_info=staticmethod(clm_depend_get_info)
    reference=staticmethod(clm_depend_reference)
    unreference=staticmethod(clm_depend_unreference)
    dispose=staticmethod(clm_depend_dispose)
    construct=staticmethod(clm_depend_construct)
    unregister=staticmethod(clm_depend_unregister)
    invoke=staticmethod(clm_depend_invoke)
    invoke_async=staticmethod(clm_depend_invoke_async)
    begin_scope=staticmethod(clm_depend_begin_scope)
    end_scope=staticmethod(clm_depend_end_scope)
    seal=staticmethod(clm_depend_seal)
    destroy=staticmethod(clm_depend_destroy)
