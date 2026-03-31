---
title: Federation Subgraph
---

GQLXY can act as an Apollo Federation subgraph, participating in a federated supergraph managed by Apollo Router or Apollo Gateway.

## Enabling federation

Set `federation = true` in `SchemaOptions`:

```cpp
Schema schema({
    .typeDefs = typeDefs,
    .resolvers = resolvers,
    .federation = true,
});
```

This auto-injects the federation protocol fields (`_service` and `_entities`) into your schema.

## Defining entities

Mark types as entities using the `@key` directive in your SDL:

```cpp
static const std::string TypeDefs = R"(
    type User @key(fields: "id") {
        id: ID!
        name: String!
        email: String!
    }

    type Product @key(fields: "sku") {
        sku: ID!
        title: String!
        price: Float!
    }

    type Query {
        user(id: ID!): User
        product(sku: ID!): Product
    }
)";
```

## Entity resolvers

For each `@key` type, provide a `__resolveReference` resolver. It receives the entity's key fields via `args.Args()` and returns the full entity:

```cpp
.resolvers = {
    {"User", Resolver{
        {"__resolveReference", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
            auto id = r.Args()["id"].get<std::string>();
            auto it = Users.find(id);
            return it != Users.end() ? ValueResolver(it->second) : ValueResolver(std::monostate{});
        }}}
    }},
    {"Product", Resolver{
        {"__resolveReference", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
            auto sku = r.Args()["sku"].get<std::string>();
            auto it = Products.find(sku);
            return it != Products.end() ? ValueResolver(it->second) : ValueResolver(std::monostate{});
        }}}
    }}
}
```

## Federation protocol queries

With `federation = true`, GQLXY handles these queries automatically:

### `_service { sdl }`

Returns the annotated SDL of your subgraph for schema composition:

```graphql
{ _service { sdl } }
```

### `_entities(representations: [_Any!]!)`

Resolves entity references sent by the gateway. Each representation is a `{ __typename, ...keyFields }` object:

```graphql
query ($reps: [_Any!]!) {
    _entities(representations: $reps) {
        ... on User { id name }
        ... on Product { sku title }
    }
}
```

With variables:

```json
{
  "reps": [
    { "__typename": "User", "id": "2" },
    { "__typename": "Product", "sku": "widget-a" }
  ]
}
```

## Supported directives

The following directives are supported by GQLXY:

| Directive | Purpose |
|---|---|
| `@key(fields: "...")` | Declares an entity's primary key |
| `@external` | Marks a field as owned by another subgraph |
| `@requires(fields: "...")` | Declares fields needed from the parent entity |
| `@provides(fields: "...")` | Declares fields provided to other subgraphs |
| `@extends` | Extends a type defined in another subgraph |

These directives are preserved in `_service { sdl }` output for composition.

## Federation v2

GQLXY also supports the `@link` directive and Federation v2 import syntax, allowing your subgraph SDL to declare which federation spec version it targets.
