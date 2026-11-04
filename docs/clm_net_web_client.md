# CLM Net Web Client

> **Current interface reference** · Language-neutral contract draft · Based on C headers supplied 2026-11-05

## Dependencies

- **CLM modules (current C reference):** [clm_http_utils](clm_http_utils.md), [clm_net](clm_net.md), [clm_tls](clm_tls.md)
- **External libraries / platform facilities (C reference):** nghttp2/QUIC backend dependencies as enabled; OpenSSL or platform TLS through transport.

**Portability rule:** The listed C dependencies describe the reference implementation. A language port may use native libraries or an equivalent design as long as it preserves the language-neutral observable contract.

## Purpose

Exposes asynchronous HTTP requests with HTTP/1.1, HTTP/2 and HTTP/3 protocol selection via a common callback-and-streaming contract.

## Capabilities and operations

- Configure a client and submit multiple requests with stable request IDs.
- Choose a fixed protocol or an automatic setup strategy.
- Stream an outgoing body through an upload callback.
- Receive final response headers and body chunks through callbacks.
- Pause/resume application upload/download, cancel requests and shut down the client.

## Resource, data and ownership model

The client owns connection management and request state. A successfully submitted request must generate exactly one completion callback, including cancellation and destruction. Callback metadata/data views are borrowed only during the callback. Request IDs are identifiers for client requests, not socket handles.

## Behavioral contract and boundaries

A successful transport completion can carry any HTTP status, including 4xx/5xx. `AGAIN` from an upload/data callback indicates an application pause with zero bytes accepted for that callback. The current AUTO strategy attempts HTTP/3 for secure requests when compiled in, then negotiates HTTP/2 or HTTP/1.1; fallback is setup-only and must not follow certificate validation failures or a request already transmitted. Body transfer decoding is separate from content-encoding decompression.

## Cross-language interoperability

Language ports should preserve data meaning, resource lifetime, successful outcomes and distinguishable failure conditions. Native type systems, threading primitives, platform libraries, error conventions and allocation strategies are not required to match the C implementation. An API name shown in the C reference is evidence of a capability; it is not a mandatory signature in other languages.

## Example use case

Submit a GET request, stream response body chunks into a file, and resume a paused consumer when the file writer is ready.

## Related CLM modules

[clm_http_utils](clm_http_utils.md), [clm_net](clm_net.md), [clm_tls](clm_tls.md).
