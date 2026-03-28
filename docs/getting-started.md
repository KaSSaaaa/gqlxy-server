# Getting Started

This guide walks you through installing GQLXY, building your first GraphQL schema, and executing queries.

## Prerequisites

- A C++20-capable compiler (Clang 14+, GCC 12+, MSVC 19.30+)
- [CMake](https://cmake.org/) 3.10+
- [Ninja](https://ninja-build.org/) (recommended) or another CMake generator

Dependencies are managed automatically via [vcpkg](https://vcpkg.io/) — no manual installation required.

## Installation

### Clone the repository

```bash
git clone https://github.com/KaSSaaaa/gqlxy.git
cd gqlxy
```

### Build

GQLXY ships CMake presets for every major platform. Pick the one that matches your system:

| Platform | Preset |
|---|---|
| macOS Apple Silicon | `arm64-osx-debug` / `arm64-osx-release` |
| macOS Intel | `x64-osx-debug` / `x64-osx-release` |
| Linux x64 | `x64-linux-debug` / `x64-linux-release` |
| Windows x64 | `x64-windows-debug` / `x64-windows-release` |

```bash
# Configure (downloads dependencies via vcpkg automatically)
cmake --preset arm64-osx-debug

# Build
cmake --build out/build/arm64-osx-debug
```

### Run the tests

```bash
ctest --test-dir out/build/arm64-osx-debug --output-on-failure
```

## Building your first schema

A minimal GQLXY schema needs two things: **type definitions** and **resolvers**.

### Type definitions
Like every GraphQL server, GQLXY uses a **schema** to define the shape of the data that a client can query.

You can declare your schema inline in your C++ code or read it from a file.

```cpp
// inline
string typeDefs = R"(
    type Query {
        hello: String
    }
)"

// from a file
ifstream schemaFile("schema.graphql");
ostringstream ss;
ss << schemaFile.rdbuf();
string typeDefs = ss.str();
```

### Resolvers
Resolvers are a way to expose your data to clients. It simply says how your data is going to be fetched, transformed, and sent to your clients. They will simply match your schema.

From the type definition above, we can have the following resolver :
```cpp
Resolver resolver = {
    {"Query", Resolver{
        {"hello", "Hello, world!"}
    }}
}
```
This will tell the engine, when asked for **hello**, return "Hello, world!".

There are also other types of resolvers :

```cpp
// Static values
{"name", "Alice"}                         // string
{"age", 30}                               // int
{"score", 9.5}                            // double
{"active", true}                          // bool
{"deletedAt", std::nullopt}               // null

// Nested objects
{"user", Resolver{
    {"id", "1"},
    {"name", "Alice"}
}}

// Lists
{"tags", std::vector<ValueResolver>{"graphql", "cpp", "gqlxy"}}
```

For dynamic resolution, use one of the four function resolver types:

```cpp
// FunctionResolver - returns data synchronously
{"user", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
    auto id = args.Args()["id"].get<std::string>();
    return Resolver{
        {"id", id},
        {"name", "Alice"}
    };
}}}

// AsyncFunctionResolver — returns std::future
{"user", AsyncFunctionResolver{[](const ResolverArgs& args) -> std::future<ValueResolver> {
    return std::async(std::launch::async, [&]() -> ValueResolver {
        return Resolver{
            {"id", "1"},
            {"name", "Alice"}
        };
    });
}}}

// CoroutineResolver — Using C++20 coroutines
{"user", CoroutineResolver{[](const ResolverArgs&) -> Task<ValueResolver> {
    co_return Resolver{
        {"id", "1"},
        {"name", "Alice"}
    };
}}}

// CallbackResolver — for callback-based APIs
{"user", CallbackResolver{[](const ResolverArgs&, const std::function<void(const ValueResolver&)>& cb) {
    cb(Resolver{
        {"id", "1"},
        {"name", "Alice"}
    });
}}}
```

### Complete example

```cpp
#include <gqlxy/schema.h>
#include <iostream>

using namespace gqlxy;

int main() {
    Schema schema({
        //Can be read from a file, or inline like this
        .typeDefs = R"(
            type Query {
                hello: String
            }
        )",
        //A map of how your schema is going to be resolved
        .resolvers = {
            {"Query", Resolver{
                {"hello", "Hello, world!"}
            }}
        }
    });

    auto result = schema.Resolve({
        .query = "{ hello }"
    }).get();

    std::cout << result.data.value().dump(2) << std::endl;
    // Output: { "hello": "Hello, world!" }
}
```

# Standalone server

GQLXY includes an opt-in standalone HTTP/WebSocket/SSE server backed by [Oat++](https://oatpp.io/). It serves all four GraphQL transports on a single port and path — similar to Apollo Server's `startStandaloneServer`.

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
    server.Start(); // blocks
}
```

<!-- TODO Add a "Execute your first query" -->

[API References](./api-reference.md)

<!-- TODO From there everything should be moved elsewhere

## Executing queries

Call `Schema::Resolve()` with a `SchemaResolveArgs` struct and `.get()` the coroutine result:

```cpp
auto result = schema.Resolve({
    .query = "query GetUser($id: ID!) { user(id: $id) { name } }",
    .variables = {{"id", "42"}},       // nlohmann::json object
    .operationName = "GetUser"         // optional, needed if document has multiple operations
}).get();
```

The returned `ResolveResult` contains:

- `data` — `std::optional<nlohmann::json>` with the query result
- `errors` — `std::optional<FieldErrors>` with any errors that occurred

## Field arguments

Arguments defined in the SDL are automatically parsed and available via `args.Args()`:

```cpp
Schema schema({
    .typeDefs = R"(
        type Query {
            greet(name: String!): String
        }
    )",
    .resolvers = {
        {"Query", Resolver{
            {"greet", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
                return "Hello, " + args.Args()["name"].get<std::string>() + "!";
            }}}
        }}
    }
});

