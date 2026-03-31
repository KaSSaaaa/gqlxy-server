---
title: Resolvers
---

Resolvers is the way for GQLXY to deliver your data through simple functions. There are many types of Resolvers available.

## Static values

The simplest resolvers are values set at schema construction time:

```cpp
Schema schema({
    .typeDefs = R"(
        type Query {
            user: User
        }
        type User {
            id: ID!
            name: String!
            email: String!
            active: Boolean!
        }
    )",
    .resolvers = {
        {"Query", Resolver{
            {"user", Resolver{
                {"id", "1"},
                {"name", "Alice"},
                {"email", "alice@example.com"},
                {"active", true}
            }}
        }}
    }
});
```

## Function resolvers

For dynamic data, you can use function resolver types. All receive a `const ResolverArgs&`:

### FunctionResolver (sync)

```cpp
{"user", FunctionResolver{[&db](const ResolverArgs& args) -> ValueResolver {
    auto id = args.Args()["id"].get<std::string>();
    auto user = db.findUser(id);
    if (!user) return std::nullopt;
    return Resolver{
        {"id", user->id},
        {"name", user->name},
        {"email", user->email}
    };
}}}
```

### AsyncFunctionResolver (std::future)

```cpp
{"user", AsyncFunctionResolver{[](const ResolverArgs& args) -> std::future<ValueResolver> {
    return std::async(std::launch::async, [=]() -> ValueResolver {
        return Resolver{
            {"id", "1"},
            {"name", "Alice"}
        };
    });
}}}
```

### CoroutineResolver (C++20 coroutine)

```cpp
{"user", CoroutineResolver{[](const ResolverArgs& args) -> Task<ValueResolver> {
    auto user = co_await fetchUserAsync(args.Args()["id"].get<std::string>());
    co_return Resolver{
        {"id", user.id},
        {"name", user.name}
    };
}}}
```

### CallbackResolver

```cpp
{"user", CallbackResolver{[](const ResolverArgs& args, const std::function<void(const ValueResolver&)>& cb) {
    fetchUserWithCallback(args.Args()["id"].get<std::string>(), [cb](const User& u) {
        cb(Resolver{{"id", u.id}, {"name", u.name}});
    });
}}}
```

## Lists

```cpp
{"users", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
    auto users = allUsers();
    return users | std::views::transform([](const auto& u) -> ValueResolver {
        return Resolver{{"id", u.id}, {"name", u.name}};
    }) | std::ranges::to<std::vector>();
}}}
```

## Mutations

To define mutation resolvers, you can use the `Mutation` type. If you want to execute multiple mutations, they will be executed **serially**:

```cpp
{"Mutation", Resolver{
    {"addBook", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
        auto title  = args.Args()["title"].get<std::string>();
        auto author = args.Args()["author"].get<std::string>();
        return Resolver{
            {"id", "new-id"},
            {"title", title},
            {"author", author}
        };
    }}}
}}
```

## Context

GQLXY handles request-scoped state (auth tokens, DB connections, etc.) through `SchemaResolveArgs::context`:

```cpp
struct RequestContext {
    std::string userId;
};

auto result = schema.Resolve({
    .query = "{ me { name } }",
    .context = RequestContext{"42"}
}).get();
```

You can access it inside any resolver like this:

```cpp
{"me", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
    auto& ctx = args.Context<RequestContext>();
    return Resolver{
        {"id", ctx.userId}
    };
}}}
```

## Aliases, fragments, and interfaces

Aliases, named/inline fragments, and interface/union `TypeResolver` are all handled automatically. See [Unions & Interfaces](../schema/unions-and-interfaces.md) for type resolution.
