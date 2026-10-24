# CLM Depend

## Purpose

`clm_depend` registers constructors by integer **service ID**, resolves their dependencies in order, stores produced instances in an internal `clm_container` object container, and returns integer **instance IDs**. It is procedural-first and does not require classes, reflection, generics, service locators, or language-specific lifetime detection.

**Important change:** The previous API handed the constructed value to the caller. The revised contract returns a managed **instance ID**. Call `get(instance_id)` to access its value. Constructors receive **one ordered collection of dependency instance IDs**, not the underlying dependency objects. Invocation callbacks receive instance IDs as positional arguments before any caller-supplied arguments. Code previously expecting resolved values must be updated.

The module uses a single process-level registry and, currently, **one active scope at a time**. It synchronizes its registry operations using a reentrant lock in Python; this does not guarantee thread-safe access to arbitrary stored application objects.

## Terminology and identity

- **Service ID:** Integer name of a registered constructor and lifetime policy. Reusing the same service ID for a second registration is rejected.
- **Instance ID:** Monotonically allocated integer identifying one constructed instance, *distinct in purpose from its service ID*. It is not reused by a running Python process, including after registry destruction. Numeric ranges are an implementation detail, not a public contract.
- **Instance value:** Native language object, pointer, struct, handle, or other value created by the constructor.
- **Ownership reference:** A manually maintained count of owners that still require an instance. CLM cannot detect when native variables go out of scope.
- **Disposable transient:** A tracked transient instance with reference count **zero**, which is reclaimed only when `clm_depend_dispose()` runs.

In Python, Depend internally reserves object-container IDs `0xFF01` (registrations), `0xFF02` (active scoped cache), and `0xFF03` (instances). Collisions cause initialization failure. These IDs are internal implementation details.

## Lifetime rules

| Lifetime | Value | Construction / caching | `dispose()` behavior |
|---|---:|---|---|
| `EAGER` / `SINGLETON` | `0x01` | Construct during registration; return same instance ID subsequently | Not collected |
| `LAZY` | `0x02` | Construct on first resolution; cache thereafter | Not collected |
| `SCOPED` | `0x03` | Construct on first resolution within a scope; return same ID within that scope | Not collected; cleaned by `end_scope()` |
| `TRANSIENT` | `0x04` | Construct a fresh instance on every resolution | Collected **only** when references are zero |

Singleton, lazy and scoped instances are reference-counted for metadata/ownership tracking, but callers need not `unreference()` them simply to prevent automatic collection. Their lifecycle is controlled by registration/registry or scope teardown. The implementation will reject closing a scope or unregistering a cached instance while another owner holds a counted reference.

## API catalog (conceptual)

| Function | Meaning |
|---|---|
| `initialize()` | Initialize one registry; idempotent |
| `register(service_id, dependencies, constructor, lifetime, use_for=0, destructor=None)` | Register factory and policy; eager construction happens immediately |
| `resolve(service_id) -> instance_id` | Resolve a factory according to its lifetime, returning instance ID |
| `get(instance_id) -> value` | Retrieve a tracked instance's value, without creating another |
| `get_info(instance_id) -> DependInstanceInfo` | Copy metadata about tracked instance |
| `reference(instance_id)` | Increment explicit owner count |
| `unreference(instance_id)` | Decrement owner count, never free immediately |
| `dispose() -> count` | Dispose all currently zero-reference transient instances, including newly eligible child instances |
| `construct(service_id, constructor)` | Replace unconstructed/unsealed registered constructor |
| `unregister(service_id)` | Remove service registration; cached instance is explicitly torn down when safe |
| `invoke(service_ids, function, *arguments)` | Resolve services, pass their instance IDs positionally, invoke callback, release temporary transient ownership |
| `invoke_async(service_ids, async_function, *arguments)` | Same ownership rule; release only **after** the await finishes |
| `begin_scope()` / `end_scope()` | Start/end the single active scope |
| `seal()` | Prevent registration, unregistration and constructor replacement |
| `destroy()` | Explicit shutdown of the entire registry and all its owned instances |

Instance metadata:

```text
DependInstanceInfo
    id: Integer
    service_id: Integer
    lifetime: DependLifetime
    references: Integer
    dependencies: Ordered list of instance IDs
```

