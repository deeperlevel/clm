# CLM Dispatch

**Cross Language Modules · Language-agnostic module reference**

CLM is an initiative by **Deeperlevel**, developed with community contributions.  
Official repository: https://github.com/deeperlevel/clm  
Contact: deeparchive@outlook.com  
License: [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/) — subject to the rights applicable to each contribution.

> **Different languages. Shared technologies. No borders.**  
> *Don't start from scratch, bro.*

## Overview

`clm_dispatch` is a **procedural, ID-based, asynchronous message dispatcher**. Applications associate numeric message IDs with handler functions, declare which registered services each handler needs, and dispatch messages without requiring inheritance, reflection, decorators, or a particular object model.

CLM Dispatch composes two other CLM modules:

- **`clm_container`** stores handler registrations and optional logger callbacks in object containers.
- **`clm_depend`** resolves each handler's declared service dependencies and invokes its callback.

A handler may call procedural application functions, operate on structures, or use optional object-oriented services. Dispatch does not prescribe the application architecture.

```text
clm_dispatch_query(MESSAGE_SEARCH, request)
              |
              v
    create correlation chain
              |
              v
    best-effort query logger (optional)
              |
              v
    find registered handler by message ID
              |
              v
    CLM Depend resolves declared service IDs
              |
              v
    await handler(resolved services..., message)
              |
              v
       return handler result
```

## Design principles

- **Procedural first:** Public functionality is exposed through functions; an optional facade may delegate to them.
- **Explicit registration:** A handler is associated with a numeric message ID; no runtime handler discovery is needed.
- **One handler per message ID:** The current dispatcher is *not* a multi-subscriber event bus.
- **Asynchronous handlers:** Handler execution is awaitable; logger callbacks may be synchronous or asynchronous.
- **Composable dependency injection:** The handler's dependency IDs are resolved through CLM Depend rather than a second resolver.
- **Optional observability:** Logging failures do not block message processing.
- **Traceable execution:** Correlation and causation IDs relate nested dispatch calls.
- **Language-native implementation:** Event loops, futures, callbacks, task-local storage, delegates, and function pointers may differ among language ports.

## Concepts and terminology

| Term | Meaning |
| --- | --- |
| **Message ID** | Caller-defined, nonnegative integer identifying a handler registration. |
| **Payload** | Message data supplied by the caller; it can be any supported native value. |
| **Handler** | Registered asynchronous callback processing the payload. |
| **Dependency IDs** | Ordered collection of CLM Depend service IDs required by the handler. |
| **Dispatch kind** | Semantic category: command, event, query, or exception. |
| **Logger** | Optional callback receiving an envelope before a handler is invoked. |
| **Chain** | Identification of a dispatch hop and its correlation/causation relationship. |
| **Message envelope** | Logging record containing a chain, payload, and dispatch-kind context. |

## Module state and reserved identifiers

The reference implementation maintains one process-local dispatcher registry backed by two CLM object containers.

| Constant | Value | Purpose |
| --- | --- | --- |
| `HANDLER_CONTAINER` | `0xFED001` | Registered message handlers. |
| `LOGGER_CONTAINER` | `0xFED002` | Optional category logger callbacks. |
| `COMMAND_LOGGER` | `0xFED003` | Logger for command dispatches. |
| `EVENT_LOGGER` | `0xFED004` | Logger for event dispatches. |
| `QUERY_LOGGER` | `0xFED005` | Logger for query dispatches. |
| `EXCEPTION_LOGGER` | `0xFED006` | Logger for exception-category dispatches. |

The logger IDs identify categories, **not** service IDs to be resolved through CLM Depend. Avoid reusing reserved container IDs for unrelated CLM containers.

`clm_dispatch_init()` is idempotent. Registering a handler/logger or invoking an uninitialized dispatcher initializes it automatically. Initialization fails if its reserved container IDs cannot be created. `clm_dispatch_destroy()` removes both containers and resets dispatch initialization state; it does **not** destroy CLM Depend or its services.

