# http-server

A multithreaded, event-driven HTTP/1.1 server written from scratch in Modern C++20 — no frameworks, no Boost.Asio, just raw sockets, `epoll`, a hand-rolled HTTP parser, and a worker thread pool.

Built to actually understand what's happening under the hood of every web framework: TCP handshakes, non-blocking I/O, partial reads/writes, request framing, concurrent request handling, and the event loop that ties it all together.

## Why this exists

Most "build your own HTTP server" tutorials stop at a blocking, single-connection toy. This one handles **many concurrent clients on one I/O thread** using `epoll`, hands CPU-bound request parsing and routing off to a **worker thread pool** without ever touching shared connection state from more than one thread, correctly deals with **partial TCP reads/writes**, and defends against a handful of real-world failure modes — oversized requests, malformed headers, handler exceptions, and path-traversal attacks on static files — that most learning projects skip.

## Features

- **Event-driven I/O with `epoll`** — one thread accepts and drives all client sockets without blocking, using the same readiness-notification model nginx and Redis are built on
- **Non-blocking sockets** with proper `EAGAIN`/`EWOULDBLOCK` handling on both read and write paths
- **Worker thread pool for request handling** — parsing, routing, and static file lookups run off the I/O thread; completed responses are handed back to the main thread through a lock-protected result queue and an `eventfd`-based wake-up, so `epoll_wait` never busy-polls for finished work
- **Correct HTTP request framing** — buffers partial reads until `\r\n\r\n` is found, parses `Content-Length` and waits for the full body before dispatching, and supports pipelined requests (multiple requests in one TCP segment)
- **Partial write handling** — large responses that don't send in one `send()` call are buffered and flushed across multiple `EPOLLOUT` events instead of blocking or dropping data
- **Static file serving** — any file under the configured web root is served with the correct `Content-Type` inferred from its extension, with `try_files`-style fallback (`/about` resolves to `about.html`, `/` resolves to `index.html`, directory requests fall back to `index.html` inside them) — the same convention nginx and Express use, so dropping HTML/CSS/JS files into the static folder is enough, no per-page route needed
- **Path-traversal protection** — every resolved file path is canonicalized and verified to still live inside the web root, component-by-component, before being opened; `../../etc/passwd`-style requests return `403`, not file contents
- **Simple router** — register `GET`/`POST` handlers by exact path with lambda-based handlers, checked before the static file fallback so dynamic routes can override static pages
- **Exception-safe request handling** — malformed input (bad `Content-Length`, parse failures, handler exceptions) returns a proper `400`/`500` response instead of crashing the server for every connected client
- **DoS-resistant size limits** — requests with oversized headers (>8 KB) or bodies (>1 MB) are rejected with `431`/`413` and the connection is closed, rather than buffering unboundedly
- **Thread-safe file-based logging** — structured access/error logs with timestamp, level, status code, method, and path, mutex-protected so concurrent worker threads never interleave writes

## What it doesn't do (yet)

Being upfront about this matters more than pretending otherwise:

- No query string parsing or path parameters (`/users/:id`) in the router
- No `Keep-Alive`/idle-connection timeouts — connections aren't proactively closed if a client goes silent
- No HEAD, PUT, DELETE, PATCH, OPTIONS support — GET/POST only
- No TLS (intended to sit behind a reverse proxy like nginx/Caddy for HTTPS in production)
- No automated tests yet
- Static file lookups re-read from disk on every request — no in-memory caching for frequently-served files