In Python, `register()` returns `DependStatus` and validation/resolution/lifecycle failures commonly raise `DependError` with a `status` and failing ID; callback/destructor exceptions propagate unchanged. Other languages may use out-parameters/status returns, but should preserve the same observable lifecycle.

### Core example

```python
from clm_depend import *

clm_depend_initialize()

POSITION = 100
SHIP = 200

clm_depend_register(POSITION, [], lambda _: {'x': 0, 'y': 0}, DependLifetime.TRANSIENT)
clm_depend_register(SHIP, [POSITION],
                    lambda dependencies: {'position_id': dependencies[0]},
                    DependLifetime.TRANSIENT)

ship_id = clm_depend_resolve(SHIP)
ship = clm_depend_get(ship_id)
position = clm_depend_get(ship['position_id'])
position['x'] += 5

clm_depend_unreference(ship_id)  # ship remains in Depend's instance container
removed = clm_depend_dispose()  # disposes ship, then its unreferenced Position
clm_depend_destroy()
```

## Reference ownership model

**Each new instance starts with references = 1**. A direct `resolve(TRANSIENT)` transfers that initial reference to the caller. The caller must explicitly `unreference(instance_id)` when done if it wants deferred disposal. A pool may deliberately keep a positive reference until it retires the resource. Forgetting to unreference means the transient stays tracked; this is safe against premature disposal but can retain memory indefinitely.

When Depend constructs a parent with dependencies:

1. It resolves each dependency *in registration order*.
2. A freshly constructed transient dependency's initial reference is **transferred to the parent**; it does not receive a gratuitous second reference.
3. An already cached EAGER/LAZY/SCOPED dependency gets an extra reference for the parent's ownership.
4. The parent stores its dependency instance IDs in its own tracking metadata and passes the IDs to its constructor as one ordered collection.
5. On parent disposal, Depend unreferences the tracked children. Eligible transient descendants can then be collected in the **same** disposal cycle.
6. If construction fails, Depend releases the references acquired during partial dependency resolution. Already constructed, unreferenced transient children await a later explicit disposal cycle.

Do **not** unreference a dependency merely because it appears in the parent's constructor argument list: that reference belongs to the parent. If the dependency is also retained separately outside the parent's lifetime, the additional owner must call `reference(dependency_instance_id)` and eventually `unreference` it.

For services that have no independent ownership beyond the parent, Depend's tracked dependency links suffice. `clm_depend` does not inspect the returned value to discover other native pointers/references, nor does it trace arbitrary cycles.

### Transients, pooling and ownership

```python
ship_id = clm_depend_resolve(SHIP)   # references: 1 (active caller)
clm_depend_reference(ship_id)       # references: 2 (pool retains ownership)
clm_depend_unreference(ship_id)     # references: 1 (caller is done)
clm_depend_dispose()                # pool-owned ship survives
clm_depend_unreference(ship_id)     # references: 0
clm_depend_dispose()                # ship becomes reclaimable
```

Calling `unreference` when the count is already zero raises `REFERENCE_UNDERFLOW`. Accessing an instance ID after disposal raises `INSTANCE_NOT_FOUND`. `get()` does not implicitly acquire a reference. Holding a returned native value beyond disposal is the developer's responsibility.

## Destructors and disposal

An optional `destructor(value)` may be supplied when registering. The instance retains its destructor independently of subsequent registration changes. The destructor runs **before** its record is removed from the instance store, and before Depend releases its owned dependency references. Python's container removal relinquishes the container's Python reference, but cannot deterministically free arbitrary foreign native resources; destructors are especially important for C implementations.

`dispose()` considers **only TRANSIENT instances** with zero references. It repeats sweeps because disposing a parent can bring its child to zero. Returns number of disposed instances. A destructor exception stops the sweep; the affected instance remains tracked for retry. Destructors should be idempotent or carefully handle partial cleanup if they can throw.

`dispose()` never reclaims singleton, lazy, or scoped instances solely because their reference counts happen to be zero. `end_scope()`, `unregister()`, and `destroy()` are explicit lifecycle operations *distinct from routine disposal*. `end_scope()` and unregister of a cached item refuse teardown if the tracked reference count is greater than one. Destruction of the overall registry is an explicit forced shutdown; callers must not keep using handles afterward.