## Registration and invocation contract

### Handler registration

```text
clm_dispatch_register_handler(message_id, dependency_ids, handler)
```

- `message_id` must be a nonnegative integer.
- `dependency_ids` is an ordered collection of nonnegative service IDs; it may be empty.
- `handler` must be an asynchronous callable.
- A message ID may have **one** registered handler. Duplicate registration fails.
- Registered callbacks and their dependency IDs are retained until unregistered or the dispatcher is destroyed.
- The handler registration is not differentiated by dispatch kind: command, event, query, and exception dispatches consult the **same handler registry**.

A language port may express callback validation using its own callable/function-pointer type system rather than runtime inspection.

### Handler invocation

```text
result = await clm_dispatch_invoke(message_id, payload)
```

`clm_dispatch_invoke()` finds the handler, asks CLM Depend to resolve its declared dependencies, invokes the handler asynchronously, and returns its result. It raises/reports a dispatch failure when the message ID has no handler. Dependency-resolution errors and handler errors propagate.

**Important difference from constructors:** CLM Depend constructors receive **one collection** of resolved dependencies. In the supplied Dispatch reference, handler callbacks instead receive **the resolved dependencies as separate leading arguments, followed by the payload**:

```text
handler(dependency_1, dependency_2, ..., payload)
```

Conceptual pseudocode:

```text
FUNCTION search_continents(repository, payload) ASYNC
    RETURN AWAIT repository_search(repository, payload)
END

clm_dispatch_register_handler(
    MESSAGE_SEARCH_CONTINENTS,
    [SERVICE_CONTINENTS_REPOSITORY],
    search_continents
)

result = AWAIT clm_dispatch_query(MESSAGE_SEARCH_CONTINENTS, search_request)
```

The exact declaration syntax varies by language. No presentation class or method convention is required. Although the function above uses an object-like `repository` value, it could equally use a structure pointer, handle, or other resolved service.

> **Portability consideration:** Positional callback injection is the behavior of the current reference implementation, even though CLM constructor factories use a single dependency collection. A future shared invocation contract could change, but ports must not silently claim that the existing implementation passes a dependency array to handlers.

### Removing a handler

```text
clm_dispatch_unregister_handler(message_id)
```

Removes an existing message registration. Removing an unknown message ID fails. No automatic disposal of dependencies or callback-owned resources is performed.

## Dispatch operations

The four public dispatch operations share the same handler lookup and execution machinery. Their difference is **semantic context and logger selection**, not a different handler signature or registry.

| Operation | Meaning | Logger category | Envelope context |
| --- | --- | --- | --- |
| `clm_dispatch_execute(...)` | Execute a command | `COMMAND_LOGGER` | `"command"` |
| `clm_dispatch_emit(...)` | Emit an event | `EVENT_LOGGER` | `"event"` |
| `clm_dispatch_query(...)` | Execute a query and return its result | `QUERY_LOGGER` | `"query"` |
| `clm_dispatch_caught(...)` | Dispatch an exception-category message | `EXCEPTION_LOGGER` | `"exception"` |

Each operation has the conceptual signature:

```text
ASYNC dispatch_operation(message_id, payload, log = true, chain = none) -> result
```

All four return the handler's result. A command or event handler can return a value; the API does not discard it. The caller may disable the optional logger using `log = false`.

**Events are single-handler messages in this version.** `clm_dispatch_emit()` does not broadcast to multiple listeners, does not provide subscriptions, and does not implement fan-out. Similarly, `clm_dispatch_caught()` is an explicit operation; it does **not** automatically intercept uncaught exceptions elsewhere in an application.

**Direct invocation:** `clm_dispatch_invoke()` runs the handler directly. Unlike the four categorized public dispatch operations, it does not independently create a chain or invoke a category logger.

## Correlation and causation chains

Each categorized dispatch call creates a `Chain` record with three fields:

| Field | Meaning |
| --- | --- |
| `id` | Unique identity of this dispatch hop. |
| `correlation_id` | Identity shared by all related hops in a chain. |
| `causation_id` | Identity of the parent dispatch hop, or an empty value for a root. |

### Chain creation

```text
clm_dispatch_start_new() -> Chain
clm_dispatch_next(parent_chain) -> Chain
```

`start_new()` creates a root with `id == correlation_id` and an empty `causation_id`. `next(parent)` creates a fresh hop ID, preserves the parent's `correlation_id`, and sets `causation_id` to the parent's `id`.

```text
Root dispatch
  id:            A
  correlation_id: A
  causation_id:  ""
        |
        v
Nested dispatch
  id:            B
  correlation_id: A
  causation_id:  A
        |
        v
Further nested dispatch
  id:            C
  correlation_id: A
  causation_id:  B
```

The reference uses random UUID-derived identifiers, but the cross-language contract is about **uniqueness and relationships**, not a requirement to use one platform-specific UUID library.

### Automatic nested propagation

When a categorized dispatch runs inside another categorized dispatch, the child automatically continues the current chain. The reference uses execution-context-local storage so concurrent asynchronous tasks can preserve their own trace context.

The optional `chain` argument overrides the automatically inherited parent for that call. When the operation completes—successfully or exceptionally—the previous execution context is restored.

Language ports should use their native execution-context propagation mechanism, or explicitly pass the chain when such a mechanism is unavailable. A single unsynchronized global current-chain variable is **not equivalent** under concurrent execution.

> The chain is exposed to the logger via its envelope. The normal handler callback receives its resolved dependencies and payload, **not** an additional chain argument.

## Optional logging

### Logger registration

```text
clm_dispatch_register_logger(logger_id, logger_callback)
clm_dispatch_unregister_logger(logger_id)
```

- Only the four predefined logger IDs are accepted.
- At most one logger may be registered per category.
- Loggers can be synchronous or asynchronous.
- A duplicate logger registration fails; removal of an unknown logger fails.
- Logger functions receive **one `Message` envelope**, not dependency-injected leading arguments.

The envelope contains:

| Field | Value |
| --- | --- |
| `chain` | The `Chain` for this dispatch hop. |
| `message` | The original payload. |
| `context` | One of `"command"`, `"event"`, `"query"`, or `"exception"`. |

Conceptual pseudocode:

```text
FUNCTION log_query(envelope)
    write_log(envelope.chain.correlation_id, envelope.context, envelope.message)
END

clm_dispatch_register_logger(QUERY_LOGGER, log_query)

result = AWAIT clm_dispatch_query(MESSAGE_SEARCH_CONTINENTS, request)
```

### Best-effort policy

Category logging is optional and runs **before** the registered handler. If `log` is disabled or the category has no registered logger, handler execution proceeds normally.

The reference intentionally suppresses errors raised while finding or executing an optional logger. Logging failures must not prevent handler invocation. Async cancellation/control-flow termination is **not** intended to be swallowed as an ordinary logging error.

Logger calls are awaited when asynchronous, so slow loggers can still delay handler execution. **Best-effort does not mean background, concurrent, or non-blocking logging.**

Handler exceptions and CLM Depend resolution exceptions are **not** suppressed by the optional logging policy.

## Function reference

