# CLM Net Web Client HTTP/2

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_net_web_client](clm_net_web_client.md)
- **External libraries / platform facilities (C reference):** nghttp2 and TLS backend through the shared client when enabled.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Forces the common HTTP client contract to use HTTP/2 with verified TLS and ALPN negotiation.

## Capabilities and operations

- Initialize an HTTP/2-specific configuration.
- Create, submit, cancel, pause/resume and destroy client requests.
- Use shared final-response and streamed-body callbacks.

## Resource, data and ownership model

The facade is a protocol restriction, not a separate application HTTP message model.

## Behavioral contract and boundaries

Negotiated `h2` is mandatory. No cleartext h2c support or HTTP/1.1 fallback is part of this facade. A peer that does not negotiate HTTP/2 must cause a distinguishable failure.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Connect to a service that explicitly requires HTTP/2 and stream results through a callback.

## Related CLM modules

[clm_net_web_client](clm_net_web_client.md).
