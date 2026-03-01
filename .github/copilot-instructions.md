# Ariane GraphQL Server — Copilot Instructions

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
# Unit tests (resolve, resolvers, schema parser, introspection, optional)
./out/build/arm64-debug/tests/ariane_unit_tests --gtest_filter=SuiteName.TestName

# E2E tests (full introspection query against schema.today.graphql)
./out/build/arm64-debug/tests/ariane_e2e_tests --gtest_filter=SuiteName.TestName
```

**Format code:**
```sh
make format
```

Dependencies are managed via vcpkg (bootstrapped automatically by the `arm64-debug` preset). Key deps: `cppgraphqlgen` (graphqlpeg), `nlohmann-json`, `pegtl`, `gtest`, `better-enums`.

## Architecture

Ariane is a header-only-style C++20 GraphQL library. The public surface lives entirely in `include/ariane/`:

- **`schema.h`** — `Schema` is the entry point. Constructed with `SchemaOptions` (SDL `typeDefs` string + `Resolver` map + `allowIntrospection` flag). `Schema::Resolve(SchemaResolveArgs)` returns `Task<ResolveResult>` (a coroutine); call `.get()` for synchronous use.
- **`resolvers.h`** — `ValueResolver` is a recursive `std::variant` accepting scalars (`int`, `uint64_t`, `double`, `float`, `bool`, `std::string`), nested `Resolver` (alias for `unordered_map<string, ValueResolver>`), `vector<ValueResolver>`, and four async resolver function types (see below). Also defines `ResolverArgs`.
- **`task.h`** — `Task<T>`: a minimal C++20 coroutine type used internally and exposed for `CoroutineResolver`.

Internal implementation (`src/ariane/internal/`):
- `parser/` — Parses SDL type definitions into a `Document` using `cppgraphqlgen::graphqlpeg`.
- `introspection/` — Builds introspection resolvers from the parsed `Document`; injected automatically when `allowIntrospection = true`.
- `utils/visit.h` — `overloaded` helper for `std::visit` on `ValueResolver`.
- `peg/` — `first_node<T>` (direct children) and `find_node<T>` (recursive) wrappers for AST traversal.

Resolution flow: `Schema::Resolve` parses the query with `graphqlpeg`, walks the selection set, dispatches each field against the `Resolver` map, serializes to JSON via `nlohmann/json`. Arguments are parsed from the query AST into `ResolverArgs.args` (`nlohmann::json`) and passed to function resolvers. Variable references (`$var`) are substituted from `SchemaResolveArgs.variables`.

## Key Conventions

**Calling `Resolve`:**
```cpp
auto result = schema.Resolve({
    .query     = "query GetUser($id: ID!) { user(id: $id) { name } }",
    .variables = {{"id", "42"}}   // nlohmann::json object
}).get();
```

**Resolver function types** — all four accept `const ResolverArgs&`; pick the one that fits your async model:
```cpp
FunctionResolver      = std::function<ValueResolver(const ResolverArgs&)>              // sync lambda
AsyncFunctionResolver = std::function<std::future<ValueResolver>(const ResolverArgs&)> // std::future
CoroutineResolver     = std::function<Task<ValueResolver>(const ResolverArgs&)>        // C++20 coroutine
CallbackResolver      = std::function<void(const ResolverArgs&, std::function<void(const ValueResolver&)>)>
```

**Accessing field arguments inside a resolver:**
```cpp
{"greet", [](const ResolverArgs& r) -> ValueResolver {
    return "Hello, " + r.args["name"].get<std::string>();
}}
```

**Returning null** — use `std::nullopt`, `nullptr`, or `std::monostate{}` as a `ValueResolver`; all map to GraphQL `null`.

**Nested resolvers** — use brace-initializer syntax; the `ValueResolver` constructors handle `initializer_list<pair<string, ValueResolver>>` → `Resolver` and `initializer_list<ValueResolver>` → `vector<ValueResolver>` automatically.

**`clang-format`** — style is defined in `.clang-format` at the repo root. Use `// clang-format off/on` guards around deeply-nested resolver literals.

**Tests** — two binaries: `ariane_unit_tests` (unit) and `ariane_e2e_tests` (full query execution against `samples/schema.today.graphql`). Tests may include `src/` internal headers via `target_include_directories(...PRIVATE ${CMAKE_SOURCE_DIR}/src)` — use this to assert on the parsed AST via `Schema::GetDocument()`.

**C++ standards** — C++20 required (`cxx_std_20`). Prefer `std::optional` over raw pointers; no raw owning pointers; `const&` by default; functions ≤ 20 lines.

**Code Quality**
- Don't comment the code unless necessary. Code must be readable by itself
- Favor structs instead of multiple parameters to improve code readability and extensibility
- Functions have to be concise. 20 lines max.
- Use DRY principles