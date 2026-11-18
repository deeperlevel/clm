# clm_tls_quic

> Language-neutral capability overview. C implementation details are documented separately.

## Purpose

Recordless TLS 1.3 handshake support designed specifically for QUIC transports.

## Behavior and constraints

QUIC integrates TLS at the CRYPTO-frame layer rather than using the TCP TLS record stream. This module provides a caller-driven QUIC server handshake and reports outgoing handshake data, key material, and transport parameters through callbacks. It does not create sockets, threads, or a QUIC transport by itself.

Support is **implementation and platform dependent**. In the supplied C snapshot, native Schannel is used on supported recent Windows systems; other platforms report unsupported. TCP-oriented TLS is not interchangeable with QUIC TLS. Features such as session resumption, early data, and client certificates are not included in that C implementation.

The module allows a QUIC implementation to avoid making OpenSSL a mandatory dependency, without claiming every platform has equivalent native QUIC TLS support.

## Integration

- Related capabilities: TLS 1.3, QUIC connection/transport integration.
- [C reference](../c/tls_quic.md) — signatures, concrete types, backend requirements, and error codes for one implementation.
- Capability availability can vary by language implementation and platform.
