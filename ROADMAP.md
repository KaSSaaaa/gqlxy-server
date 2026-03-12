# Ariane — Engine Roadmap

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

Merge multiple independent `Schema` instances into a single unified schema. All merging happens at library level without a network gateway.

| # | Feature | Notes |
|---|---------|-------|
| 26 | **SDL merging** | Combine `typeDefs` strings from multiple schemas, detecting and rejecting duplicate type names with a clear error. Scalar and directive definitions may be de-duplicated safely. |
| 27 | **Resolver map merging** | Merge `Resolver` maps from all sub-schemas. Conflicting root-type field names must be flagged as errors; nested type resolvers are merged recursively. |
| 28 | **Type extension stitching** | Support `extend type Foo { … }` SDL to add fields from one schema onto a type declared in another, enabling cross-schema field composition. |
| 29 | **Delegation** | When a stitched field's resolver is absent in the merged map, delegate resolution to the originating sub-schema's `Resolve()` path rather than returning null. |
| 30 | **`SchemaStitcher` API** | A `SchemaStitcher` builder that accepts multiple `Schema` references and produces a new merged `Schema`: `SchemaStitcher{}.add(a).add(b).stitch()`. |

---

### P6 — Federation

Implement the Apollo Federation subgraph specification so that an Ariane schema can act as a subgraph inside a federated supergraph (Apollo Router, Apollo Gateway).

| # | Feature | Notes |
|---|---------|-------|
| 31 | **Federation SDL directives** | Parse and register `@key`, `@external`, `@requires`, `@provides`, and `@extends` in the SDL type system. These must survive round-tripping through `__Service.sdl`. |
| 32 | **`_service` query** | Auto-inject a root `_service: _Service!` field that returns `{ sdl }` — the full annotated SDL string of the subgraph, required by the gateway for schema composition. |
| 33 | **`_entities` query** | Auto-inject `_entities(representations: [_Any!]!): [_Entity]!`. For each `__typename` + key fields representation, dispatch to the matching `@key` entity resolver and return the resolved object. |
| 34 | **Entity resolver API** | A new `EntityResolver = std::function<ValueResolver(const ResolverArgs&)>` registered per entity type in `SchemaOptions`. Receives the key fields as `args` and returns the hydrated object. |
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
Phase 7 — Schema stitching   : #26, #27, #28, #29, #30
Phase 8 — Federation         : #31, #32, #33, #34, #35, #36
```
