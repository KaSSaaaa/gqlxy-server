---
title: Custom Scalars
---

GQLXY supports custom scalars for both output serialization and input coercion.

## Output

Subclass `ScalarType` allow you to control how a custom scalar is serialized in responses:

```cpp
#include <gqlxy/scalars.h>

class DateScalar : public ScalarType {
public:
    DateScalar(const std::string& v) : ScalarType([=]() -> nlohmann::json {
        return v;
    }) {}
};
```

Then, use it as a `ValueResolver`:

```cpp
{"createdAt", DateScalar("2024-01-15T08:30:00Z")}
```

## Input

Given the following schema, with a custom DateTime scalar :

```graphql
scalar DateTime

type Query {
    eventsAfter(since: DateTime!): [Event]
}
```

Then, register a `ScalarResolver` in `SchemaOptions::scalars` to coerce input values (arguments and variables) for a custom scalar:

```cpp
Schema schema({
    .scalars = {
        {"DateTime", [](const nlohmann::json& input) -> nlohmann::json {
            // Validate or transform the incoming value
            auto str = input.get<std::string>();
            // ... parse/validate ISO 8601 ...
            return str;
        }}
    }
});
```

## Unregistered scalars

Scalars declared in the SDL but without a registered `ScalarResolver` pass through as-is — the raw JSON value is forwarded to the resolver unchanged.