| Function | Purpose | Notable behavior |
| --- | --- | --- |
| `clm_dispatch_init()` | Initialize registry containers. | Idempotent; may fail on reserved-ID collision. |
| `clm_dispatch_destroy()` | Destroy dispatcher containers. | Does not destroy injected services. |
| `clm_dispatch_register_handler(message_id, dependencies, callback)` | Associate one handler with a message ID. | Async callback required; duplicates rejected. |
| `clm_dispatch_unregister_handler(message_id)` | Remove a registered handler. | Fails if missing. |
| `clm_dispatch_register_logger(logger_id, callback)` | Register one category logger. | Sync or async; four recognized IDs only. |
| `clm_dispatch_unregister_logger(logger_id)` | Remove a logger. | Fails if missing. |
| `clm_dispatch_start_new()` | Start an independent correlation chain. | Root ID equals correlation ID. |
| `clm_dispatch_next(chain)` | Create a child hop. | Preserves correlation; records causation. |
| `clm_dispatch_invoke(message_id, message)` | Invoke a registered handler through CLM Depend. | Async; no automatic category logging. |
| `clm_dispatch_execute(message_id, message, log, chain)` | Dispatch command. | Optional command logging. |
| `clm_dispatch_emit(message_id, message, log, chain)` | Dispatch event. | One registered handler, not fan-out. |
| `clm_dispatch_query(message_id, message, log, chain)` | Dispatch query. | Returns handler result. |
| `clm_dispatch_caught(message_id, message, log, chain)` | Dispatch exception-category message. | Explicit call, not automatic interception. |

Parameter spelling and return/error representation may vary slightly by language, but the operation's meaning should remain consistent.

## Error conditions

Unlike some other CLM modules, this reference does **not** define a public numeric `DispatchStatus` enumeration. Its Python implementation uses `DispatchError` and argument-type errors. A port may use language-native exceptions, return codes, or status-plus-output conventions while preserving the distinctions below.

| Condition | Expected behavior |
| --- | --- |
| Invalid message ID | Reject registration. |
| Invalid dependency ID or dependency collection | Reject registration. |
| Non-callable or non-asynchronous handler | Reject registration. |
| Duplicate message handler ID | Reject registration without replacing the existing handler. |
| Missing handler during invocation | Report dispatch failure. |
| Unsupported logger category | Reject logger registration. |
| Duplicate logger category | Reject registration without replacing the logger. |
| Missing handler/logger during explicit removal | Report failure. |
| Invalid chain argument | Reject the dispatch call. |
| Missing or invalid CLM Depend service | Propagate dependency resolution failure. |
| Handler failure | Propagate the handler's error. |
| Logger failure | Ignore ordinary logger errors and still invoke the handler. |
| Reserved container ID unavailable | Fail initialization. |

## Initialization, composition, and lifecycle

A typical application initialization sequence is:

```text
clm_depend_initialize()

register_repositories_and_services()

clm_dispatch_init()
clm_dispatch_register_handler(MESSAGE_SEARCH_CONTINENTS, [REPOSITORY_ID], search_handler)
clm_dispatch_register_logger(QUERY_LOGGER, query_logger)  // optional

start_application_backends()
```

Dispatch may initialize itself, but its **dependency services must still be registered with CLM Depend** before the relevant handler is invoked. `clm_depend` owns service lifetimes; Dispatch does not substitute for that subsystem.

When shutting down, stop accepting work and allow in-flight dispatches to finish before removing registrations or destroying the dispatcher. The reference has a process-wide registry; destroying it while work is in progress is not a supported concurrency-safe operation.

## Using Dispatch from SPIder

SPIder's transport-specific entry points can invoke the same application handler registry. The transport adapter remains responsible for decoding an HTTP request, WebSocket message, gRPC call, or socket payload into an appropriate message and for formatting the response.

```text
HTTP handler  ─────┐
WebSocket handler ─┼──> clm_dispatch_query(MESSAGE_SEARCH, payload)
gRPC handler ──────┘                  |
                                    v
                           registered handler
                                    |
                                    v
                         application / repository
```

No mandatory presentation class is involved. Optional object-oriented applications may inject an instance through CLM Depend while the registered transport and dispatch handlers remain functions.

## Cross-language implementation guidance

The shared contract defines **observable behavior**, not a common runtime or identical source representation.

