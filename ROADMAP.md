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

### P4 — Subscriptions

Implement the GraphQL subscription execution path as defined by the June 2018+ spec. Subscriptions establish a long-lived event source that maps each published event to a response stream.

| # | Feature | Notes |
|---|---------|-------|
| 20 | **`SubscriptionResolver` type** | A new resolver variant backed by an async generator / coroutine that yields `ValueResolver` events. Distinct from the four existing function resolver types. |
| 21 | **`Subscribe()` execution path** | `Schema::Subscribe(SchemaResolveArgs)` returns an event stream handle. Walks the selection set once, identifies the single root subscription field, and hooks into its `SubscriptionResolver`. |
| 22 | **Source stream → response stream mapping** | Each event emitted by the source stream is run through the normal field-execution logic (argument binding, nested resolvers, error handling) to produce a `ResolveResult`. |
| 23 | **Single root field enforcement** | The spec forbids subscription documents with more than one root field (excluding `__typename`). Reject such documents with a structured validation error before execution. |
| 24 | **Subscription error handling** | Errors during event execution must not terminate the stream; they should produce a standard `{"data": null, "errors": [...]}` payload for that event, leaving the stream open. |
| 25 | **Unsubscribe / stream cancellation** | The stream handle returned by `Subscribe()` must support cancellation. Cancellation propagates to the `SubscriptionResolver` coroutine to release resources. |

---

### P5 — Schema Stitching

Merge multiple independent `Schema` instances into a single unified schema via `Schema::Stitch()`.

| # | Feature | Notes |
|---|---------|-------|
| 26 | **SDL merging** | Types from both schemas are merged at the `SchemaDefinition` level. Duplicate non-root type names produce a `runtime_error`. Built-in scalars and introspection types are deduplicated silently. |
| 27 | **Resolver map merging** | Resolver maps are deep-merged. Conflicting user-defined field resolvers produce a `runtime_error`; `__`-prefixed system fields are safely deduplicated. |
| 28 | **Root type field merging** | `Query`, `Mutation`, and `Subscription` fields from both schemas are combined automatically — no `extend type` syntax required. |
| 29 | **Introspection consistency** | After stitching, `__schema` and `__type` are re-injected pointing at the merged type map, so introspection correctly reflects all types from both schemas. |
| 30 | **`Schema::Stitch()` API** | `schema.Stitch(other)` returns a new `Schema`. Chains naturally: `a.Stitch(b).Stitch(c)`. |

---

### SDL `extend` keyword

Support the GraphQL `extend type` SDL keyword (and the equivalent for interfaces, unions, enums, and input types) directly within a schema's `typeDefs` string.

| # | Feature | Notes |
|---|---------|-------|
| 37 | **`extend type`** | Appends fields and implemented interfaces to an existing object type. |
| 38 | **`extend interface`** | Appends fields to an existing interface type. `possibleTypes` linking is updated accordingly. |
| 39 | **`extend union`** | Appends member types to an existing union. |
| 40 | **`extend enum`** | Appends values to an existing enum type. |
| 41 | **`extend input`** | Appends fields to an existing input object type. |
| 42 | **Unknown type silently ignored** | Extending a type that is not defined in the same SDL is silently ignored (no throw). |

---

### P6 — Federation

Implement the Apollo Federation subgraph specification so that an GQLXY schema can act as a subgraph inside a federated supergraph (Apollo Router, Apollo Gateway).

| # | Feature | Notes |
|---|---------|-------|
| 31 | **Federation SDL directives** | Parse and register `@key`, `@external`, `@requires`, `@provides`, and `@extends` in the SDL type system. These must survive round-tripping through `__Service.sdl`. |
| 32 | **`_service` query** | Auto-inject a root `_service: _Service!` field that returns `{ sdl }` — the full annotated SDL string of the subgraph, required by the gateway for schema composition. |
| 33 | **`_entities` query** | Auto-inject `_entities(representations: [_Any!]!): [_Entity]!`. For each `__typename` + key fields representation, dispatch to the matching `@key` entity resolver and return the resolved object. |
| 34 | **Entity resolver API** | Entity resolvers use `__resolveReference` — a `FunctionResolver` placed directly in the entity type's `Resolver` map (e.g. `{"User", Resolver{{"__resolveReference", fn}}}`). Apollo Server-style: no separate resolver map, `@key` types and their reference resolvers live side by side in `resolvers`. |
| 35 | **Federation v2 support** | Support the `@link` directive and `@federation` import syntax introduced in Federation v2, allowing the subgraph to declare which federation spec version it targets. |
| 36 | **Composition hints** | Validate that every type annotated with `@key` has a corresponding entity resolver registered; emit a structured error at `Schema` construction time if not. |

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

### P10 — Standalone Server (`standalone-server` vcpkg feature)

An Apollo-style `StartStandaloneServer` equivalent backed by **oatpp**. Enabled as an opt-in vcpkg feature (`standalone-server`) that pulls in `oatpp` + `oatpp-websocket`.  All four GraphQL transports are served on a single port and path.

| # | Feature | Notes |
|---|---------|-------|
| 43 | **HTTP GraphQL transport** | `POST /graphql` with `application/json` body (`query`, `variables`, `operationName`). `GET /graphql?query=...` for read-only operations. Response `Content-Type: application/graphql-response+json`. CORS preflight (`OPTIONS`) handled automatically. |
| 44 | **`graphql-transport-ws`** | WebSocket subprotocol `graphql-transport-ws` (modern, used by Apollo Client 3+, Relay, Insomnia). Messages: `connection_init` / `connection_ack`, `subscribe`, `next`, `error`, `complete`, `ping` / `pong`. Per-subscription async threads; mutex-protected sends. |
| 45 | **`graphql-ws`** (legacy) | WebSocket subprotocol `graphql-ws` (legacy `subscriptions-transport-ws` protocol). Messages: `connection_init` / `connection_ack`, `start`, `data`, `stop`, `complete`, `connection_terminate`. Auto-detected from the first operation message (`start` → `graphql-ws`, `subscribe` → `graphql-transport-ws`). |
| 46 | **`graphql-sse`** | Server-Sent Events via `Accept: text/event-stream` on the same path (distinct-connections mode). Streams `connection_ack`, `next`, and `complete` events. Works without WebSocket support (firewalls, proxies). |
| 47 | **`StandaloneServer` API** | `StandaloneServer({.schema, .host, .port, .path})`. `Start()` blocks; `StartAsync()` returns immediately; `Stop()` terminates the server; `GetUrl()` returns the base URL. oatpp `Environment` is managed internally with a ref-counted init/destroy. |
| 48 | **Demo server** (`samples/demo-server`) | A fully wired books-and-reviews API with `Query` (books, book, reviews), `Mutation` (addBook, addReview), and `Subscription` (reviewAdded, bookAdded). Reachable out-of-the-box from Insomnia, Postman, Apollo Studio, and any GraphQL client. |
