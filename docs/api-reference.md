# API Reference

Complete reference for all public types and functions in `include/gqlxy/`.

All types live in the `gqlxy` namespace. Server types are in `gqlxy::server`.

---

## Schema

**Header:** `<gqlxy/schema.h>`

### SchemaOptions

```cpp
struct SchemaOptions {
    std::string typeDefs;            // GraphQL SDL
    Resolver resolvers;              // Type → field resolver map
    Directives directives;           // Custom directive handlers
    Scalars scalars;                 // Custom scalar input coercion
    bool allowIntrospection = true;  // Enable __schema / __type
    bool federation = false;         // Enable Apollo Federation protocol
};
```

### SchemaResolveArgs\<TContext\>

```cpp
template <typename TContext = std::monostate>
struct SchemaResolveArgs {
    std::string query;                                    // GraphQL query string
    nlohmann::json variables = nlohmann::json::object();  // Query variables
    std::string operationName;                            // Named operation selector
    TContext context = {};                                // Request-scoped context
};
```

### Schema

```cpp
class Schema {
public:
    explicit Schema(const SchemaOptions& options);

    template <typename TContext = std::monostate>
    Task<GraphQLResponse> Resolve(const SchemaResolveArgs<TContext>& args) const;

    template <typename TContext = std::monostate>
    SubscriptionHandle Subscribe(const SchemaResolveArgs<TContext>& args) const;

    Schema Stitch(const Schema& other) const;
};
```

| Method | Description |
|---|---|
| `Resolve(args)` | Execute a query/mutation. Returns `Task<GraphQLResponse>` — call `.get()` for synchronous use. |
| `Subscribe(args)` | Start a subscription. Returns a `SubscriptionHandle`. |
| `Stitch(other)` | Merge another schema into this one. Returns a new `Schema`. |

---

## Resolvers

**Header:** `<gqlxy/resolvers.h>`

### Type aliases

```cpp
using Resolver               = std::unordered_map<std::string, ValueResolver>;
using FunctionResolver       = std::function<ValueResolver(const ResolverArgs&)>;
using AsyncFunctionResolver  = std::function<std::future<ValueResolver>(const ResolverArgs&)>;
using CallbackResolver       = std::function<void(const ResolverArgs&, const std::function<void(const ValueResolver&)>&)>;
using TypeResolver           = std::function<std::optional<std::string>(const Resolver&)>;
using SubscriptionResolver   = std::function<SubscriptionEventStream(const ResolverArgs&)>;
using ScalarResolver         = std::function<nlohmann::json(const nlohmann::json&)>;
using Scalars                = std::unordered_map<std::string, ScalarResolver>;
using DirectiveResolver      = std::function<std::optional<ValueResolver>(const ResolverArgs&, const ValueResolver&)>;
using Directives             = std::unordered_map<std::string, DirectiveResolver>;
```

### ValueResolver

A variant type that holds any resolvable value:

```cpp
struct ValueResolver : std::variant<
    int, uint64_t, double, float, bool, std::string,
    Resolver,                    // nested object
    std::vector<ValueResolver>,  // list
    FunctionResolver,            // sync function
    AsyncFunctionResolver,       // std::future
    CoroutineResolver,           // C++20 coroutine
    CallbackResolver,            // callback
    TypeResolver,                // interface/union type resolver
    SubscriptionResolver,        // event stream
    ScalarType,                  // custom scalar output
    std::monostate               // null
>;
```

**Convenience constructors:**

| Constructor | Converts to |
|---|---|
| `ValueResolver(std::nullopt_t)` | `std::monostate` (null) |
| `ValueResolver(std::nullptr_t)` | `std::monostate` (null) |
| `ValueResolver(const char*)` | `std::string` |
| `ValueResolver(std::optional<T>)` | `T` if present, `std::monostate` if empty |
| `ValueResolver(initializer_list<pair<string, ValueResolver>>)` | `Resolver` |
| `ValueResolver(initializer_list<ValueResolver>)` | `std::vector<ValueResolver>` |

**Methods:**

| Method | Description |
|---|---|
| `Is<T>()` | Returns `true` if the variant holds type `T` |
| `As<T>()` | Returns a reference to the held `T` (throws if wrong type) |
| `AsIf<T>()` | Returns `std::optional<T>` — the value if held, `std::nullopt` otherwise |
| `IsNull()` | Returns `true` if holding `std::monostate` |

