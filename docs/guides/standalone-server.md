# Standalone Server

GQLXY includes an opt-in standalone HTTP/WebSocket/SSE server backed by [oatpp](https://oatpp.io/). It serves all four GraphQL transports on a single port and path — similar to Apollo Server's `startStandaloneServer`.

## Prerequisites

The standalone server requires the `standalone-server` vcpkg feature. This is enabled by default in the CMake presets. If building manually, add:

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
        .typeDefs = R"(
            type Query { hello: String }
        )",
        .resolvers = {
            {"Query", Resolver{{"hello", "Hello, world!"}}}
        }
    });

    StandaloneServer server({.schema = schema, .port = 4000});

    std::cout << "🚀 Server ready at " << server.GetUrl() << std::endl;
    server.Start(); // blocks
}
```

## StandaloneServerOptions

| Field | Type | Default | Description |
|---|---|---|---|
| `schema` | `Schema&` | *(required)* | The schema to serve |
| `host` | `std::string` | `"0.0.0.0"` | Bind address |
| `port` | `uint16_t` | `4000` | Port number |
| `path` | `std::string` | `"/graphql"` | Endpoint path |

## API

| Method | Description |
|---|---|
| `Start()` | Starts the server and **blocks** the calling thread |
| `StartAsync()` | Starts the server in the background and returns immediately |
| `Stop()` | Gracefully shuts down the server |
| `GetUrl()` | Returns the base URL (e.g., `http://0.0.0.0:4000/graphql`) |

### Non-blocking usage

```cpp
StandaloneServer server({.schema = schema, .port = 4000});
server.StartAsync();

// Server is running in the background
std::cout << "Server at " << server.GetUrl() << std::endl;

// ... do other work ...

server.Stop();
```

## Supported transports

All transports are served on the same path (default: `/graphql`).

### HTTP

- **`POST /graphql`** with `application/json` body containing `query`, `variables`, and `operationName`
- **`GET /graphql?query=...`** for read-only operations
- Response `Content-Type: application/graphql-response+json`
- CORS preflight (`OPTIONS`) handled automatically

### WebSocket: `graphql-transport-ws`

The modern WebSocket subprotocol used by Apollo Client 3+, Relay, and Insomnia.

Messages: `connection_init` / `connection_ack`, `subscribe`, `next`, `error`, `complete`, `ping` / `pong`.

### WebSocket: `graphql-ws` (legacy)

The legacy `subscriptions-transport-ws` protocol.

Messages: `connection_init` / `connection_ack`, `start`, `data`, `stop`, `complete`, `connection_terminate`.

The protocol is auto-detected from the first operation message (`start` → legacy, `subscribe` → modern).

### Server-Sent Events (SSE)

Streaming via `Accept: text/event-stream` on the same path (distinct-connections mode). Events: `connection_ack`, `next`, `complete`.

Works without WebSocket support (firewalls, proxies).

## Full example: Books & Reviews API

The `samples/demo-server/` directory contains a fully wired application with queries, mutations, and subscriptions:

```cpp
#include <gqlxy/pubsub.h>
#include <gqlxy/schema.h>
#include <gqlxy/server/standalone_server.h>

using namespace gqlxy;
using namespace gqlxy::server;

int main() {
    PubSub pubsub;

    Schema schema({
        .typeDefs = R"(
            type Book { id: ID! title: String! author: String! year: Int! }
            type Query { books: [Book!]! }
            type Mutation { addBook(title: String!, author: String!, year: Int!): Book! }
            type Subscription { bookAdded: Book! }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"books", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
                    // return books from your data store
                    return std::vector<ValueResolver>{};
                }}}
            }},
            {"Mutation", Resolver{
                {"addBook", FunctionResolver{[&pubsub](const ResolverArgs& args) -> ValueResolver {
                    auto book = Resolver{
                        {"id", "1"},
                        {"title", args.Args()["title"].get<std::string>()},
                        {"author", args.Args()["author"].get<std::string>()},
                        {"year", args.Args()["year"].get<int>()}
                    };
                    pubsub.Publish("BOOK_ADDED", book);
                    return book;
                }}}
            }},
            {"Subscription", Resolver{
                {"bookAdded", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                    return pubsub.AsyncIterator({"BOOK_ADDED"});
                }}}
            }}
        }
    });

    StandaloneServer server({.schema = schema, .port = 4000});
    std::cout << "🚀 Server ready at " << server.GetUrl() << std::endl;
    server.Start();
}
```

Test with any GraphQL client (Insomnia, Postman, Apollo Studio) pointing at `http://localhost:4000/graphql`.
