# CLM ECS — Language-Agnostic Specification

**One-line description:** ID-based entity and component composition with system queries built on CLM Depend's tracked transient factories.

**Project:** CLM — Cross Language Modules · Deeperlevel · CC0 1.0 Universal  
**Motto:** Don't start from scratch, bro.

## Purpose and boundaries

`clm_ecs` is a **semantic ECS façade over `clm_depend`**, not a separate dependency injector, object allocator, or archetype engine. All components and entities are registered as **TRANSIENT** factories, and every created entity gets an independently instantiated set of registered component dependencies. The module knows which registrations are components, which are entities, and which entity instances it created. It offers straightforward component-set queries and systems; it does **not** promise optimized archetype queries, parallel scheduling, runtime component-template evolution, or dynamic attachment/removal in this initial version.

Dependency chain: **clm_ecs → clm_depend → clm_container**. Call `clm_depend_initialize()` before registering. `clm_ecs` does not initialize/destroy Depend implicitly.

## Registration and instance IDs

- **Component registration ID:** names a transient component constructor, *not* a component instance.
- **Entity registration ID:** names an entity blueprint, whose ordered dependencies are component registration IDs.
- **Instance ID:** unique identifier allocated by Depend upon each transient resolution; entity and component instance IDs share Depend's instance-ID namespace.
- **System ID:** identifies a registered ECS system in ECS's private system registry; its ID namespace is logically independent from Depend's services.
- The same service ID must not be registered as both an ECS component and entity. ECS component IDs must already be registered when registering an entity.
- Application constructors use **one ordered list of dependency instance IDs** and return an application-defined value. Component factories in this version receive an empty dependency list.

## Procedural API

| Operation | Contract |
| --- | --- |
| `clm_ecs_register_component(component_id, constructor, destructor=None)` | Registers a transient component via Depend; returns ECSStatus. Python expresses `destructor` as keyword-only. |
| `clm_ecs_register_entity(entity_id, components, constructor, destructor=None)` | Registers a transient entity factory with ordered component dependencies. Duplicate dependencies rejected. Returns ECSStatus. |
| `clm_ecs_create_entity(entity_id)` | Resolves and tracks a new entity; returns its Depend instance ID. Raises ECSError for unknown entity registration. |
| `clm_ecs_get_entity(instance_id)` | Returns constructed entity value without incrementing references. |
| `clm_ecs_get_components(entity_instance_id)` | Returns `{component_registration_id: component_instance_id}` for the entity's registered dependencies. |
| `clm_ecs_get_component(instance_id)` | Returns a component instance value without incrementing references. |
| `clm_ecs_reference(instance_id)` | Increase Depend reference count; caller manages ownership explicitly. |
| `clm_ecs_unreference(instance_id)` | Decrease reference count; does not immediately destroy. |
| `clm_ecs_dispose()` | Calls `clm_depend_dispose()` and prunes stale ECS handles; returns Depend's total disposed transient count. |
| `clm_ecs_register_system(system_id, components, callback)` | Registers a system with required component registration IDs. |
| `clm_ecs_query(components)` | Returns snapshot list of `(entity_instance_id, component_id_mapping)` for live entities containing all requested components. |
| `clm_ecs_run_system(system_id)` | Invokes callback `(entity_instance_id, requested_component_mapping)` for matching entities; returns number processed. |
| `clm_ecs_get_system_info(system_id)` | Returns ID and required component IDs. |
| `clm_ecs_reset()` | Clears ECS bookkeeping **only**. Does not unregister or dispose anything in Depend. Intended for tests/shutdown after Depend teardown. |

**Status codes (Python):** `SUCCESS=0`, `INVALID_ARGUMENT=1`, `ALREADY_REGISTERED=2`, `NOT_REGISTERED=3`, `NOT_ENTITY=4`, `NOT_COMPONENT=5`, `ENTITY_IN_USE=6` (reserved). Follow status/exception distinction in portable implementations: registration returns status; missing/invalid instance access raises `ECSError`, or returns an equivalent error status in languages without exceptions.

## Example (language-neutral pseudocode)