See [Roadmap](#roadmap) below — these are the next things being built.

## Architecture

```
┌─────────────┐
│   main.cpp  │  registers routes, starts the server
└──────┬──────┘
       │
┌──────▼───────────────────────────────────────────────┐
│                        Server                        │
│              (epoll loop — I/O thread only)          │
│                                                      │
│  accept() ── recv()/send() ── frame requests         │
│       │                              │               │
│       │                    enqueue(requestData, fd)  │
│       │                              ▼               │
│       │                    ┌──────────────────┐      │
│       │                    │    ThreadPool    │      │
│       │                    │  parse → route → │      │
│       │                    │  static fallback │      │
│       │                    └────────┬─────────┘      │
│       │                             │ push result    │
│       │                    ┌────────▼──────────┐     │
│       └─ eventfd wakeup ◀─│    ResultQueue    │     │
│              │             └───────────────────┘     │
│              ▼                                       │
│     writeBuffer += response, enable EPOLLOUT         │
└──────────────────────────────────────────────────────┘
```

- **`Socket`** — RAII wrapper around the listening file descriptor
- **`Server`** — owns the `epoll` instance and the event loop; accepts connections, reads/writes non-blocking sockets, frames requests, and dispatches finished work back to clients. `clients`, `epoll_ctl`, and all connection state are touched only from this thread.
- **`ClientConnection`** — per-connection state: read buffer (incoming bytes, possibly partial), write buffer (outgoing response, possibly partially sent), close-after-write flag
- **`ThreadPool`** — a fixed set of worker threads pulling parse/route work off a shared task queue; workers never touch `clients` or epoll directly
- **`ResultQueue`** — mutex-protected handoff from worker threads back to the I/O thread; paired with an `eventfd` so the main `epoll_wait` wakes immediately when a result is ready, instead of polling
- **`HTTPRequest`** — parses a raw request buffer into method/path/version/headers/body
- **`HTTPResponse`** — builds a well-formed HTTP/1.1 response string from status, headers, and body
- **`Router`** — maps `(method, path)` → handler lambda for dynamic routes
- **`StaticFileHandler`** — resolves a request path against a web root using `try_files`-style conventions, with path-traversal protection on every candidate
- **`Logger`** — thread-safe, writes structured log lines to disk
- **`EpollEventLoop`** — thin wrapper around `epoll_create`/`epoll_ctl`/`epoll_wait` that exposes a small `registerFD`/`changeFdMode`/`removeFD`/`getEvents` API instead of raw epoll calls, so `Server` deals in `IOEvent`s rather than `epoll_event` structs directly
- **`ServerConfig`** — plain struct holding port, worker thread count, max epoll events, header/body size limits, log file paths, and the static file root; built via a `ServerConfigBuilder` fluent API instead of being hardcoded

## Getting started

### Prerequisites

- A Linux environment (this uses `epoll`, which is Linux-specific)
- CMake ≥ 3.15
- A C++20-capable compiler (GCC 13+ recommended — earlier versions may lack full `<format>`/`<filesystem>` support used here)

### Build

```bash
git clone https://github.com/prathamGiri/http-server.git
cd http-server
mkdir build && cd build
cmake ..
make
```

### Configuration

Server settings are assembled in `main.cpp` via `ServerConfigBuilder` rather than hardcoded — port, worker thread count, max epoll events per `epoll_wait` call, header/body size limits, log file paths, and the static file root all go through it:

```cpp
ServerConfig config = ServerConfigBuilder()
    .withPort(8080)
    .withMaxThreads(4)
    .withMaxEpollEvents(10)
    .withServerLogFile("/var/log/http-server/ServerLogs.log")
    .withClientLogFile("/var/log/http-server/ClientLogs.log")
    .withStaticFileDir("../static")
    .build();
```

The default log paths point at `/var/log/http-server/`, so create that directory (or point the builder somewhere writable) before running:

```bash
sudo mkdir -p /var/log/http-server && sudo chown $USER /var/log/http-server
```

### Run

```bash
./server
```

The server starts listening on port `8080` (or whatever `.withPort(...)` was set to). Try it:

```bash
curl http://localhost:8080/                          # serves static/index.html
curl http://localhost:8080/about                      # resolves to static/about.html
curl http://localhost:8080/static/style.css            # any static asset
curl http://localhost:8080/static/../../../etc/passwd  # 403, blocked
curl -X POST http://localhost:8080/login -d "username=demo&password=demo"
```

### Serving static pages

Drop HTML/CSS/JS/image files into the `static/` directory — no route registration needed. `/about` will resolve to `static/about.html`, `/` resolves to `static/index.html`, and `/blog/` resolves to `static/blog/index.html`, following the same convention nginx's `try_files`/`index` directives use.

### Defining dynamic routes

Routes are registered in `main.cpp` and checked before the static fallback:

```cpp
router.get("/", [](const HTTPRequest&) {
    HTTPResponse response;
    response.setStatus(200, "OK");
    response.setHeader("Content-Type", "text/plain");
    response.setBody("Hello, world!");
    return response;
});
```

## Project structure

```
http-server/
├── include/
│   ├── concurrency/
│   │   ├── ResultQueue.hpp
│   │   └── ThreadPool.hpp
│   ├── core/
│   │   ├── HTTPRequest.hpp
│   │   └── HTTPResponse.hpp
│   ├── logger/
│   │   └── Logger.hpp
│   ├── net/
│   │   ├── ClientConnection.hpp
│   │   ├── EpollEventLoop.hpp
│   │   └── Socket.hpp
│   ├── routing/
│   │   ├── Router.hpp
│   │   └── StaticFileHandler.hpp
│   └── server/
│       ├── Server.hpp
│       └── ServerConfig.hpp
├── src/
│   ├── concurrency/
│   │   └── ThreadPool.cpp
│   ├── core/
│   │   ├── HTTPRequest.cpp
│   │   └── HTTPResponse.cpp
│   ├── logging/
│   │   └── Logger.cpp
│   ├── net/
│   │   ├── EpollEventLoop.cpp
│   │   └── Socket.cpp
│   ├── routing/
│   │   ├── Router.cpp
│   │   └── StaticFileHandler.cpp
│   ├── server/
│   │   ├── Server.cpp
│   │   └── ServerConfig.cpp
│   └── main.cpp
├── static/                  # Static site root — drop HTML/CSS/JS/images here
│   ├── css/style.css
│   └── index.html
├── .github/workflows/       # CI: deploys to a self-hosted VM on push to main
├── CMakeLists.txt
└── README.md
```

## Roadmap

- [ ] Query string parsing and URL decoding
- [ ] Path parameters in the router (`/users/:id`)
- [ ] `Keep-Alive` support and idle-connection timeouts
- [ ] HEAD, PUT, DELETE, PATCH, OPTIONS methods
- [ ] Middleware pipeline (logging, CORS, auth) applied before route/static dispatch
- [ ] Generation-tagged connection identity to eliminate a known file-descriptor-reuse race between worker results and closed/recycled connections
- [ ] In-memory caching for frequently-served static files
- [ ] Unit tests (parser, router, static file resolution) and integration tests (live request/response, concurrent load)
- [ ] Load testing results (throughput/latency under `wrk`/`ab`, before/after thread pool comparison)
- [ ] Dockerfile
- [ ] CI pipeline that builds and runs tests on every PR (currently `.github/workflows/deploy.yml` only pulls, rebuilds, and restarts on a self-hosted VM on push to `main` — no test/build gate on PRs)

## Design notes

**Why `epoll` and not `select`/`poll`?** `epoll` scales to a large number of connections without the O(n) per-call overhead of scanning every file descriptor — it's the mechanism nginx, Redis, and most production event loops are built on, and using it here was a deliberate choice to learn that model rather than the simpler-but-less-scalable alternatives.

**Why split I/O and request handling across threads the way it's done here?** All `epoll_ctl` calls and all mutation of the client-connection map happen exclusively on the I/O thread. Worker threads only ever operate on a self-contained copy of the raw request bytes and push a plain result struct back through a mutex-protected queue — they never reach into shared connection state directly. This avoids a whole class of data races that a naive "just call the handler in a new thread" approach would introduce, at the cost of a small amount of copying and a queue hop.

**Why `try_files`-style fallback instead of exact-path-only static serving?** Real static-file servers don't register a route per page — they apply a small, fixed set of filename conventions and check the filesystem, which is why dropping a new HTML file into a directory "just works" with no server restart or config change. Implementing that convention here, rather than hardcoding per-file routes, is what makes the static handler behave the way `nginx`/`Express`'s static middleware do.

**Path traversal defense:** resolved paths are compared to the web root component-by-component (not via string-prefix matching), which correctly rejects sibling directories that happen to share a string prefix (e.g. `static-backup/` vs `static/`) — a naive `starts_with` check would wrongly allow that case through.

## License

MIT — feel free to use this as a learning reference or a starting point for your own server.
