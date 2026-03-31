---
title: Schema basics
description: Basic features for GQLXY Server
---

A GQLXY schema is built from two things: a **type definition** (SDL) and **resolver maps**. The type definition describes the shape of the data your clients can query, while the resolver maps describes how your server will shape and resolve those data.

## Type definitions (SDL)

Describe your API in standard GraphQL SDL. You can write it inline or read it from a file:

```cpp
// Inline
std::string typeDefs = R"(
    type Query {
        hello: String
        user(id: ID!): User
    }

    type User {
        id: ID!
        name: String!
        email: String!
    }
)";

// From a file
std::ifstream file("schema.graphql");
std::ostringstream buf;
buf << file.rdbuf();
std::string typeDefs = buf.str();
```

## Scalar types

GQLXY supports all built-in GraphQL scalars out of the box:

| Scalar | C++ type |
|---|---|
| `String` | `std::string` |
| `Int` | `int` |
| `Float` | `double` / `float` |
| `Boolean` | `bool` |
| `ID` | `std::string` |

For custom scalars, see [Custom Scalars](custom-scalars.md).

## Nullable and non-null fields

By default, all fields are nullable. Append `!` to make a field non-null:

```graphql
type User {
    id: ID!          # non-null
    nickname: String # nullable
}
```

If a non-null field resolver returns null at runtime, GQLXY propagates the null upward to the nearest nullable parent (per the GraphQL spec). To specify that a field is null, you can either return `nullptr`, `std::nullopt` or `std::monostate{}`. If an `std::optional` has no value, it will also be treated automatically as null value.

## Lists

Lists are simply defined by surrounding the type with `[]`:

```graphql
type Query {
    users: [User!]!    # non-null list of non-null Users
    tags: [String]     # nullable list of nullable strings
}
```

Any range-based type that defines `std::ranges::begin()` and `std::ranges::end()` can do the trick : `std::vector<ValueResolver>`, `std::set<ValueResolver>`, `std::initializer_list<ValueResolver>`, ... Any of them will work !

## Enums

```graphql
enum Episode {
    NEWHOPE
    EMPIRE
    JEDI
}

type Query {
    hero(episode: Episode): Character
}
```

Enum values are passed to resolvers as strings. If you want to use `better-enums`, you can simply return the value:

```cpp
BETTER_ENUM(Episode, int, NEWHOPE, EMPIRE, JEDI);

{"episode", Episode::NEWHOPE}
```

If you don't want to use `better-enums`, you can simply return the `std::string` equivalent:

```cpp
{"episode", "NEWHOPE"}
```

## Input types

You can use `input` types for complex mutation arguments:

```graphql
input CreateUserInput {
    name: String!
    email: String!
}

type Mutation {
    createUser(input: CreateUserInput!): User!
}
```

In the resolvers, you can access input fields like this:

```cpp
{"createUser", [](const ResolverArgs& args) {
    auto name = args.Args()["input"]["name"];
    // ...
}}
```

## Example

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

The `Resolve()` returns a `Task<ResolveResult>`. To get the result, call `.get()` to block for the result.
