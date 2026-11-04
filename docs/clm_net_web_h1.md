# CLM Net Web HTTP/1.1

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_net_web](clm_net_web.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Offers a constrained server entry point that accepts HTTP/1.1 requests and shares the main CLM web server routing/middleware interfaces.

## Capabilities and operations

- Configure a server restricted to HTTP/1.1.
- Create, bind, start, stop and destroy its listener.
- Retrieve the bound address and use common routes, middleware and handlers.

## Resource, data and ownership model

The facade uses the same server, request, response and route concepts as `clm_net_web`. It adds a protocol restriction, not an incompatible request model.

## Behavioral contract and boundaries

HTTP/1.1 is required. If TLS is configured, the advertised protocol is `http/1.1`; clients that send an incompatible version are rejected before route dispatch. Plain HTTP is supported.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Create an HTTP/1.1 service that serves one landing page over plain HTTP or HTTPS.

## Related CLM modules

[clm_net_web](clm_net_web.md).