auto result = schema.Resolve({
    .query = R"({ greet(name: "GQLXY") })"
}).get();
// result.data → { "greet": "Hello, GQLXY!" }
```

## Variables

Query variables are substituted automatically from `SchemaResolveArgs::variables`:

```cpp
auto result = schema.Resolve({
    .query = R"(
        query GetUser($id: ID!) {
            user(id: $id) { name email }
        }
    )",
    .variables = {{"id", "42"}}
}).get();
```

## Returning null

Use any of these to represent GraphQL `null`:

```cpp
return std::nullopt;
return nullptr;
return std::monostate{};
```

## Error handling

If a resolver throws, GQLXY catches the exception, sets the field to `null`, and appends a structured error with `message` and `path` to the `errors` array. Other fields continue resolving normally.

```cpp
{"user", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
    throw std::runtime_error("User not found");
}}}
```

```json
{
  "data": {
    "user": null
  },
  "errors": [
    {
      "message": "User not found",
      "path": [
        "user"
      ]
    }
  ]
}
```

## Context

Pass request-scoped state (auth tokens, DB connections, etc.) via `context`:

```cpp
struct RequestContext {
    std::string authToken;
    DatabaseConnection& db;
};

RequestContext ctx{.authToken = "Bearer ...", .db = db};

auto result = schema.Resolve({
    .query = "{ me { name } }",
    .context = ctx
}).get();
```

Access it inside resolvers:

```cpp
{"me", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
    auto& ctx = args.Context<RequestContext>();
    auto user = ctx.db.findUserByToken(ctx.authToken);
    return Resolver{{"name", user.name}};
}}}
```

## Next steps

- [Resolvers in depth](guides/resolvers.md) — interfaces, unions, type resolution, aliases, fragments
- [Subscriptions](guides/subscriptions.md) — real-time event streaming with PubSub
- [Directives & Custom Scalars](guides/directives-and-scalars.md) — `@skip`, `@include`, custom directives, scalar types
- [Schema Stitching](guides/schema-stitching.md) — merge multiple schemas
- [Apollo Federation](guides/federation.md) — act as a federated subgraph
- [Standalone Server](guides/standalone-server.md) — HTTP, WebSocket, and SSE transport
- [API Reference](api-reference.md) — full type and function reference
 -->