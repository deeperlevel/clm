# CLM Net Web HTTP/2

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_net_web](clm_net_web.md)
- **External libraries / platform facilities (C reference):** nghttp2 through the shared server implementation when enabled; TLS backend through clm_tls.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Requires HTTP/2 over TLS while preserving the shared CLM web server routing and middleware model.

## Capabilities and operations

- Initialize HTTP/2-only server configuration.
- Create and start a TLS-bound server that requires ALPN `h2`.
- Register common routes and middleware; stop/destroy with configured drain behavior.

## Resource, data and ownership model

The facade constrains protocol selection of the shared server rather than defining a separate request/response contract.

## Behavioral contract and boundaries

TLS is mandatory. Connections that omit or fail negotiation for `h2` do not fall back to HTTP/1.1. Cleartext h2c is outside scope.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Host an API that requires multiplexed HTTP/2 streams over HTTPS.

## Related CLM modules

[clm_net_web](clm_net_web.md).
