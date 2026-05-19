# GQLXY — Engine Roadmap

This document tracks what is already working and what remains to be built for a fully spec-compliant GraphQL execution engine (transport layer excluded).

---

## What works today

- SDL schema parsing — object, interface, union, enum, scalar, input types
- Rich `ValueResolver` variant accepting sync functions, `std::future`, coroutines, and callbacks
- Basic query execution: operation → selection set → field → nested resolver
- Shorthand / anonymous queries treated as implicit `query`
- Field arguments parsed from the query AST and forwarded to resolvers via `ResolverArgs`
- Variable substitution — `$var` references resolved from the `variables` JSON passed to `Resolve()`
- Full introspection: `__schema`, `__typename`, `__type`, all meta-types and built-in scalars auto-registered
- `schema {}` definition parsing (`queryType`, `mutationType`, `subscriptionType`)
- SDL description parsing (block-string and single-line)
- Input object `inputFields` populated
- Argument default values parsed
- Directive declarations in SDL (`directive @foo on LOCATIONS`)
- JSON string escaping (via `nlohmann::json`)
- Named fragments (`fragment Foo on Bar { … }`) and fragment spreads (`…Foo`)
- Inline fragments (`… on Bar { … }`)
- Field aliases — `alias: fieldName` correctly used as the JSON response key
- Per-field error handling — resolver exceptions set the field to `null` and append a structured entry (with `message` and `path`) to `errors[]`; other fields continue resolving
- Abstract type resolution — `TypeResolver` determines the concrete type of a union/interface field at runtime; inline fragments and named fragment spreads are filtered by the resolved concrete type; `__typename` always reflects the actual runtime type
- `@skip` / `@include` directives — built-in runtime conditional field inclusion evaluated before resolution; custom directives registered via `SchemaOptions::directives` as `DirectiveResolver = std::function<bool(const ResolverArgs&)>`; applied to fields, inline fragments, and named fragment spreads; variable argument substitution supported
- Custom scalars — `ScalarType` base class for output serialization; `ScalarResolver` functions in `SchemaOptions::scalars` for input coercion of scalar arguments; unregistered scalars pass through as-is
- Resolver context — `SchemaResolveArgs::context` (`std::any`) is threaded through execution and available on every `ResolverArgs::context`; carries request-scoped state (auth, DB connection, etc.) without coupling the library to a concrete type
- Serial mutation execution — top-level mutation fields execute strictly in document order (spec §6.3.1); each field is fully resolved before the next begins
- Operation name selection — `SchemaResolveArgs::operationName` selects a named operation when the document contains multiple; omitting it with a multi-operation document returns a structured error
- Query validation — before execution, the document is validated against the schema: undeclared variables (#16), missing non-nullable variables (#17), unknown fields/arguments (#18), required arguments not supplied (#19)


---

### ✅ Subscriptions

Subscription execution is implemented per the June 2018+ spec. Subscriptions establish a long-lived event source that maps each published event to a response stream.

| # | Feature | Status |
|---|---------|-------|
| 20 | **`SubscriptionResolver` type** | ✅ Implemented — `SubscriptionResolver = std::function<SubscriptionEventStream(const ResolverArgs&)>` in `resolvers.h` |
| 21 | **`Subscribe()` execution path** | ✅ Implemented — `Schema::Subscribe(SchemaResolveArgs)` returns a `SubscriptionHandle` |
| 22 | **Source stream → response stream mapping** | ✅ Implemented — each event flows through full field-execution and produces a `GraphQLResponse` |
| 23 | **Single root field enforcement** | ✅ Implemented |
| 24 | **Subscription error handling** | ✅ Implemented — errors produce a standard error payload without terminating the stream |
| 25 | **Unsubscribe / stream cancellation** | ✅ Implemented — `SubscriptionHandle::Cancel()` unblocks pending `Next()` and releases resources |

---

### ✅ Schema Stitching

Multiple independent `Schema` instances can be merged into a single unified schema via `Schema::Stitch()`.

| # | Feature | Status |
|---|---------|-------|
| 26 | **SDL merging** | ✅ Implemented — types from both schemas are merged at the `SchemaDefinition` level; duplicate non-root type names produce a `runtime_error` |
| 27 | **Resolver map merging** | ✅ Implemented — resolver maps are deep-merged; conflicting user-defined field resolvers produce a `runtime_error` |
| 28 | **Root type field merging** | ✅ Implemented — `Query`, `Mutation`, and `Subscription` fields from both schemas are combined automatically |
| 29 | **Introspection consistency** | ✅ Implemented — after stitching, `__schema` and `__type` reflect all types from both schemas |
| 30 | **`Schema::Stitch()` API** | ✅ Implemented — `schema.Stitch(other)` returns a new `Schema`; chains naturally: `a.Stitch(b).Stitch(c)` |

---

### ✅ SDL `extend` keyword

The `extend type` SDL keyword is supported for all type kinds.

| # | Feature | Status |
|---|---------|-------|
| 37 | **`extend type`** | ✅ Implemented — appends fields and implemented interfaces to an existing object type |
| 38 | **`extend interface`** | ✅ Implemented — appends fields to an existing interface type |
| 39 | **`extend union`** | ✅ Implemented — appends member types to an existing union |
| 40 | **`extend enum`** | ✅ Implemented — appends values to an existing enum type |
| 41 | **`extend input`** | ✅ Implemented — appends fields to an existing input object type |
| 42 | **Unknown type silently ignored** | ✅ Implemented — extending an undefined type is silently ignored |

---

### ✅ Federation

GQLXY acts as an Apollo Federation subgraph (`@key`, `_service`, `_entities`). Enable with `SchemaOptions::federation = true`.

| # | Feature | Status |
|---|---------|-------|
| 31 | **Federation SDL directives** | ✅ Implemented — `@key`, `@external`, `@requires`, `@provides`, `@extends` are parsed and preserved in `__Service.sdl` |
| 32 | **`_service` query** | ✅ Implemented — auto-injected `_service: _Service!` returns the full annotated SDL |
| 33 | **`_entities` query** | ✅ Implemented — auto-injected `_entities(representations: [_Any!]!): [_Entity]!` dispatches to `__resolveReference` resolvers |
| 34 | **Entity resolver API** | ✅ Implemented — `__resolveReference` placed in the entity type's `Resolver` map |
| 35 | **Federation v2 support** | ✅ Implemented — `@link` directive and `@federation` import syntax supported |
| 36 | **Composition hints** | ✅ Implemented — structured error at `Schema` construction if a `@key` type has no entity resolver |

---

## Suggested implementation order

```
Phase 1 — Core correctness   : #1, #2, #3  ✓
Phase 2 — Spec compliance    : #11, #6, #7, #8, #9, #10 ✓
Phase 3 — Type system        : #12 ✓
Phase 4 — Ergonomics         : #13, #14, #15 ✓
Phase 5 — Query validation   : #16, #17, #18, #19 ✓
Phase 6 — Subscriptions      : #20, #21, #22, #23, #24, #25 ✓
Phase 7 — Schema stitching   : #26, #27, #28, #29, #30 ✓
Phase 8 — SDL extend         : #37, #38, #39, #40, #41, #42 ✓
Phase 9 — Federation         : #31, #32, #33, #34, #35, #36 ✓
Phase 10 — Standalone Server : #43, #44, #45, #46, #47, #48 ✓
```

---

### ✅ Standalone Server (`standalone-server` vcpkg feature)

An Apollo-style `StartStandaloneServer` equivalent backed by **oatpp**. Enabled as an opt-in vcpkg feature (`standalone-server`) that pulls in `oatpp` + `oatpp-websocket` + `oatpp-openssl`. All four GraphQL transports are served on a single port and path.

| # | Feature | Status |
|---|---------|-------|
| 43 | **HTTP GraphQL transport** | ✅ Implemented — `POST /graphql` with `application/json` body; `GET /graphql?query=...` for read-only; `Content-Type: application/graphql-response+json`; CORS preflight auto-handled |
| 44 | **`graphql-transport-ws`** | ✅ Implemented — modern WebSocket subprotocol used by Apollo Client 3+, Relay, Insomnia |
| 45 | **`graphql-ws`** (legacy) | ✅ Implemented — legacy `subscriptions-transport-ws` protocol; auto-detected from first message |
| 46 | **`graphql-sse`** | ✅ Implemented — Server-Sent Events via `Accept: text/event-stream`; distinct-connections mode |
| 47 | **`StandaloneServer` API** | ✅ Implemented — `StandaloneServer({.schema, .host, .port, .path, .tls, .mcp})`; `Start()` blocks; `StartAsync()` returns immediately; `Stop()` terminates; `GetUrl()` returns base URL; TLS via `TlsOptions`; MCP via `McpServerOptions` |
| 48 | **Demo server** (`samples/demo-server`) | ✅ Implemented — books-and-reviews API with `Query`, `Mutation`, and `Subscription` |
