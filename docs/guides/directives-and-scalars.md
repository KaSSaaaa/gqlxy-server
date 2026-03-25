# Directives & Custom Scalars

## Built-in directives

GQLXY supports the standard `@skip` and `@include` directives out of the box. They work on fields, inline fragments, and named fragment spreads:

```graphql
query ($hideEmail: Boolean!) {
    name
    email @skip(if: $hideEmail)
    role @include(if: true)
}
```

Variable substitution is supported in directive arguments.

## Custom directives

Register custom directives via `SchemaOptions::directives`. A `DirectiveResolver` receives the resolver args (with the directive's arguments) and the resolved field value, and returns:

- `std::nullopt` — keep the original value unchanged
- `std::optional<ValueResolver>(value)` — replace with a new value
- `std::optional<ValueResolver>(std::monostate{})` — set the field to `null` (redact)

### Transform directive

Transform the resolved value:

```cpp
Schema schema({
    .typeDefs = R"(
        directive @uppercase on FIELD
        type Query { name: String }
    )",
    .resolvers = {
        {"Query", Resolver{{"name", "Alice"}}}
    },
    .directives = {
        {"uppercase", [](const ResolverArgs&, const ValueResolver& v) -> std::optional<ValueResolver> {
            auto s = v.As<std::string>();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return s;
        }}
    }
});

// Query: { name @uppercase }
// Result: { "name": "ALICE" }
```

### Directive with arguments

```cpp
.directives = {
    {"prefix", [](const ResolverArgs& args, const ValueResolver& v) -> std::optional<ValueResolver> {
        return args.Args().value("with", "") + v.As<std::string>();
    }}
}

// Query: { role @prefix(with: "role: ") }
// Result: { "role": "role: admin" }
```

### Redact directive (skip-style)

```cpp
.directives = {
    {"redact", [](const ResolverArgs& args, const ValueResolver&) -> std::optional<ValueResolver> {
        // If redact(if: true), return monostate (null); otherwise keep original
        return args.Args().value("if", false)
            ? std::nullopt                               // keep original
            : std::optional<ValueResolver>(std::monostate{});  // redact to null
    }}
}
```

## Custom scalars

GQLXY supports custom scalars for both output serialization and input coercion.

### Output: ScalarType

Subclass `ScalarType` to control how a custom scalar is serialized in responses:

```cpp
#include <gqlxy/scalars.h>

class DateScalar : public ScalarType {
public:
    DateScalar(const std::string& v)
        : ScalarType([=]() -> nlohmann::json { return v; }) {}
};
```

Use it as a `ValueResolver`:

```cpp
{"createdAt", DateScalar("2024-01-15T08:30:00Z")}
```

### Input: ScalarResolver

Register a `ScalarResolver` in `SchemaOptions::scalars` to coerce input values (arguments and variables) for a custom scalar:

```cpp
Schema schema({
    .typeDefs = R"(
        scalar DateTime
        type Query { eventsAfter(since: DateTime!): [Event] }
    )",
    .resolvers = { /* ... */ },
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

### Unregistered scalars

Scalars declared in the SDL but without a registered `ScalarResolver` pass through as-is — the raw JSON value is forwarded to the resolver unchanged.
