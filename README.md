# http-server

A multithreaded... well, currently single-threaded but concurrent, event-driven HTTP/1.1 server written from scratch in Modern C++20 — no frameworks, no Boost.Asio, just raw sockets, `epoll`, and a hand-rolled HTTP parser.

Built to actually understand what's happening under the hood of every web framework: TCP handshakes, non-blocking I/O, partial reads/writes, request framing, and the event loop that ties it all together.

## Why this exists

Most "build your own HTTP server" tutorials stop at a blocking, single-connection toy. This one handles **many concurrent clients on one thread** using `epoll`, correctly deals with **partial TCP reads/writes** (a request or response is never guaranteed to arrive/send in one syscall), and defends against a handful of real-world failure modes — oversized requests, malformed headers, and handler exceptions — that most learning projects skip.

## Features

- **Event-driven I/O with `epoll`** — a single thread handles all client connections without blocking, using the edge/level-triggered readiness model the same way nginx and Redis do at their core
- **Non-blocking sockets** with proper `EAGAIN`/`EWOULDBLOCK` handling on both read and write paths
- **Correct HTTP request framing** — buffers partial reads until `\r\n\r\n` is found, parses `Content-Length` and waits for the full body before dispatching, and supports pipelined requests (multiple requests in one TCP segment)
- **Partial write handling** — large responses that don't send in one `send()` call are buffered and flushed across multiple `EPOLLOUT` events instead of blocking or dropping data
- **Simple router** — register `GET`/`POST` handlers by exact path with lambda-based handlers
- **Exception-safe request handling** — malformed input (bad `Content-Length`, parse failures) returns a proper `400`/`500` response instead of crashing the whole server for every connected client
- **DoS-resistant size limits** — requests with oversized headers (>8 KB) or bodies (>1 MB) are rejected with `431`/`413` and the connection is closed, rather than buffering unboundedly
- **File-based logging** — structured access/error logs with timestamp, level, status code, method, and path

## What it doesn't do (yet)

Being upfront about this matters more than pretending otherwise:

- No thread pool — request *handling* is still single-threaded (I/O is concurrent via `epoll`, but CPU-bound handler logic isn't parallelized yet)
- No static file serving
- No query string parsing or path parameters (`/users/:id`)
- No `Keep-Alive`/idle-connection timeouts
- No TLS (intended to sit behind a reverse proxy like nginx/Caddy for HTTPS in production)
- No automated tests yet

See [Roadmap](#roadmap) below — these are the next things being built.

## Architecture

```
┌─────────────┐
│   main.cpp   │  registers routes, starts the server
└──────┬──────┘
       │
┌──────▼──────┐     ┌──────────┐     ┌─────────────┐
│    Server    │────▶│  Router  │────▶│ HTTPRequest │
│ (epoll loop) │     └──────────┘     │ HTTPResponse│
└──────┬──────┘                       └─────────────┘
       │
┌──────▼────────────┐
│ ClientConnection    │  per-client read/write buffers,
│ (one per socket)    │  tracked in an fd → connection map
└─────────────────────┘
```

- **`Socket`** — RAII wrapper around the listening file descriptor
- **`Server`** — owns the `epoll` instance and the event loop; accepts connections, reads/writes non-blocking sockets, and drives request framing per client
- **`ClientConnection`** — per-connection state: read buffer (incoming bytes, possibly partial), write buffer (outgoing response, possibly partially sent)
- **`HTTPRequest`** — parses a raw request buffer into method/path/version/headers/body
- **`HTTPResponse`** — builds a well-formed HTTP/1.1 response string from status, headers, and body
- **`Router`** — maps `(method, path)` → handler lambda
- **`Logger`** — writes structured log lines to disk

## Getting started

### Prerequisites

- A Linux environment (this uses `epoll`, which is Linux-specific)
- CMake ≥ 3.15
- A C++20-capable compiler (GCC 11+ recommended)

### Build

```bash
git clone https://github.com/<your-username>/http-server.git
cd http-server
mkdir build && cd build
cmake ..
make
```

### Run

```bash
./server
```

The server starts listening on port `8080`. Try it:

```bash
curl http://localhost:8080/
curl http://localhost:8080/about
curl -X POST http://localhost:8080/login -d "username=demo&password=demo"
```

### Defining routes

Routes are registered in `main.cpp`:

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
├── include/           # Header files (interfaces)
│   ├── Server.hpp
│   ├── Socket.hpp
│   ├── ClientConnection.hpp
│   ├── HTTPRequest.hpp
│   ├── HTTPResponse.hpp
│   ├── Router.hpp
│   └── Logger.hpp
├── src/                # Implementation
│   ├── Server.cpp
│   ├── Socket.cpp
│   ├── HTTPRequest.cpp
│   ├── HTTPResponse.cpp
│   ├── Router.cpp
│   ├── Logger.cpp
│   └── main.cpp
├── static/             # Static assets (not yet served)
├── CMakeLists.txt
└── README.md
```

## Roadmap

- [ ] Thread pool for request handling, decoupled from the I/O event loop
- [ ] Static file serving with path-traversal protection
- [ ] Query string parsing and URL decoding
- [ ] Path parameters (`/users/:id`)
- [ ] `Keep-Alive` support and idle-connection timeouts
- [ ] HEAD, PUT, DELETE, PATCH, OPTIONS methods
- [ ] Middleware pipeline (logging, CORS, auth)
- [ ] Unit tests (parser, router) and integration tests (live request/response)
- [ ] Load testing results (throughput/latency under `wrk`/`ab`)
- [ ] Dockerfile
- [ ] CI pipeline that builds and runs tests on every PR (currently CI only deploys on push to `main`)

## Design notes

**Why `epoll` and not `select`/`poll`?** `epoll` scales to a large number of connections without the O(n) per-call overhead of scanning every file descriptor — it's the mechanism nginx, Redis, and most production event loops are built on, and using it here was a deliberate choice to learn that model rather than the simpler-but-less-scalable alternatives.

**Why one thread today?** Correctly handling non-blocking I/O, partial reads/writes, and request framing across `epoll` events was the first problem worth solving in isolation. Thread pooling for handler execution is the next layer, built on top of a foundation that's already correct under partial I/O — trying to parallelize before the single-threaded event loop was solid would have made bugs much harder to isolate.