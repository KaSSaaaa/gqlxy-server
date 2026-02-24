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
# or directly:
./out/build/arm64-debug/tests/ariane_tests
```

**Run a single test:**
```sh
./out/build/arm64-debug/tests/ariane_tests --gtest_filter=SuiteName.TestName
```

**Format code:**
```sh
make format
```

Dependencies are managed via vcpkg (bootstrapped automatically by the `arm64-debug` preset). Key deps: `cppgraphqlgen` (graphqlpeg), `nlohmann-json`, `pegtl`, `gtest`, `better-enums`.

## Architecture

Ariane is a header-only-style C++20 GraphQL library. The public surface lives entirely in `include/ariane/`:

- **`schema.h`** — `Schema` is the entry point. Constructed with `SchemaOptions` (SDL `typeDefs` string + `Resolver` map + `allowIntrospection` flag). `Schema::Resolve()` returns `Task<ResolveResult>` (a coroutine); call `.get()` for synchronous use.
- **`resolvers.h`** — `ValueResolver` is a recursive `std::variant` accepting scalars (`int`, `uint64_t`, `double`, `float`, `bool`, `std::string`), nested `Resolver` (alias for `unordered_map<string, ValueResolver>`), `vector<ValueResolver>`, and four async resolver function types (see below).
- **`task.h`** — `Task<T>`: a minimal C++20 coroutine type used internally and exposed for `CoroutineResolver`.

Internal implementation (`src/ariane/internal/`):
- `parser/` — Parses SDL type definitions into a `Document` using `cppgraphqlgen::graphqlpeg`.
- `introspection/` — Builds introspection resolvers from the parsed `Document`; injected automatically when `allowIntrospection = true`.
- `utils/visit.h` — Visitor helpers for `ValueResolver` variant dispatch.
- `peg/` — Thin wrappers around `cppgraphqlgen` PEG nodes.

Resolution flow: `Schema::Resolve` parses the query with `graphqlpeg`, walks the selection set, dispatches each field against the `Resolver` map, and serializes to JSON via `nlohmann/json`.

## Key Conventions

**Resolver function types** — four variants are supported; pick the one that fits your async model:
```cpp
FunctionResolver      = std::function<ValueResolver()>              // sync lambda
AsyncFunctionResolver = std::function<std::future<ValueResolver>()> // std::future
CoroutineResolver     = std::function<Task<ValueResolver>()>        // C++20 coroutine
CallbackResolver      = std::function<void(std::function<void(const ValueResolver&)>)> // callback
```

**Returning null** — use `std::nullopt`, `nullptr`, or `std::monostate{}` as a `ValueResolver`; all map to GraphQL `null`.

**Nested resolvers** — use brace-initializer syntax; the `ValueResolver` constructors handle `initializer_list<pair<string, ValueResolver>>` → `Resolver` and `initializer_list<ValueResolver>` → `vector<ValueResolver>` automatically.

**`clang-format`** — style is defined in `.clang-format` at the repo root. The `// clang-format off/on` guards are used around deeply-nested resolver literals in samples.

**Tests** — Google Test with fixtures (`testing::Test` subclasses). Tests access `Schema::GetDocument()` directly to assert on the parsed AST; the `internal` headers are included in test targets via `target_include_directories(...PRIVATE ${CMAKE_SOURCE_DIR}/src)`.

**C++ standards** — C++20 required (`cxx_std_20`). Prefer `std::optional` over raw pointers; no raw owning pointers; `const&` by default; functions ≤ 20 lines.

**Code Quality**
- Don't comment the code unless necessary. Code must be readable by itself
- Favor structs instead of multiple parameters to improve code readability and extensibility
- Functions have to be concise. 20 lines max.
- Use DRY principles