| Concern | Language-independent requirement |
| --- | --- |
| Handler storage | Associate numeric IDs with one callback and ordered dependency IDs. |
| Callback representation | Use native functions, pointers, delegates, or equivalent callable handles. |
| Async execution | Support awaitable/asynchronous completion or an equivalent completion mechanism. |
| Dependency invocation | Delegate resolution to CLM Depend; preserve the current callback argument order. |
| Tracing | Produce unique hop IDs and preserve correlation/causation relationships. |
| Concurrent contexts | Isolate trace context among concurrent requests/tasks. |
| Logging | Supply envelope data; tolerate ordinary optional-logger errors. |
| Resource lifetime | Define ownership and cleanup for callbacks, message payloads, services, and in-flight operations. |
| Synchronization | Protect registry mutation and lifecycle without holding blocking locks across awaited callbacks. |

### Portability boundaries in the reference

1. The reference has one **process-wide** registry, rather than independently instantiable dispatcher registries.
2. Only **one handler** is supported per message ID, including events.
3. All four categorized dispatch APIs share that same handler namespace.
4. Handlers are **asynchronous**, while logger callbacks may be synchronous or asynchronous.
5. Dependencies are supplied to handlers as **leading positional arguments**, unlike array-based CLM Depend constructors.
6. Trace IDs use UUID-style values in the reference; the contract needs unique identifiers and parent-child relationships.
7. Nested correlation propagation relies on execution-context-local state in the reference.
8. Logging is optional and best-effort but **awaited**, not automatically queued in the background.
9. `caught()` is an explicit exception-category dispatch, not a global exception hook.
10. No built-in handler fan-out, retries, message persistence, transactional delivery, or automatic resource disposal is provided.
11. The reference locks registry mutations; callers should coordinate lifecycle changes with in-flight asynchronous operations.

## Compatibility checklist

- [ ] Registers one asynchronous handler for each nonnegative message ID.
- [ ] Rejects duplicate handler IDs and invalid dependency IDs.
- [ ] Resolves ordered handler dependencies using CLM Depend.
- [ ] Invokes the handler with resolved dependencies **followed by** the message payload, consistent with the current reference.
- [ ] Returns the handler's result and propagates handler/dependency failures.
- [ ] Supports command, event, query, and exception-category dispatches.
- [ ] Treats event dispatch as single-handler, not fan-out.
- [ ] Provides optional synchronous/asynchronous loggers per dispatch category.
- [ ] Supplies the `Chain`, original payload, and context string in logger envelopes.
- [ ] Does not let ordinary logger failures prevent handler execution.
- [ ] Creates root and child chains preserving correlation and causation relationships.
- [ ] Isolates nested trace context across concurrent executions.
- [ ] Initializes/destroys the registry without changing CLM Depend's lifecycle.
- [ ] Documents ownership, concurrency, and asynchronous-execution conventions for the target language.

---

**CLM — Cross Language Modules**  
*Different languages. Shared technologies. No borders.*  
**Don't start from scratch, bro.**


## Tracked Depend interoperability (revised)

`clm_dispatch_invoke(message_id, payload)` resolves each declared Depend service ID,
obtains the corresponding **value** with `clm_depend_get(instance_id)`, and invokes
`handler(dependency_values, payload)` (one ordered collection, then payload).

- A transient resolution owns one reference for the duration of the handler, including async awaits.
- All invocation-owned transient references are released in reverse order on completion,
  dependency-resolution failure, handler exception, or cancellation.
- Cached singleton, lazy and scoped instances are not automatically unreferenced by Dispatch.
- Released transient records stay tracked until `clm_depend_dispose()` (or Managed) collects them.
- A handler that retains any transient value **beyond the callback** must explicitly acquire
  an additional reference to the instance ID through Depend; merely retaining a Python value
  does not pin its CLM resource. For retained ownership, use an explicit Depend handle.
- Exactly one registered handler per message ID; `emit` is not a fan-out bus.
- Logger failures are best-effort and do not suppress handler failures.

Concurrent async callbacks are possible; Depend's current single active scope remains
process-global, and must not be mistaken for an async-request-local scope.
