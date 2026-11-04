# CLM Net Web HTTP/3

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_net_quic](clm_net_quic.md), [clm_net_web](clm_net_web.md)
- **External libraries / platform facilities (C reference):** ngtcp2 / nghttp3 / OpenSSL QUIC through enabled QUIC and HTTP/3 backend.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Runs the CLM web application routing and handler model over HTTP/3 using QUIC and UDP.

## Capabilities and operations

- Initialize HTTP/3 config with PEM credentials and transport/stream limits.
- Create a server and register shared CLM web routes or middleware.
- Bind/start an HTTP/3 UDP listener; inspect its local address.
- Stop gracefully with a GOAWAY/drain policy and destroy resources.

## Resource, data and ownership model

The HTTP/3 facade reuses the web request/response abstractions but runs an independent UDP/QUIC reactor. It is not a TLS-wrapped TCP listener. Request bodies are bounded and buffered before ordinary route dispatch; WebSocket CONNECT tunnels have a separate incremental path.

## Behavioral contract and boundaries

Requires QUIC support and valid credentials. There is no HTTP/1 fallback, TCP listener, server push or 0-RTT in the reference. Stop semantics rely on cooperative callback return and a bounded drain period. A backend lacking QUIC must explicitly report unsupported behavior.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Expose the same application routes over HTTP/3 alongside a separately bound HTTP/1.1 or HTTP/2 service.

## Related CLM modules

[clm_net_quic](clm_net_quic.md), [clm_net_web](clm_net_web.md).
