---
title: MCP Server
---

GQLXY can expose your GraphQL schema as an [MCP (Model Context Protocol)](https://modelcontextprotocol.io/) server alongside the standard GraphQL endpoint. AI agents and LLM toolchains that speak MCP can then query and mutate your graph directly, without any additional infrastructure.

The MCP server is part of the `standalone-server` feature and is opt-in via `McpServerOptions`.

## Prerequisites

The MCP server requires the `standalone-server` vcpkg feature:

```json
{
  "dependencies": [
    { 
      "name": "gqlxy-server",
      "features": ["standalone-server"]
    }
  ]
}
```

## Quick start

Pass `McpServerOptions` when constructing `StandaloneServer`:

```cpp
#include <gqlxy/server/schema.h>
#include <gqlxy/server/standalone/standalone_server.h>

using namespace gqlxy;
using namespace gqlxy::server;

int main() {
    Schema schema({
        .typeDefs = R"(
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
        .port = 4000,
        .path = "/graphql",
        .mcp = McpServerOptions{
            .path = "/mcp",
            .policy = DefaultMcpPolicy::Enabled
        }
    });

    server.Start();
}
```

The MCP endpoint is now reachable at `http://localhost:4000/mcp` via SSE, and the GraphQL endpoint remains at `http://localhost:4000/graphql`.

## Default tool generation

When `mcp` is set, GQLXY automatically creates one MCP tool per root field on `Query`, `Mutation`, and `Subscription`. The `policy` field controls their default visibility:

| Value | Behaviour |
|---|---|
| `DefaultMcpPolicy::Enabled` | All auto-generated tools are listed and callable. |
| `DefaultMcpPolicy::Hidden` | Tools are callable but not advertised in the tool list. |
| `DefaultMcpPolicy::Disabled` | No auto-generated tools. Use `additionalTools` to register tools manually. |

## Custom tools

Register additional tools alongside the auto-generated ones via `additionalTools`:

```cpp
#include <gqlxy/server/mcp/mcp_tool.h>

.mcp = McpServerOptions{
    .path = "/mcp",
    .policy = DefaultMcpPolicy::Enabled,
    .additionalTools = {
        mcp::McpTool{
            .name = "ping",
            .description = "Check server liveness",
            .handler = [](const nlohmann::json&) {
                return SubscriptionHandle::SingleShot({
                    {"status", "ok"}
                });
            }
        }
    }
}
```

### `McpTool` fields

| Field | Type | Description |
|---|---|---|
| `name` | `string` | Tool identifier exposed to the MCP client. |
| `description` | `optional<string>` | Human-readable description shown in the tool list. |
| `args` | `vector<McpToolArg>` | Input argument schema (see below). |
| `handler` | `function<SubscriptionHandle(const nlohmann::json& args)>` | Called when the tool is invoked. |

### `McpToolArg` fields

| Field | Type | Description |
|---|---|---|
| `name` | `string` | Argument name. |
| `description` | `optional<string>` | Argument description. |
| `jsonSchemaType` | `string` | JSON Schema primitive type (`"string"`, `"integer"`, `"boolean"`, etc.). |
| `jsonSchemaItemType` | `optional<string>` | Element type for `"array"` arguments. |
| `required` | `bool` | Whether the argument must be provided. |

### Returning results from a handler

Handlers return a `SubscriptionHandle`. For a simple one-shot response, use `SubscriptionHandle::SingleShot`:

```cpp
.handler = [](const nlohmann::json& args) {
    auto name = args.value("name", "world");
    return SubscriptionHandle::SingleShot({
        {"message", "Hello, " + name + "!"}
    });
}
```

For streaming tools, return a handle backed by a `PubSub` async iterator — the same mechanism used by GraphQL subscriptions.

## Transport

The MCP server uses **Server-Sent Events (SSE)** as its transport, served on the path specified in `McpServerOptions::path`. The same `StandaloneServer` instance handles both GraphQL and MCP traffic.
