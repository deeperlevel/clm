## clm_ecs — language-neutral contract

Component type registration, entity blueprint registration, instance creation, component attachment/removal/get, system registration, component-set queries and system invocation. A query matches entities containing all requested component types (supersets match). Creating an entity creates its declared component instances; removing/deleting an entity invokes registered destructors. This first C implementation is single-threaded, permits callbacks to inspect data, and **forbids altering ECS state while querying or invoking systems**. IDs identify entity instances separately from component/blueprint IDs.

### Cross-language expectations

This document specifies observable behavior, not syntax or compiler bindings. Keep input order, output values, error cases, and data ownership consistent across CLM language ports. For C signatures, see `include/clm_ecs.h`.
