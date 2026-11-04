# CLM Net Web Client HTTP/3

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_net_quic](clm_net_quic.md), [clm_net_web_client](clm_net_web_client.md)
- **External libraries / platform facilities (C reference):** QUIC/HTTP3 native libraries through the shared client when enabled.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Forces the common HTTP client lifecycle over HTTP/3 using QUIC/TLS with no TCP fallback.

## Capabilities and operations

- Initialize HTTP/3-only configuration.
- Submit requests using the shared callback model.
- Cancel, resume and destroy HTTP/3 client requests.

## Resource, data and ownership model

The application-facing request shape follows the common client; stream and connection management use QUIC as transport.

## Behavioral contract and boundaries

HTTP/3 must be enabled and supported by the implementation. If absent, the operation returns not-supported rather than silently using HTTP/2. There is no implicit retry via another protocol.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Use HTTP/3-only client mode when testing QUIC behavior and performance or connecting to an HTTP/3-restricted endpoint.

## Related CLM modules

[clm_net_quic](clm_net_quic.md), [clm_net_web_client](clm_net_web_client.md).
