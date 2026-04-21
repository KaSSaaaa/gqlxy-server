# GQLXY Server — Agent Instructions

Communicate at a senior C++ engineer level with deep GraphQL knowledge.

## Build & Test

**Configure and build** (macOS arm64):
```sh
cmake --preset arm64-debug
cmake --build out/build/arm64-debug
```

**Run all tests:**
```sh
ctest --test-dir out/build/arm64-debug --output-on-failure
```

**Run a single test:**
```sh
./out/build/arm64-debug/tests/gqlxy_unit_tests --gtest_filter=SuiteName.TestName
./out/build/arm64-debug/tests/gqlxy_e2e_tests --gtest_filter=SuiteName.TestName
```

Dependencies are managed via vcpkg (bootstrapped automatically by the `arm64-debug` preset). Key deps: `cppgraphqlgen` (graphqlpeg), `nlohmann-json`, `pegtl`, `gtest`, `better-enums`.

## Architecture

GQLXY Server is a header-only-style C++20 GraphQL library. The public surface lives entirely in `include/gqlxy/`:

- **`schema.h`** — `Schema` is the entry point. Constructed with `SchemaOptions` (SDL `typeDefs` string + `Resolver` map + `allowIntrospection` flag). `Schema::Resolve(SchemaResolveArgs)` returns `Task<GraphQLResponse>` (a coroutine); call `.get()` for synchronous use.
- **`resolvers.h`** — `ValueResolver` is a recursive `std::variant` accepting scalars (`int`, `uint64_t`, `double`, `float`, `bool`, `std::string`), nested `Resolver` (`unordered_map<string, ValueResolver>`), `vector<ValueResolver>`, and four async resolver function types. Also defines `ResolverArgs`.
- **`task.h`** — `Task<T>`: a minimal C++20 coroutine type used internally and exposed for `CoroutineResolver`.

Internal implementation (`src/gqlxy/internal/`):
- `parser/` — Parses SDL type definitions into a `Document` using `cppgraphqlgen::graphqlpeg`.
- `introspection/` — Builds introspection resolvers from the parsed `Document`; injected automatically when `allowIntrospection = true`.
- `utils/visit.h` — `overloaded` helper for `std::visit` on `ValueResolver`.
- `peg/` — `first_node<T>` (direct children) and `find_node<T>` (recursive) wrappers for AST traversal.

Resolution flow: `Schema::Resolve` parses the query with `graphqlpeg`, walks the selection set, dispatches each field against the `Resolver` map, serializes to JSON via `nlohmann/json`. Arguments are parsed into `ResolverArgs.args` (`nlohmann::json`). Variable references (`$var`) are substituted from `SchemaResolveArgs.variables`.

## Key Conventions

**Calling `Resolve`:**
```cpp
auto result = schema.Resolve({
    .query     = "query GetUser($id: ID!) { user(id: $id) { name } }",
    .variables = {{"id", "42"}}
}).get();
```

**Resolver function types** — all four accept `const ResolverArgs&`:
```cpp
FunctionResolver      = std::function<ValueResolver(const ResolverArgs&)>
AsyncFunctionResolver = std::function<std::future<ValueResolver>(const ResolverArgs&)>
CoroutineResolver     = std::function<Task<ValueResolver>(const ResolverArgs&)>
CallbackResolver      = std::function<void(const ResolverArgs&, std::function<void(const ValueResolver&)>)>
```

**Accessing field arguments:**
```cpp
{"greet", [](const ResolverArgs& r) -> ValueResolver {
    return "Hello, " + r.args["name"].get<std::string>();
}}
```

**Returning null** — use `std::nullopt`, `nullptr`, or `std::monostate{}` as a `ValueResolver`; all map to GraphQL `null`.

**Nested resolvers** — use brace-initializer syntax; `ValueResolver` constructors handle `initializer_list<pair<string, ValueResolver>>` → `Resolver` and `initializer_list<ValueResolver>` → `vector<ValueResolver>` automatically.

**`clang-format`** — style defined in `.clang-format` at repo root. Use `// clang-format off/on` around deeply-nested resolver literals.

**Tests** — `gqlxy_unit_tests` (unit) and `gqlxy_e2e_tests` (full query execution against `samples/schema.today.graphql`). Tests may include `src/` internal headers via `target_include_directories(...PRIVATE ${CMAKE_SOURCE_DIR}/src)` to assert on the parsed AST via `Schema::GetDocument()`.

## Code Quality

- Don't comment code unless it needs clarification — code must be self-readable
- Favor structs over multiple parameters to improve readability and extensibility
- Functions ≤ 20 lines
- `std::optional` over raw pointers; no raw owning pointers; `const&` by default
- C++20 required (`cxx_std_20`)
- Use DRY principles
