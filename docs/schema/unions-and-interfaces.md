---
title: Unions and Interfaces
---

## Defining an interface

Interfaces are abstract types in a GraphQL schema. They define a set of fields that will be defined in the concrete Object types.

```graphql
interface Node {
    id: ID!
}

type Task implements Node {
    id: ID!
    title: String!
    isComplete: Boolean!
}

type Appointment implements Node {
    id: ID!
    subject: String!
    when: String!
}

type Query {
    node(id: ID!): Node
}
```

To resolve an interface type in GQLXY, you can define a `TypeResolver` on the `__resolveType` key for the abstract type. Then, return the type name if the Resolver contains the field of the concrete type.

```cpp
Schema schema({
    .resolvers = {
        {"Node", Resolver{
            {"__resolveType", TypeResolver{[](const Resolver& obj) -> std::optional<std::string> {
                if (obj.contains("title")) return "Task";
                if (obj.contains("subject")) return "Appointment";
                return std::nullopt;
            }}}
        }}
    }
});
```

You can then query your interface with an **Inline Fragment**.

```graphql
query {
    node(id: "1") {
        __typename
        id
        ... on Task {
            title
            isComplete
        }
        ... on Appointment {
            subject
            when
        }
    }
}
```

## Unions

Union types serve kind of the same purpose, except that they don't need to share common fields.

```graphql
type User {
    id: ID!
    email: String!
}

type Post {
    id: ID!
    title: String!
}

union SearchResult = User | Post

type Query {
    search(term: String!): [SearchResult]
}
```

Then, define a `TypeResolver` on the union type name:

```cpp
Schema schema({
    .resolvers = {
        {"SearchResult", Resolver{
            {"__resolveType", TypeResolver{[](const Resolver& obj) -> std::optional<std::string> {
                if (obj.contains("email")) return "User";
                if (obj.contains("title")) return "Post";
                return std::nullopt;
            }}}
        }}
    }
});
```