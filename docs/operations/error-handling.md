---
title: Error Handling
---

## Resolver errors

When a resolver throws an exception, GQLXY catches it, sets the field value to `null`, and appends a structured error to the `errors` array. Execution continues for all other fields.

```cpp
Schema schema({
    .typeDefs = R"(
        type Query {
            user: User
            config: String
        }
        
        type User {
          id: ID!
          name: String!
        }
    )",
    .resolvers = {
        {"Query", Resolver{
            {"user", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
                throw std::runtime_error("User not found");
            }}},
            {"config", "production"}
        }}
    }
});

auto result = schema.Resolve({
  .query = R"({
    user {
      name
    }
    config
  })"
}).get();
```

Response:

```json
{
  "data": {
    "user": null,
    "config": "production"
  },
  "errors": [
    {
      "message": "User not found",
      "path": ["user"]
    }
  ]
}
```

## Error paths

The `path` array traces the exact field that failed. Nested errors include every level:

```cpp
{"profile", Resolver{
    {"avatar", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
        throw std::runtime_error("CDN unavailable");
    }}}
}}
```

```json
{
  "errors": [
    {
      "message": "CDN unavailable",
      "path": ["profile", "avatar"]
    }
  ]
}
```

## Partial results

Data and errors are always returned together. If some fields succeed and others fail, the response contains both a partial `data` object and an `errors` array:

```cpp
if (result.data)   { /* process result.data.value() */ }
if (result.errors) { /* log result.errors.value()   */ }
```

## Validation errors

Before execution, GQLXY validates the query against the schema. Validation errors are returned in `errors` without any `data`:

```graphql
{ nonExistentField }
```

```json
{
  "errors": [
    {
      "message": "Cannot query field \"nonExistentField\" on type \"Query\""
    }
  ]
}
```

Common validation errors:

| Situation | Error message |
|---|---|
| Querying a field that doesn't exist | `Cannot query field "X" on type "Y"` |
| Variable used but not declared | `Variable "$X" is not declared in the operation` |
| Required variable not provided | `Variable "$X" of type "T!" was not provided` |
| Required field argument missing | `Argument "X" of field "Y.Z" is required` |

## Serializing the response

If you want to transform the `ResolveResult` to JSON, you can use the `Serialize` method to produce the standard GraphQL JSON envelope:

```cpp
#include <gqlxy/results.h>

auto result = schema.Resolve({.query = query}).get();
nlohmann::json response = gqlxy::Serialize(result);
// { "data": {...}, "errors": [...] }
```

`data` is omitted when there are only validation errors. `errors` is omitted when there are none.
