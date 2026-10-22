# CLM Depend

**Cross Language Modules · Language-agnostic module reference**

CLM is an initiative by **Deeperlevel**, developed with community contributions.  
Official repository: https://github.com/deeperlevel/clm  
Contact: deeparchive@outlook.com  
License: [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/) — subject to the rights applicable to each contribution.

> **Different languages. Shared technologies. No borders.**  
> *Don't start from scratch, bro.*

## Overview

`clm_depend` is a **procedural, ID-based dependency registration and resolution module**. It lets applications register a service under a numeric identifier, describe other services needed to construct it, and select how often it is constructed or reused.

CLM Depend is **not an OOP framework**. The service may be an object, a structure, a resource handle, a function table, or another language-native value. A constructor is a **registered function** that receives an **ordered collection of already-resolved dependencies** and produces a value. Class constructors, reflection, inheritance, annotations, and generic type discovery are not required.

CLM Depend uses `clm_container` object containers for registration metadata and cached scoped values. Its public contract is intended to be consistent across programming languages, although parameter representation, error delivery, ownership, and concurrency mechanisms may differ.

### At a glance

```text
register(service ID, dependency IDs, constructor, lifetime)
                            |
                            v
                      resolve(service ID)
                            |
                            v
                 resolve dependencies in order
                            |
                            v
                 constructor([resolved values])
                            |
                            v
                   cache or return instance
```

## Design principles

- **Procedural first:** All core operations are standalone functions. A language may offer an optional `Depend` facade.
- **Explicit numeric IDs:** Registration and resolution use caller-assigned service IDs rather than type-name discovery.
- **Constructor functions:** Each constructor accepts one ordered dependency collection, even when empty.
- **No forced OOP:** Services can be ordinary data and function-based components.
- **Native implementation:** Each language may use appropriate native containers, function pointers, delegates, pointers, or handles.
- **Composable:** `clm_depend` relies on `clm_container`, without requiring SPIder or an object-runtime module.
- **Predictable lifetimes:** Registration determines whether service results are cached and when they are constructed.

## Terminology

| Term | Meaning |
| --- | --- |
| **Service ID** | Caller-defined, nonnegative numeric identifier of a registration. |
| **Dependency IDs** | Ordered sequence of service IDs required by a constructor. |
| **Constructor** | Callable receiving one ordered collection of resolved dependencies and returning a service value. |
| **Registration** | Stored association among ID, dependencies, constructor, and lifetime. |
| **Instance** | Value returned by the constructor, including non-OOP values. |
| **Scope** | A bounded cache for services registered with `SCOPED` lifetime. |
| **Sealed registry** | Registry that no longer accepts registration, unregistration, or constructor replacement. |

## Initialization and state

The supplied reference implementation manages **one process-local registry** and **at most one active scope**. It reserves two container identifiers:

| Constant | Value | Use |
| --- | --- | --- |
| `DEPEND_CONTAINER_ID` | `0xFF01` | Service registrations and cached persistent instances. |
| `DEPEND_SCOPED_CONTAINER_ID` | `0xFF02` | Cached instances for the currently active scope. |

Call `clm_depend_initialize()` before registering, resolving, or invoking services. Initialization is idempotent: invoking it again does not erase existing registrations. Initialization fails if the underlying reserved container IDs are already occupied.

Call `clm_depend_destroy()` to destroy the registry's containers and return the module to its uninitialized state. Destroying the registry does **not** invoke application-defined destructors or disposal hooks.

> **Reserved IDs:** If implementations expose a shared container ID space, avoid allocating these two IDs for unrelated containers.

## Lifetime constants

| Constant | Value | Construction | Reuse |
| --- | --- | --- | --- |
| `DependLifetime.EAGER` | `0x01` | During successful registration | Shared/cached |
| `DependLifetime.SINGLETON` | `0x01` | Alias of `EAGER` | Shared/cached |
| `DependLifetime.LAZY` | `0x02` | First resolution | Shared/cached after creation |
| `DependLifetime.SCOPED` | `0x03` | First resolution inside an active scope | Reused inside that scope only |
| `DependLifetime.TRANSIENT` | `0x04` | Every resolution | Not cached |

**Important:** In the current contract, `SINGLETON` is an alias of **eager** construction, not an alternative lazy singleton. To construct a shared service on its first use, select `LAZY`.

### Lifetime compatibility

An `EAGER`/`SINGLETON` or `LAZY` service **cannot depend directly or transitively on a `SCOPED` service** during resolution. Such a dependency produces `INCOMPATIBLE_LIFETIME`. A scoped service requires an active scope to resolve, and a transient service can be constructed whenever its own dependencies can be resolved.

