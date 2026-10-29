## clm_dispatch — language-neutral contract

Exactly one handler per message ID. Handler declarations list numeric dependency IDs for explicit resolution. Four categorization methods—command, event, query and exception—call the same registered handler; events **do not fan out**. Optional logging receives an envelope before invocation. Each message hop carries an ID, correlation ID, and causation ID; children preserve correlation and record parent causation. The C port requires the parent to be passed explicitly, rather than using implicit asynchronous context propagation. The C implementation does not await asynchronous callbacks; its adapters may submit work to an async executor explicitly. Registrations own copies of dependency lists, **not** dependency instances.

### Cross-language expectations

This document specifies observable behavior, not syntax or compiler bindings. Keep input order, output values, error cases, and data ownership consistent across CLM language ports. For C signatures, see `include/clm_dispatch.h`.