```text
clm_depend_initialize()
clm_ecs_register_component(POSITION, position_factory)
clm_ecs_register_component(HEALTH, health_factory)
clm_ecs_register_entity(SHIP, [POSITION, HEALTH], ship_factory)

ship_id = clm_ecs_create_entity(SHIP)
components = clm_ecs_get_components(ship_id)
position = clm_ecs_get_component(components[POSITION])

clm_ecs_register_system(MOVE_SYSTEM, [POSITION], movement_callback)
clm_ecs_run_system(MOVE_SYSTEM)

clm_ecs_unreference(ship_id)
clm_ecs_dispose()
clm_depend_destroy()
clm_ecs_reset()
```

Each resolve builds a **new** ship and Position/Health instances. The ship constructor receives Position/Health *instance IDs* in the specified order. Depend tracks these owned component dependencies; disposing an unreferenced ship releases its dependencies. If an outside owner needs to retain a component, it must explicitly `reference(component_instance_id)` first. Depend's disposal sweep may reclaim the components in the same cycle if their references reach zero.

## Systems and queries

This initial ECS queries **only entities created through `clm_ecs_create_entity()`**. A direct `clm_depend_resolve(ENTITY_SERVICE)` is not automatically registered in the ECS live-entity set. Query is a linear scan of tracked entity instances, not an indexed/archetype query. It skips zero-reference entities awaiting disposal and prunes externally disposed instances. Query results contain only the *requested* component IDs. `run_system` temporarily references matched entities for the duration of execution, then releases those references even if a callback fails.

The system callback executes outside ECS's registry lock. The caller must synchronize externally if it concurrently mutates the Depend registry or ECS scene. System ordering, delta-time passing, scheduling and ECS-specific component mutations are game-level responsibilities.

## Ownership, disposal and reset

- Component instance ownership is primarily held by the entity via Depend's dependency tracking; no language-scoping inference.
- `get_entity`, `get_component` and `get_components` are *borrowed lookups*, not ownership grants.
- `unreference()` is deferred; actual cleanup is performed through Depend disposal or `clm_managed`.
- Singletons and scoped services managed by Depend are unaffected by transient disposal; the ECS's registered entity/component factories are transient.
- `clm_ecs_reset()` cannot safely run mid-game: it discards ECS knowledge of live instances and registrations, but Depend still owns them. Prefer shutting down Depend first.
- ECS registration uses Depend's global registration registry. If another subsystem unregisters or replaces a service, the ECS registry must be reset/rebuilt as appropriate.

## Not yet supported

Runtime component add/remove, hierarchical entity relationships, parent-owned *non-factory* components, archetype packing, query indexes, scene/world isolation, multi-scope parallelism, automatic component field schemas, persistence/networking. These are explicitly **not** part of this first implementation.

## Portability

The conceptual API uses numeric IDs, ordered dependency ID arrays, constructor callback + optional destructor callback, and explicit reference operations. Python dictionaries/sets are only implementation details; C may use CLM Container/Storage. No Python-specific reflection, dataclasses, inheritance, or GC is required by the cross-language design. Cross-language implementations should maintain identical membership and lifecycle semantics.


## Dynamic entity composition (extension)

`clm_ecs_attach_component(entity_instance_id, component_registration_id)`
creates one transient component through Depend, attaches it to the parent
entity's tracked dependency list, and returns the new component instance ID.
Existing component types cannot be added twice to the same entity.

`clm_ecs_remove_component(entity_instance_id, component_registration_id)`
detaches the matched component and releases the entity-owned reference; this
**does not immediately destroy it**. Only Depend disposal reclaims eligible
transients. External owners must use manual `reference`/`unreference`.

`clm_ecs_get_components()` and `clm_ecs_query()` read current component IDs
from Depend instance metadata, so newly attached/removed components are reflected
in queries and system matching without rebuilding the entity. Constructor-returned
custom objects are not magically rewritten when composition changes.

Queries remain linear and return snapshots. Optimized archetypes/indexing and
a scheduled execution graph are **not implemented**. Since the ECS facade is
process-wide and the underlying Depend registry has one scope, it does not yet
provide fully isolated concurrent game worlds.
