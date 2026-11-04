# CLM Net Web Client HTTP/1.1

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_net_web_client](clm_net_web_client.md)
- **External libraries / platform facilities (C reference):** No mandatory third-party library identified for this module; host-language/platform facilities may be used.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Restricts the common CLM HTTP client lifecycle to HTTP/1.1 over plain TCP or TLS.

## Capabilities and operations

- Initialize a protocol-specific client configuration.
- Create and submit HTTP/1.1 requests.
- Cancel, resume and destroy through the same request/callback contract as the common client.

## Resource, data and ownership model

This facade uses the shared HTTP client request and response data model, but fixes protocol negotiation to HTTP/1.1.

## Behavioral contract and boundaries

HTTPS still requires normal certificate and hostname validation. The facade never silently upgrades to HTTP/2 or HTTP/3; any HTTP/1.1 wire-framing limitation is inherited from the shared client.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Use the HTTP/1.1 client when integrating with a service or proxy that cannot negotiate HTTP/2.

## Related CLM modules

[clm_net_web_client](clm_net_web_client.md).
