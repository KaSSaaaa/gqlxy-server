---
title: Standalone Server
---

GQLXY includes an opt-in HTTP/WebSocket/SSE server backed by [Oat++](https://oatpp.io/). It exposes all four GraphQL transports on a single endpoint — similar to Apollo Server's `startStandaloneServer`.

## Prerequisites

The standalone server requires the `standalone-server` vcpkg feature. This is enabled by default in the CMake presets. When building manually, add:

```bash
cmake -DBUILD_STANDALONE_SERVER=ON -DVCPKG_MANIFEST_FEATURES="standalone-server" ...
```

## Quick start

```cpp
#include <gqlxy/schema.h>
#include <gqlxy/server/standalone_server.h>

using namespace gqlxy;
using namespace gqlxy::server;

int main() {
    Schema schema({
        .typeDefs  = R"(
            type Query {
                hello: String
            }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"hello", "Hello, world!"}
            }}
        }
    });

    StandaloneServer server({
        .schema = schema,
        .port = 4000
    });

    std::cout << "🚀 Server ready at " << server.GetUrl() << std::endl;
    server.Start();
}
```

### Non-blocking usage

```cpp
StandaloneServer server({
    .schema = schema,
    .port = 4000
});
server.StartAsync();
// ... do other work ...
server.Stop();
```

## Supported transports

All transports are served on the same path (default `/graphql`).

### HTTP

- `POST /graphql` — `application/json` body with `query`, `variables`, `operationName`
- `GET /graphql?query=...` — for read-only operations
- Response `Content-Type: application/graphql-response+json`
- CORS preflight (`OPTIONS`) handled automatically

### WebSocket — `graphql-transport-ws`

The modern protocol used by Apollo Client 3+, Relay, and Insomnia.

Messages: `connection_init` / `connection_ack`, `subscribe`, `next`, `error`, `complete`, `ping` / `pong`.

### WebSocket — `graphql-ws` (legacy)

The legacy `subscriptions-transport-ws` protocol.

Messages: `connection_init` / `connection_ack`, `start`, `data`, `stop`, `complete`, `connection_terminate`.

The protocol is auto-detected from the first message (`start` → legacy, `subscribe` → modern).

### Server-Sent Events (SSE)

Streaming via `Accept: text/event-stream` on the same path. Works through firewalls and proxies that block WebSockets. Events: `connection_ack`, `next`, `complete`.
