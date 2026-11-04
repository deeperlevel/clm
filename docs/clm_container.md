# CLM Container

> **Current interface reference** · Language-neutral capability and behavioral description · C header snapshot 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_base](clm_base.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

ID-based service registration and resolution, with explicit dependency validation and instance lifetimes.

## Capabilities and operations

- Register services by numeric ID with constructor, declared dependency IDs and a lifetime policy.
- Resolve eager singletons and per-resolution transients, or use scoped/intern cached instances.
- Begin/end the process-wide scope and intern frames; invoke callbacks after validating dependency registrations.
- Dependencies are validated but not automatically injected; constructors resolve required services themselves.
- The container owns singleton and frame instances. The caller owns transient instances and must release them.

## Service lifecycle model

| Lifetime | Resolution behavior | Ownership |
|---|---|---|
| **Singleton** | Created eagerly during successful registration; later resolutions share the instance. | Container owns the instance until unregister. |
| **Transient** | A fresh instance is constructed on every resolution. | Resolving caller owns and releases the instance. |
| **Scoped** | One cached instance per active scope frame, created at scope start. | Container owns the instance until the scope ends. |
| **Intern** | One cached instance per active intern frame, created at intern start. | Container owns the instance until the intern frame ends. |

## Registration and invocation

- A service registration carries a numeric service ID, a constructor, a lifetime, a context ID and a list of required service IDs.
- Required service IDs must already be registered; the list is checked rather than used for implicit constructor-parameter injection.
- Constructors explicitly resolve their own dependencies. A failed constructor must clean up any partially created resources.
- Dependency-validated invocation executes a supplied operation once after verifying the required services.
- A singleton cannot recursively resolve itself while still being constructed; this is reported as busy.

## Scope and concurrency

The current contract defines explicit process-wide **begin/end scope** and **begin/end intern** transitions. Overlapping or repeated frame transitions can report busy. Unregistering an active service can fail while it is in use. Once a container-owned instance is released, clients must not retain and reuse its ID. This module is service management; it does not replace [clm_dictionary](clm_dictionary.md) or [clm_table](clm_table.md).

## Shared behavioral contract

- CLM resource IDs are identifiers, not automatic lifetime management. The implementation or caller owns resources as described by the relevant module.
- Expose observable outcomes consistently: successful operations, invalid arguments, unsupported functionality and recoverable states must remain distinguishable.
- Use the host language’s native types and error mechanisms where appropriate, while preserving the data model and documented behavior across language ports.
- These descriptions define module-level behavior; binding-specific signatures, allocation mechanics and build instructions belong to each language implementation.
- Singleton lifetime is eager; scoped and intern lifetimes use explicit process-wide frames; transient is per-resolution.

## Example use cases

Register a database service with its dependencies, then resolve it within a controlled lifetime.
