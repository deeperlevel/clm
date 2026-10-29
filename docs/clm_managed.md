## clm_managed — language-neutral contract

Optional coordination of three cleanup phases: Depend → Container → Storage. Four policies: manual, time interval, resource threshold, or hybrid (either trigger). Two execution modes: caller-driven polling or a worker thread. Disabling stops automatic collection; explicit collection remains possible. Cleanup callbacks provide disposed counts and pending estimates. Management callbacks must not re-enter the coordinator; failure statistics record unsuccessful sweeps. This is **not** a garbage collector and does not own the backing resources.

### Cross-language expectations

This document specifies observable behavior, not syntax or compiler bindings. Keep input order, output values, error cases, and data ownership consistent across CLM language ports. For C signatures, see `include/clm_managed.h`.
