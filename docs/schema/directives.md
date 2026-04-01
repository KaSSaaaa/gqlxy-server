---
title: Directives
---

Directives allow you to replicate certain behaviors and modify some parts of your GraphQL schema. There are 2 types of directives : **type system directives** and **executional directives**.

- Type System Directive : allow you to annotate your schema with information exposed by the server to the user.
- Executional Directive : allow you to alter the result of the query, by either modifying or erasing the data.

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

- `std::nullopt` — omit the field from the response entirely
- `std::optional<ValueResolver>(value)` — replace with a new value (return `make_optional(v)` to pass through unchanged)

### Example

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
    {"redact", [](const ResolverArgs& args, const ValueResolver& v) -> std::optional<ValueResolver> {
        // Omit the field when if: true, keep original value when if: false
        return args.Args().value("if", false)
            ? std::nullopt               // omit field
            : std::make_optional(v);     // keep original
    }}
}
```