The implementation enforces these checks at resolution or eager construction time; it does not fully prevalidate the graph at every registration.

## Registration and constructor contract

### `clm_depend_register(service_id, dependencies, constructor, lifetime = EAGER, use_for = 0)`

Register a service with a unique numeric ID.

| Parameter | Contract |
| --- | --- |
| `service_id` | Nonnegative integer; must not already be registered. |
| `dependencies` | Ordered collection of nonnegative service IDs; empty is valid. |
| `constructor` | Callable that accepts **one collection of resolved dependency values**. |
| `lifetime` | One of the defined lifetime constants; default `EAGER`. |
| `use_for` | Optional nonnegative integer metadata in the supplied reference implementation; **does not implement aliases or affect lookup**. |

The function returns a `DependStatus` code for registration outcomes. Constructor failures unrelated to `DependError` may propagate as language-native errors. For `EAGER`, construction happens immediately; if it fails, the new registration is rolled back.

#### Language-neutral pseudocode

```text
SERVICE_REPOSITORY   = 0x1001
SERVICE_APPLICATION  = 0x1002

function new_repository(dependencies):
    // dependencies is empty
    return create_repository()

function new_application(dependencies):
    repository = dependencies[0]
    return create_application(repository)

clm_depend_initialize()

clm_depend_register(
    SERVICE_REPOSITORY,
    [],
    new_repository,
    LAZY
)

clm_depend_register(
    SERVICE_APPLICATION,
    [SERVICE_REPOSITORY],
    new_application,
    LAZY
)

application = clm_depend_resolve(SERVICE_APPLICATION)
```

The dependency at position zero is the first registered dependency ID, position one is the second, and so on. **A constructor does not need to call `clm_depend_resolve` for each argument**; the module resolves them before invoking it.

A zero-dependency constructor receives an **empty collection**, not a call with zero parameters. Languages may unpack, index, or cast collection values in their native style.

### Constructor outcomes

The constructor's return value becomes the service instance. The reference implementation permits any returned value, including null-like values, but this may not be a useful valid service for every application. Callers should establish an application-level policy for constructor failures and invalid results.

Constructors should not assume every language returns an identical representation of the dependency collection (e.g., list, slice, pointer array, or vector). The portable behavior is **one collection containing resolved values in registration order**.

## API reference

Signatures below express conceptual arguments and results; they are **not tied to any particular language's type syntax**.

| Function | Operation | Result / failure behavior |
| --- | --- | --- |
| `clm_depend_initialize()` | Create registry and scoped storage, if not already initialized. | Idempotent; fails if internal container IDs cannot be reserved. |
| `clm_depend_register(service_id, dependencies, constructor, lifetime, use_for)` | Register a service; eagerly construct if configured. | Registration status; may propagate constructor failures. |
| `clm_depend_resolve(service_id)` | Resolve dependencies and return the corresponding instance. | Instance or resolution error. |
| `clm_depend_construct(service_id, constructor)` | Replace the constructor of a registered, not-yet-constructed service. | Success or error; cannot replace a locked constructor. |
| `clm_depend_unregister(service_id)` | Remove service registration and its cached references. | Success or error; does **not** dispose the instance. |
| `clm_depend_invoke(service_ids, callback, ...extra_arguments)` | Resolve dependencies and invoke a callback. | Callback result; see distinct callback contract below. |
| `clm_depend_invoke_async(service_ids, callback, ...extra_arguments)` | Invoke an asynchronous callback and await completion. | Awaited result or error. |
| `clm_depend_begin_scope()` | Activate the single supported scope. | Error if already active. |
| `clm_depend_end_scope()` | Clear cached scoped services and deactivate scope. | Error if no scope is active. |
| `clm_depend_seal()` | Prevent registry mutations. | No return value in the reference implementation. |
| `clm_depend_destroy()` | Destroy both containers; reset the module. | Idempotent; does not dispose application instances. |

### `clm_depend_resolve(service_id)`

1. Validate that the registry is initialized and the ID is valid.
2. Find the service registration by ID.
3. Check lifetime compatibility and scope requirements.
4. Return an existing cached instance when applicable.
5. Detect cyclic dependency construction.
6. Resolve registered dependencies **in order**.
7. Call the constructor with the resolved dependency collection.
8. Cache the result according to lifetime, then return it.

Missing registration IDs become `NOT_REGISTERED` when requested directly; when missing during another service's dependency resolution, the reference implementation reports `MISSING_DEPENDENCY` with the missing dependency ID.

### `clm_depend_construct(service_id, constructor)`

Replace the factory for a registered service **only before that service has been constructed**. Replacement is rejected if the registry is sealed, the service is being constructed, a persistent cached instance exists, or a scoped instance is already cached in the active scope.

