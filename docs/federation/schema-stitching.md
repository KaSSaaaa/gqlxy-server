---
title: Schema Stitching
---

Schema stitching lets you merge multiple independent `Schema` instances into a single unified schema via `Schema::Stitch()`.

## Basic usage

```cpp
#include <gqlxy/schema.h>

using namespace gqlxy;

Schema usersSchema({
    .typeDefs = R"(
        type Query {
            user(id: ID!): User
        }

        type User {
            id: ID!
            name: String!
            email: String!
        }
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
        type Query {
            post(id: ID!): Post
        }

        type Post {
            id: ID!
            title: String!
            authorId: ID!
        }
    )",
    .resolvers = {
        {"Query", Resolver{
            {"post", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                return Resolver{
                    {"id", r.Args()["id"].get<std::string>()},
                    {"title", "Hello GQLXY"},
                    {"authorId", "1"}
                };
            }}}
        }}
    }
});

auto stitched = usersSchema.Stitch(postsSchema);
```

## Querying the stitched schema

All types and root fields are available in the merged schema:

```cpp
auto result = stitched.Resolve({
    .query = R"({
        user(id: "1") {
            name
            email
        }
        post(id: "1") {
            title
        }
    })"
}).get();
```

## Chaining

`Stitch()` returns a new `Schema`, so you can chain multiple stitches:

```cpp
auto combined = schemaA.Stitch(schemaB).Stitch(schemaC);
```

## Rules

### Types

- **Duplicate non-root type names** across schemas produce a `runtime_error`
- Built-in scalars and introspection types (`__`-prefixed) are deduplicated silently

### Root types

`Query`, `Mutation`, and `Subscription` fields from both schemas are merged into a single root type. Conflicting field names produce a `runtime_error`.

### Introspection

After stitching, `__schema` and `__type` are re-injected to reflect all types from both schemas.
