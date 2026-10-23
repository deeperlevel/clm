# CLM Managed — Language-Agnostic Specification

**One-line description:** Optional manual or interval-based coordinator for deferred disposal of CLM Depend, Container and Storage resources.

**Project:** CLM — Cross Language Modules · Deeperlevel · CC0 1.0 Universal  
**Motto:** Don't start from scratch, bro.

## Goal

`clm_managed` is **not** a tracing, cycle-detecting or host-language garbage collector. It is an optional scheduler invoking resource-owning CLM modules' `dispose()` operations. No stack scanning, pointer discovery, scope inference, automatic reference counting, arbitrary object cleanup or forced reclamation occurs.

Resource reference counts are **manually maintained** through `clm_depend_reference/unreference`, `clm_container_reference/unreference`, and `clm_storage_reference/unreference`. A reference count of zero marks a resource eligible for its owner's disposal API; it does not immediately destroy it. Normal Depend disposal reclaims **transients only**, not singleton, lazy, or scoped instances.

Dependencies: `clm_depend`, `clm_container`, `clm_storage` (and Storage's `clm_block`/`clm_block3d` imports in the Python edition).

## Modes

| Setting | Codes | Description |
| --- | --- | --- |
| Algorithm | `MANUAL=0x01`, `INTERVAL=0x02` | Manual collects only on request; interval allows scheduling. |
| Execution mode | `CALLER=0x01`, `WORKER=0x02` | Caller explicitly polls due intervals; worker uses a dedicated thread. |
| Interval | Integer milliseconds, ≥1 | Used only when algorithm is INTERVAL. Default 5000 ms. |
| Enabled | Boolean | **Disabled by default**; enabling does not itself collect. |

Threshold and hybrid scheduling are possible later features but are **not implemented**. The first version does not support arbitrary thread affinity or targeting a particular existing application thread.

## Procedural API

| Function | Meaning |
| --- | --- |
| `clm_managed_initialize()` | Idempotently mark coordinator ready; leave disabled. |
| `clm_managed_enable()` | Enable selected algorithm and start dedicated worker if configured. |
| `clm_managed_disable()` | Stop future scheduled cycles and join worker; no collection is triggered. |
| `clm_managed_set_algorithm(algorithm)` | Select manual or interval; configure only while disabled. |
| `clm_managed_set_interval(milliseconds)` | Configure collection interval; only while disabled. |
| `clm_managed_set_thread(mode)` | Select caller or worker; only while disabled. |
| `clm_managed_collect()` | Perform immediate collection **even when disabled**, returning `(transients, containers, storages)` counts. |
| `clm_managed_poll()` | With enabled INTERVAL+CALLER, collect if elapsed time ≥ configured interval; return true if cycle ran. |
| `clm_managed_get_stats()` | Return snapshot totals and last error. |
| `clm_managed_is_enabled()` | Return enabled state. |
| `clm_managed_destroy()` | Disable/join worker, clear coordinator settings/statistics, without automatically collecting or destroying dependents. |

Status enum: `SUCCESS=0`, `NOT_INITIALIZED=1`, `INVALID_ARGUMENT=2`, `ACTIVE=3` (configuration refused while enabled). Errors from `collect()` propagate in the calling context; a worker records exceptions and stays alive. `ManagedStats` comprises `cycles`, `disposed_transients`, `disposed_containers`, `disposed_storages`, `failures`, `last_error`.

## Collection cycle

```text
function clm_managed_collect():
    transients = clm_depend_dispose()
    container_status, containers = clm_container_dispose()
    storage_status, storages = clm_storage_dispose()
    validate statuses
    record successful cycle and counts
    return (transients, containers, storages)
```

The ordering is deliberate: disposing transient instances may release references to Containers or Storage resources in custom destructors, making them eligible in the same collection cycle. `clm_depend_dispose()` itself sweeps owned transient dependency trees.

**Failure behavior:** an exception stops the cycle. Prior module sweeps may already have reclaimed resources: collection is *not atomic*. Completed collection counts are only committed to ManagedStats after all three modules succeed. A failed cycle increases `failures` and records `last_error`. Destructor failures in Depend follow Depend's contract.

## Examples (pseudocode)

Manual collection on a game maintenance boundary:

```text
clm_depend_initialize()
clm_managed_initialize()
...
clm_depend_unreference(enemy_id)
clm_managed_collect()  // allowed while automatic collection is disabled
clm_managed_destroy()
clm_depend_destroy()
```

Caller-thread interval scheduling:

```text
clm_managed_initialize()
clm_managed_set_algorithm(INTERVAL)
clm_managed_set_interval(5000)
clm_managed_set_thread(CALLER)
clm_managed_enable()

while running:
    update_game()
    clm_managed_poll()  // checks interval on this thread

clm_managed_disable()
clm_managed_destroy()
```

Dedicated-worker mode:

```text
clm_managed_set_algorithm(INTERVAL)
clm_managed_set_thread(WORKER)
clm_managed_set_interval(5000)
clm_managed_enable()
// ... background cycles ...
clm_managed_disable()  // waits for in-progress cycle
```

## Concurrency and ownership caveats

**WARNING: worker mode is not automatically safe alongside arbitrary concurrent CLM Container or Storage mutations.** The coordinator serializes its *own* collection calls, but existing Container/Storage registries do not guarantee full concurrent access safety. Host applications must ensure there are no races between worker collection and mutations, either by external serialization, a safe point/stop-the-world scheme, or by using CALLER + `poll()` on the appropriate application thread. In particular, do not start worker disposal while C/C++ game simulation threads access resources without synchronization. CALLER+MANUAL is the safest default for custom engines.

Do not allow the background worker to destroy a referenced resource by failing to register a manual owner. These are developer obligations; Managed deliberately does not introspect object graphs. A zero-reference resource may still be borrowed until a collect, but callers should reacquire a reference before retaining it.

### Initialization and shutdown

Initialize Depend (and any other required CLM subsystem) before collection. Destroy Managed or disable it **before** destroying Depend, Container or Storage infrastructure. In the Python version `clm_managed_collect()` before Depend initialization raises and records a failed cycle. Disabling waits for an active worker collection to finish; manual collection performed by another caller is synchronized by the coordinator lock, but unrelated external code is not.

## Portability

The algorithm uses manual counters, procedural dispose entry points and a monotonic scheduling clock. C implementations may use OS threads or platform adapters, while Python uses `threading` and `time.monotonic`. The contract does not require a language GC, reflection, object ownership introspection, or mandatory background worker.


## Threshold and hybrid scheduling (extension)

Collection scheduling additionally supports `ManagedAlgorithm.THRESHOLD` (`0x03`)
and `ManagedAlgorithm.HYBRID` (`0x04`). Configure these only when disabled.
`clm_managed_set_threshold(count)` accepts a positive integer; default: 10.

A pending resource is a zero-reference transient Depend instance, Container,
or Storage resource. `THRESHOLD` collects when their combined pending count
reaches `count`. `HYBRID` collects when **either** the pending count crosses
`count` or the configured interval elapses. `INTERVAL` and `MANUAL` retain
their existing behavior. Manual `collect()` always performs a sweep even when
the automatic coordinator is disabled.

`clm_managed_poll()` is the caller-thread scheduler; a worker-mode scheduler
checks at the configured interval. Thus threshold collection is checked at
**poll times**, not synchronously on every release. Worker mode polls at the
configured interval; it does not awaken on each reference decrement.

CLM Managed does not trace host pointers or retain its own application memory.
Its worker thread cannot make unsynchronized Container/Storage access safe;
applications should coordinate mutations and collection, preferably collecting
at controlled points in interactive/game loops.