## Injection and scope rules

A factory signature is **one ordered collection**:

```text
factory([dependency_instance_id_0, dependency_instance_id_1, ...]) -> value
```

`invoke()` instead expands resolved IDs positionally before any supplied payload arguments. It automatically releases the temporary ownership obtained for its argument resolutions when callback execution ends. `invoke_async()` retains that ownership across suspension until the awaited callback finishes. If callers retain those IDs after invocation, they must explicitly acquire their own reference while the callback runs.

Currently only one global active scope can exist. `SCOPED` resolution without a scope is an error; singleton/lazy parents cannot depend on a scoped service. Ending the scope removes cached scoped instances but does not call the transient sweep: children reaching zero are collected by a later `dispose()` call. The module is **not** yet suitable for concurrent independent request scopes without additional scope-context isolation.

## Failure and compatibility notes

- Constructor exceptions propagate; eager registration rolls back its registration on failure.
- Missing registered dependencies and dependency cycles are errors.
- Sealing prevents edits to registrations but does not block resolution or disposal.
- `use_for` is retained as metadata; it does not implement alias-based resolution.
- Service IDs and instance IDs are logically different; `resolve()` accepts **service IDs** whereas `get()`, `reference()`, and `unreference()` accept **instance IDs**.
- This version **breaks** the former value-returning `resolve()` contract. Applications built on the old API, including some older CLM Dispatch code, must be migrated to the ID-based contract.
- Dynamic runtime ECS composition can use transient factories and dependency graphs, while entity/component queries and system execution belong in a higher-level `clm_ecs` module.
- There is no automatic reference discovery, tracing garbage collection, or object-cycle detection.
- Avoid callbacks/destructors that recursively dispose their own instance. Thread-safe coordination between external consumer objects and a background `clm_managed` collector still requires careful synchronization.

## Integration with CLM Managed

`clm_managed` can schedule calls to:

```text
clm_depend_dispose()
clm_container_dispose()
clm_storage_dispose()
```

Each module retains authority over its own eligibility and teardown rules. Managed does not infer native-language scope or pointer reachability. It can be disabled entirely without changing the `clm_depend` lifecycle contract.

## Porting considerations

- C: an object container may hold pointers plus optional destructor callbacks; service and instance IDs should be fixed-width integer types sized for the chosen ID policy.
- C++/D: smart pointers are optional implementation choices, not required by the public procedural API.
- Python/Go: native collectors coexist with CLM's **explicit logical resource references**; one does not replace the other.
- Tests for every language should exercise unique transient IDs, parent/child teardown, cached lifetime exemptions, failed constructors, invalid IDs, invocation ownership, and reference underflow.

## Tests (Python)

```bash
python -m unittest -v test_clm_depend
```

The Python tests validate the behavior above using the updated `clm_container.py` as the storage dependency.


## Runtime dependency composition (extension)

These operations extend the tracked-instance model:

- `clm_depend_attach(parent_instance_id, dependency_service_id)` creates a new
  TRANSIENT dependency instance, adds its ID to the parent's ordered dependency
  metadata, and transfers the child's initial reference to its parent. Returns
  the child instance ID. The parent must be a live TRANSIENT with references > 0.
- `clm_depend_detach(parent_instance_id, dependency_service_id)` removes a
  matching dependency from the parent's metadata and releases the parent's one
  reference. Returns the detached instance ID, which is only *eligible* for
  collection when its count reaches zero. Existing duplicate service IDs are
  disallowed by attach.
- `clm_depend_pending_count()` counts zero-reference TRANSIENT instances awaiting
  disposal; singleton, lazy, and scoped instances are excluded.

Constructors receive an ordered list of dependency **instance IDs**. Dynamic
attachment modifies Depend's ownership graph, **not automatically the fields of
any application-defined object** previously returned by its constructor.
Applications should consult `clm_depend_get_info()` or their ECS facade to see
updated composition. This is manual reference management, not automatic tracing.

**Scope limitation:** the Python implementation still has one global active
scope. It does not provide isolated request/task scopes; concurrent multi-scope
support requires a separately specified scope-ID API and lifecycle contract.