---

## ResolverArgs

**Header:** `<gqlxy/resolver_args.h>`

```cpp
class ResolverArgs {
public:
    const nlohmann::json& Args() const;            // Field arguments

    template<typename T> T& Context();             // Mutable context
    template<typename T> const T& Context() const; // Const context
};
```

---

## Results

**Header:** `<gqlxy/results.h>`

```cpp
struct ErrorLocation {
    int line;
    int column;
};

struct GraphQLError {
    std::string message;
    std::vector<std::string> path;
    std::vector<ErrorLocation> locations;
};

using GraphQLErrors = std::vector<GraphQLError>;

struct GraphQLResponse {
    std::optional<nlohmann::json> data;
    std::optional<GraphQLErrors> errors;
};

nlohmann::json Serialize(const GraphQLResponse& result);
```

`Serialize()` produces the standard GraphQL JSON response shape with `data` and `errors` keys.

---

## Subscriptions

**Header:** `<gqlxy/subscription.h>`

### SubscriptionEventStream

A pair of callbacks representing a stream of events:

```cpp
class SubscriptionEventStream {
public:
    SubscriptionEventStream(std::function<ValueResolver()> next,
                           std::function<void()> close);

    ValueResolver Next();   // Get next event (blocks)
    void Close();           // Close the stream
    bool Valid() const;     // Check if stream is valid
};
```

### SubscriptionHandle

Returned by `Schema::Subscribe()`:

```cpp
class SubscriptionHandle {
public:
    std::optional<GraphQLResponse> Next();  // Get next resolved event (blocks)
    void Cancel();                        // Cancel subscription

    // Move-only
    SubscriptionHandle(SubscriptionHandle&&) noexcept;
    SubscriptionHandle& operator=(SubscriptionHandle&&) noexcept;
};
```

### IsSubscription

```cpp
bool IsSubscription(const std::string& query);
```

Returns `true` if the query string is a subscription operation.

---

## PubSub

**Header:** `<gqlxy/pubsub.h>`

Thread-safe publish-subscribe system for subscriptions:

```cpp
class PubSub {
public:
    PubSub();

    void Publish(const std::string& topic, const ValueResolver& payload);
    SubscriptionEventStream AsyncIterator(std::initializer_list<std::string> topics);
};
```

| Method | Description |
|---|---|
| `Publish(topic, payload)` | Broadcast a value to all subscribers on the given topic |
| `AsyncIterator(topics)` | Create an event stream that receives events from the listed topics |

---

## Custom Scalars

**Header:** `<gqlxy/scalars.h>`

```cpp
class ScalarType {
public:
    ScalarType(const std::function<nlohmann::json()>& serialize);

    nlohmann::json Serialize() const;
};
```

Subclass `ScalarType` to implement custom output serialization. Register a `ScalarResolver` in `SchemaOptions::scalars` for input coercion.

---

## Task (Coroutines)

**Header:** `<gqlxy/task.h>`

```cpp
template <typename T>
struct Task {
    T get();  // Block until the coroutine completes and return the result
};
```

`Task<T>` is a minimal C++20 coroutine type. Used internally by `Schema::Resolve()` and available for `CoroutineResolver`.

---

## CoroutineResolver

**Header:** `<gqlxy/resolvers/CoroutineResolver.h>`

```cpp
class CoroutineResolver {
public:
    template<typename F> CoroutineResolver(F&& f);

    Task<ValueResolver> operator()(const ResolverArgs& args) const;
    explicit operator bool() const noexcept;
};
```

Type-erased callable wrapping a function that returns `Task<ValueResolver>`. GCC-compatible alternative to `std::function<Task<ValueResolver>(const ResolverArgs&)>`.

---

## Standalone Server

**Header:** `<gqlxy/server/standalone_server.h>`  
**Namespace:** `gqlxy::server`

```cpp
struct StandaloneServerOptions {
    Schema& schema;
    std::string host = "0.0.0.0";
    uint16_t port = 4000;
    std::string path = "/graphql";
};

class StandaloneServer {
public:
    explicit StandaloneServer(const StandaloneServerOptions& options);

    void Start();            // Blocks
    void StartAsync();       // Non-blocking
    void Stop();             // Graceful shutdown
    std::string GetUrl() const;
};
```

Requires the `standalone-server` vcpkg feature and `BUILD_STANDALONE_SERVER=ON`.
