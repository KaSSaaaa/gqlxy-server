# Resolvers

Resolvers are the core of GQLXY. They map your GraphQL schema fields to C++ values and functions.

## Static values

The simplest resolvers are static values set at schema construction time:

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

`ValueResolver` natively accepts: `int`, `uint64_t`, `double`, `float`, `bool`, `std::string`, `const char*`, `std::nullopt` / `nullptr` / `std::monostate` (null), nested `Resolver` maps, and `std::vector<ValueResolver>` (lists).

## Function resolvers

For dynamic data, wrap your logic in one of four function resolver types. All receive a `const ResolverArgs&` parameter:

### FunctionResolver (sync)

The default choice for most resolvers:

```cpp
{"user", FunctionResolver{[&db](const ResolverArgs& args) -> ValueResolver {
    auto id = args.Args()["id"].get<std::string>();
    auto user = db.findUser(id);
    if (!user) return std::nullopt;
    return Resolver{
        {"id",    user->id},
        {"name",  user->name},
        {"email", user->email}
    };
}}}
```

### AsyncFunctionResolver (std::future)

For I/O-bound work that should run on a separate thread:

```cpp
{"user", AsyncFunctionResolver{[](const ResolverArgs& args) -> std::future<ValueResolver> {
    return std::async(std::launch::async, [=]() -> ValueResolver {
        // expensive I/O here
        return Resolver{
            {"id", "1"},
            {"name", "Alice"}
        };
    });
}}}
```

### CoroutineResolver (C++20 coroutine)

If your codebase uses coroutines:

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

For APIs that use callback patterns:

```cpp
{"user", CallbackResolver{[](const ResolverArgs& args,
                             const std::function<void(const ValueResolver&)>& cb) {
    fetchUserWithCallback(args.Args()["id"].get<std::string>(),
        [cb](const User& user) {
            cb(Resolver{
                {"id", user.id},
                {"name", user.name}
            });
        });
}}}
```

## Lists

Return lists as `std::vector<ValueResolver>`:

```cpp
{"users", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
    std::vector<ValueResolver> result;
    for (const auto& u : allUsers()) {
        result.push_back(Resolver{
            {"id", u.id},
            {"name", u.name}
        });
    }
    return result;
}}}
```

Or with brace-initialized static data:

```cpp
{"users", std::vector<ValueResolver>{
    Resolver{
        {"id", "1"},
        {"name", "Alice"}
    },
    Resolver{
        {"id", "2"},
        {"name", "Bob"}
    }
}}
```

## Aliases

Field aliases are handled automatically. The alias is used as the JSON response key:

```graphql
{
    first: user(id: "1") { name }
    second: user(id: "2") { name }
}
```

```json
{
    "first": { "name": "Alice" },
    "second": { "name": "Bob" }
}
```

## Fragments

Both named fragments and inline fragments work out of the box:

```graphql
fragment UserCard on User {
    id
    name
    email
}

query {
    user(id: "1") {
        ...UserCard
        bio
    }
}
```

Inline fragments:

```graphql
query {
    user(id: "1") {
        id
        ... on User {
            name
            bio
        }
    }
}
```

## Interfaces and unions

### Defining an interface

```cpp
Schema schema({
    .typeDefs = R"(
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
    )",
    .resolvers = {
        {"Query", Resolver{
            {"node", FunctionResolver{[&nodes](const ResolverArgs& args) -> ValueResolver {
                auto id = args.Args()["id"].get<std::string>();
                auto it = nodes.find(id);
                return it != nodes.end() ? ValueResolver(it->second) : ValueResolver(std::nullopt);
            }}}
        }},
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

The `TypeResolver` receives the resolved object's `Resolver` map and returns the concrete type name. Place it in the abstract type's resolver map under the `__resolveType` key.

### Querying with inline fragments

```graphql
query {
    node(id: "1") {
        __typename
        id
        ... on Task { title isComplete }
        ... on Appointment { subject when }
    }
}
```

`__typename` always reflects the runtime type returned by `__resolveType`.

### Unions

Unions work identically — define a `TypeResolver` on the union type name:

```cpp
.typeDefs = R"(
    union SearchResult = User | Post
    type Query { search(term: String!): [SearchResult] }
)",
.resolvers = {
    {"SearchResult", Resolver{
        {"__resolveType", TypeResolver{[](const Resolver& obj) -> std::optional<std::string> {
            if (obj.contains("email")) return "User";
            if (obj.contains("title")) return "Post";
            return std::nullopt;
        }}}
    }}
}
```

## Mutations

Mutations are defined under the `Mutation` type. Top-level mutation fields execute **serially** in document order (per the GraphQL spec §6.3.1):

```cpp
Schema schema({
    .typeDefs = R"(
        type Query { books: [Book!]! }
        type Mutation {
            addBook(title: String!, author: String!): Book!
        }
        type Book { id: ID! title: String! author: String! }
    )",
    .resolvers = {
        {"Mutation", Resolver{
            {"addBook", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
                auto title  = args.Args()["title"].get<std::string>();
                auto author = args.Args()["author"].get<std::string>();
                // Insert into database...
                return Resolver{
                    {"id", "new-id"},
                    {"title", title},
                    {"author", author}
                };
            }}}
        }}
    }
});
```

## ResolverArgs reference

Every function resolver receives a `const ResolverArgs&` with:

| Method | Returns | Description |
|---|---|---|
| `Args()` | `const nlohmann::json&` | Field arguments as a JSON object |
| `Context<T>()` | `T&` or `const T&` | Request-scoped context (see [Getting Started](../getting-started.md#context)) |