This operation changes the registered constructor, **not** the lifetime or dependency list.

### `clm_depend_unregister(service_id)`

Remove a service registration. Already obtained references may remain alive depending on language-native ownership rules. Unregistration does not automatically call destructors or invalidate references held elsewhere.

### `clm_depend_invoke(...)` and `clm_depend_invoke_async(...)`

**These callbacks follow a different calling convention from constructor functions in the supplied implementation.**

- **Constructor:** receives **one collection** of resolved dependencies.
- **Invocation callback:** receives resolved dependencies as **individual positional arguments**, followed by any extra explicit arguments.

```text
// Constructor: one collection
new_application([resolved_repository])

// Invoke callback: positional values
callback(resolved_repository, request_context)
```

The current reference implementation expresses `invoke` extra arguments with Python-style variadic parameters. Languages without compatible variadics will need an equivalent API design; **a universal array-only invocation contract has not yet been established by this source**. Do not assume `invoke` currently accepts the same one-array signature as constructors.

`clm_depend_invoke_async` requires an asynchronous callback, invokes it, and awaits its result. Dependency constructors themselves are synchronous in the reference implementation.

## Scopes

`SCOPED` registrations are cached only during an active scope.

```text
clm_depend_begin_scope()

first  = clm_depend_resolve(SERVICE_SCOPED)
second = clm_depend_resolve(SERVICE_SCOPED)
// first and second refer to the same cached instance

clm_depend_end_scope()
// Scoped cache is cleared; later scope creates a new instance
```

The current implementation has **one active scope globally**, rather than a distinct scope per request, thread, task, or WebSocket connection. Calling `begin_scope` while a scope is active fails with `SCOPE_ALREADY_ACTIVE`; calling `end_scope` without a scope fails with `SCOPE_NOT_ACTIVE`.

**Concurrency warning:** A process-wide scope is not appropriate for overlapping request/connection lifetimes. Web servers must not treat this implementation as request-isolated DI. Future versions may require separately identified scopes or execution-context-local scope storage before supporting concurrent HTTP or WebSocket scopes safely.

Use guaranteed cleanup (`finally`, defer, scope guards, or equivalent native constructs) around scope end where possible. Clearing the scope drops cached references but does not invoke arbitrary resource cleanup methods.

## Sealing and lifecycle

Calling `clm_depend_seal()` prevents:

- New registrations.
- Unregistration.
- Constructor replacement.

Sealing **does not prevent resolution** or construction of previously registered `LAZY`, `SCOPED`, and `TRANSIENT` services.

To reset an application registry, call `clm_depend_destroy()` and then `clm_depend_initialize()`. A sealed registry is reset by destruction, not by an `unseal` operation.

## Status and error reference

The following values are part of the reference implementation's named status set. Other language implementations should preserve their meanings even if they report errors using a different language-native mechanism.

| Status | Numeric value | Meaning |
| --- | ---: | --- |
| `DependStatus.SUCCESS` | `0` | Registration or operation succeeded. |
| `DependStatus.ALREADY_REGISTERED` | `1` | Service ID is already registered. |
| `DependStatus.NOT_REGISTERED` | `2` | Requested service ID is absent. |
| `DependStatus.SEALED` | `3` | Mutation was attempted after sealing. |
| `DependStatus.INVALID_ARGUMENT` | `4` | Invalid ID, factory, dependencies, or lifetime. |
| `DependStatus.MISSING_DEPENDENCY` | `5` | A dependency ID could not be resolved. |
| `DependStatus.INCOMPATIBLE_LIFETIME` | `6` | Shared cached service depends on a scoped service. |
| `DependStatus.CIRCULAR_DEPENDENCY` | `7` | A service was revisited during its own dependency construction. |
| `DependStatus.SCOPE_ALREADY_ACTIVE` | `8` | Attempted to start a second active scope. |
| `DependStatus.SCOPE_NOT_ACTIVE` | `9` | Attempted to end a nonexistent scope. |
| `DependStatus.SCOPE_REQUIRED` | `10` | Scoped resolution was attempted without an active scope. |
| `DependStatus.CONSTRUCTOR_LOCKED` | `11` | Factory replacement or unregistration conflicts with active/completed construction. |

**Error transport varies by operation:** `register` returns a status; `resolve`, `construct`, `unregister`, and scope operations may raise a `DependError` carrying a status and service ID in Python. Initialization and some backend failures may raise runtime errors. Other languages may use status-plus-output parameters or result structures.

### Expected registration failure handling

```text
status = clm_depend_register(...)
if status != SUCCESS:
    report_registration_failure(status)
    abort_application_startup()
```

