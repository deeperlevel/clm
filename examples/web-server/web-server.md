# CLM Web Server

**CLM Web Server** is a tiny HTTP/1.1 web server that serves a local `index.html` page. It provides a minimal demonstration of creating and running a web server using CLM.

The example loads the page at startup and serves it for `GET /` and `GET /index.html`. It also demonstrates route registration, request middleware, response headers (including `Server: clm net/web`), optional gzip support, and server startup and shutdown. Other HTTP methods are rejected with a `405 Method Not Allowed` response.

## CLM modules demonstrated

- [clm_net_web](/docs/clm_net_web.md) — Provides the HTTP server, routes, middleware, request handling, and responses.
- [clm_net](/docs/clm_net.md) — Initializes networking and configures the listening address.
- [clm_http_utils](/docs/clm_http_utils.md) — Provides HTTP-related text utilities, including method comparison.

The web-server module also uses supporting CLM capabilities internally, including [clm_async_pool](/docs/clm_async_pool.md) for worker execution and [clm_compress](/docs/clm_compress.md) for compression support.
