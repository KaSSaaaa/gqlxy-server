# Schema Stitching

Schema stitching lets you merge multiple independent `Schema` instances into a single unified schema via `Schema::Stitch()`.

## Basic usage

Create separate schemas and stitch them together:

```cpp
#include <ariane/schema.h>

using namespace ariane::graphql;

Schema usersSchema({
    .typeDefs = R"(
        type Query { user(id: ID!): User }
        type User { id: ID name: String email: String }
    )",
    .resolvers = {
        {"Query", Resolver{
            {"user", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                return Resolver{
                    {"id", r.Args()["id"].get<std::string>()},
                    {"name", "Alice"},
                    {"email", "alice@example.com"}
                };
            }}}
        }}
    }
});

Schema postsSchema({
    .typeDefs = R"(
        type Query { post(id: ID!): Post }
        type Post { id: ID title: String authorId: ID }
    )",
    .resolvers = {
        {"Query", Resolver{
            {"post", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                return Resolver{
                    {"id", r.Args()["id"].get<std::string>()},
                    {"title", "Hello Ariane"},
                    {"authorId", "1"}
                };
            }}}
        }}
    }
});

auto stitched = usersSchema.Stitch(postsSchema);
```

## Querying across schemas

After stitching, all types and query fields are available in a single schema:

```cpp
auto result = stitched.Resolve({
    .query = R"({
        user(id: "1") { name email }
        post(id: "1") { title }
    })"
}).get();
```

```json
{
  "user": {
    "name": "Alice",
    "email": "alice@example.com"
  },
  "post": {
    "title": "Hello Ariane"
  }
}
```

## Chaining

`Stitch()` returns a new `Schema`, so you can chain naturally:

```cpp
auto combined = schemaA.Stitch(schemaB).Stitch(schemaC);
```

## How it works

### SDL merging

Types from both schemas are merged at the `SchemaDefinition` level:

- **Duplicate non-root type names** produce a `runtime_error`
- **Built-in scalars** (`String`, `Int`, `Float`, `Boolean`, `ID`) and introspection types (`__Schema`, `__Type`, etc.) are deduplicated silently

### Resolver map merging

Resolver maps are deep-merged:

- **Conflicting user-defined field resolvers** produce a `runtime_error`
- **System fields** (`__`-prefixed) are safely deduplicated

### Root type merging

`Query`, `Mutation`, and `Subscription` fields from both schemas are combined automatically — no `extend type` syntax required.

### Introspection

After stitching, `__schema` and `__type` are re-injected pointing at the merged type map. Introspection correctly reflects all types from both schemas.