For eagerly constructed services, failures attributable to dependency-resolution statuses cause the attempted registration to be removed. **This is not a general transaction for earlier successful registrations**.

## Relationship to CLM Container and other modules

`clm_depend` uses the **object** variant of `clm_container` to store service registration records and cached scoped instances. Consumers do not need to interact with these internal containers directly.

- **`clm_container`:** Storage of registered metadata and instances.
- **`clm_depend`:** Dependency graph resolution and lifetime policies.
- **`clm_dispatch`:** May use Depend to supply services when invoking registered message handlers.
- **SPIder:** Can resolve application services or OOP presentation instances without requiring every route handler to be a class method.

### Function-based SPIder example

```text
function get_continents(context):
    application = clm_depend_resolve(CONTINENTS_APPLICATION)
    return application_search_continents(application, context.query)

spider_routes_register(CONTINENTS_ROUTE, HTTP_GET, get_continents)
```

### Optional OOP-style SPIder example

```text
function get_continents(context):
    presentation = clm_depend_resolve(CONTINENTS_PRESENTATION)
    return presentation.get(context)

spider_routes_register(CONTINENTS_ROUTE, HTTP_GET, get_continents)
```

Both styles use the **same** SPIder function-registration mechanism. Depend neither enforces nor detects object-oriented presentation structure.

## Portability and implementation guidance

The following are **implementation choices**, not mandatory requirements of the cross-language contract:

| Concern | Examples of language-native choices |
| --- | --- |
| Service storage | CLM object container backed by maps, associative arrays, vectors, or other data structures. |
| Service values | References, object handles, pointers, interfaces, boxed values, or variant types. |
| Constructor representation | Function pointer, function value, delegate, or callable. |
| Dependency argument collection | Array, slice, vector, or pointer-plus-length representation. |
| Ownership | GC, smart pointers, explicit destruction, reference counting, or borrowed handles with documented lifetimes. |
| Error reporting | Exceptions, result structures, return statuses, or status-plus-output parameters. |
| Synchronization | Locks, atomics, per-service construction guards, or another native coordination mechanism. |

**Do not infer ownership from the lifetime name.** Singleton caching does not make a raw pointer safe, and clearing a scoped cache does not guarantee resource disposal. C and C++ implementations in particular should define who owns constructed instances, whether resolved handles are borrowed or retained, and how instances are released.

The reference implementation uses a global lock during resolution, including construction. A language port must consider deadlocks, reentrancy, contention, and callbacks that recursively access the registry. The existing global construction-tracking set is likewise not a substitute for independent concurrent-resolution state.

## Current implementation boundaries

This document describes the **behavior visible in the supplied reference implementation**, not unimplemented future promises:

1. One global registry and at most one global active scope.
2. `EAGER` and `SINGLETON` are aliases with immediate construction.
3. `LAZY` caches on first resolution; `TRANSIENT` does not cache.
4. Constructor dependencies are passed as **one ordered collection**.
5. Invocation callbacks currently receive **expanded positional dependencies**, a different convention requiring future cross-language API clarification.
6. `use_for` is stored as metadata only; no alias or alternative matching behavior is implemented.
7. No automatic service disposal, shutdown hooks, or ownership transfer protocol is provided.
8. Service factories are synchronous; the asynchronous invocation helper awaits callbacks, not factories.
9. Circular dependency detection occurs during construction, not via whole-graph prevalidation.
10. A missing dependency in an eagerly built registration can cause that registration to fail immediately; with lazy/scoped/transient services the error may not surface until resolution.

## Cross-language compatibility checklist

An implementation can be evaluated against these behaviors:

- [ ] Uses caller-defined nonnegative service IDs and rejects duplicates.
- [ ] Initializes its registry without erasing an already initialized registry.
- [ ] Accepts constructors that receive **one** ordered collection of resolved values.
- [ ] Resolves dependencies in declared order, including an empty list.
- [ ] Implements all five named lifetimes with `SINGLETON` aliasing `EAGER`.
- [ ] Enforces scoped-service availability and shared/scoped compatibility.
- [ ] Detects circular constructions and missing dependency IDs.
- [ ] Prevents changes after sealing, but permits resolution.
- [ ] Rejects constructor replacement for already constructed or actively constructing services.
- [ ] Defines the scope lifecycle and cache reset behavior.
- [ ] Documents instance ownership, disposal, and synchronization limitations.
- [ ] Distinguishes constructor collection passing from the current positional callback invocation behavior.
- [ ] Provides corresponding failure meanings for the documented status values.

---

**CLM — Cross Language Modules**  
*Different languages. Shared technologies. No borders.*  
**Don't start from scratch, bro.**
