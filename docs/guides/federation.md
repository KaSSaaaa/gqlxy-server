# Apollo Federation

GQLXY can act as an Apollo Federation subgraph, allowing your schema to participate in a federated supergraph managed by Apollo Router or Apollo Gateway.

## Enabling federation

Set `federation = true` in your `SchemaOptions`:

```cpp
Schema schema({
    .typeDefs   = typeDefs,
    .resolvers  = resolvers,
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

For each `@key` type, provide a `__resolveReference` resolver in that type's resolver map. It receives the entity representation (the key fields) and returns the full entity:

```cpp
.resolvers = {
    {"Query", Resolver{
        {"user", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
            auto id = r.Args()["id"].get<std::string>();
            auto it = Users.find(id);
            return it != Users.end() ? ValueResolver(it->second) : ValueResolver(std::monostate{});
        }}}
    }},
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

The `__resolveReference` resolver receives the key fields as `args.Args()` — matching the fields declared in `@key(fields: "...")`.

## Federation protocol queries

With `federation = true`, GQLXY automatically handles these queries:

### `_service { sdl }`

Returns the full annotated SDL string of your subgraph. The gateway uses this for schema composition:

```graphql
{
    _service {
        sdl
    }
}
```

### `_entities(representations: [_Any!]!)`

Resolves entity references. The gateway sends a list of `{ __typename, ...keyFields }` representations, and GQLXY dispatches each to the matching `__resolveReference` resolver:

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
    {
      "__typename": "User",
      "id": "2"
    },
    {
      "__typename": "Product",
      "sku": "widget-a"
    }
  ]
}
```

## Federation v2

GQLXY supports the `@link` directive and `@federation` import syntax introduced in Federation v2, allowing the subgraph to declare which federation spec version it targets.

## Composition validation

At `Schema` construction time, GQLXY validates that every type annotated with `@key` has a corresponding `__resolveReference` entity resolver registered. If not, a structured error is emitted.

## Supported directives

The following federation SDL directives are parsed and preserved:

| Directive | Purpose |
|---|---|
| `@key(fields: "...")` | Declares an entity's primary key |
| `@external` | Marks a field as owned by another subgraph |
| `@requires(fields: "...")` | Declares fields needed from the parent entity |
| `@provides(fields: "...")` | Declares fields provided to other subgraphs |
| `@extends` | Extends a type defined in another subgraph |

These directives survive round-tripping through `_service { sdl }`.
