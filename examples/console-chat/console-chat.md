# CLM Console Chat

**CLM Console Chat** is a simple TCP chat application made up of a console server and console clients. Multiple clients can connect, register a name, and exchange text messages through the server. The server can also send messages to clients and disconnect them.

The example demonstrates TCP networking alongside message queues and asynchronous task execution. It uses a simple line-based text protocol, making it suitable for reproducing the same chat behavior across CLM implementations in different programming languages.

## CLM modules demonstrated

- [clm_net](/docs/clm_net.md) — Creates TCP connections and performs nonblocking network I/O and readiness polling.
- [clm_channel](/docs/clm_channel.md) — Transfers message identifiers between concurrent execution contexts.
- [clm_async](/docs/clm_async.md) — Provides cooperative task execution concepts used by the server.
- [clm_async_pool](/docs/clm_async_pool.md) — Runs the server's asynchronous message-routing task.
- [clm_string](/docs/clm_string.md) — Stores ID-based text such as connected usernames.
- [clm_id](/docs/clm_id.md) — Identifies shared CLM-managed resources.
- [clm_bpa](/docs/clm_bpa.md) — Supports the underlying ID-based storage.
- [clm_dictionary](/docs/clm_dictionary.md) — Supports structured option data.
- [clm_clp](/docs/clm_clp.md) — Demonstrates integration with command-line parsing.
