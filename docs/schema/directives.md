---
title: Directives
---

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

You can register custom directives in the `SchemaOptions::directives` with a `DirectiveResolver`. It receives the resolver args (with the directive's arguments) and the resolved field value, and returns:

- `std::nullopt` — keep the original value unchanged
- `std::optional<ValueResolver>(value)` — replace with a new value
- `std::optional<ValueResolver>(std::monostate{})` — set the field to `null` (redact)

### Transform

Given the following schema :

```graphql
directive @uppercase on FIELD

type Query {
    name: String
}
```

You can resolve your custom `DirectiveResolver` like this :

```cpp
Schema schema({
    .resolvers = {
        {"Query", Resolver{
            {"name", "Alice"}
        }}
    },
    .directives = {
        {"uppercase", [](const ResolverArgs&, const ValueResolver& v) -> std::optional<ValueResolver> {
            auto s = v.As<std::string>();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return s;
        }}
    }
});
```

Executing the following query :

```graphql
query {
    name @uppercase
}
```

You get the following output :

```json
{
    "data": {
        "name": "ALICE"
    }
}
```

### With arguments

```cpp
.directives = {
    {"prefix", [](const ResolverArgs& args, const ValueResolver& v) -> std::optional<ValueResolver> {
        return args.Args().value("with", "") + v.As<std::string>();
    }}
}
```

```graphql
query {
    role @prefix(with: "role: ")
}
```

```json
{
    "data": {
        "role": "role: admin"
    }
}
```

### Redact directive (skip-style)

```cpp
.directives = {
    {"redact", [](const ResolverArgs& args, const ValueResolver&) -> std::optional<ValueResolver> {
        // If redact(if: true), return monostate (null); otherwise keep original
        return !args.Args().value("if", false)
            ? std::optional<ValueResolver>(std::monostate{})  // redact to null
            : std::nullopt;                                   // keep original
    }}
}
```