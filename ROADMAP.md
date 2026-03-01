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

---

## What's missing

### P0 — Required by the GraphQL specification

| # | Feature | Notes |
|---|---------|-------|
| 7 | **Named fragments** | `fragment Foo on Bar { … }` declarations and `…Foo` spreads are silently ignored during execution. |
| 8 | **Inline fragments** | `… on Bar { … }` inside selection sets are silently ignored. Depends on #7. |
| 9 | **Field aliases** | `alias: fieldName` is not handled; the field name is always used as the JSON response key. |
| 10 | **`@skip` / `@include` directives** | Runtime conditional field inclusion — must be evaluated before a field is resolved. |
| 11 | **Per-field error handling** | Any resolver exception currently crashes the whole request. Field errors must be caught, the field set to `null`, and an entry appended to `errors[]`. |
| 12 | **Abstract type resolution (`__resolveType`)** | No mechanism exists to determine the concrete type of a union or interface field at runtime, making `__typename` and conditional fragment spreading unreliable. Depends on #8. |

---

### P2 — Completeness and developer ergonomics

| # | Feature | Notes |
|---|---------|-------|
| 13 | **Resolver context** | Resolvers have no way to receive a request-scoped context (auth, DB connection, etc.). A `Context` type should be threaded through the execution. |
| 14 | **Serial mutation execution** | The spec requires top-level mutation fields to execute strictly in document order. |
| 15 | **Operation name selection** | `Resolve()` should accept an `operationName` parameter and select the matching named operation when the document contains multiple operations. |

---

### P3 — Query validation

A validation layer that runs before execution and rejects invalid documents with structured errors.

| # | Feature | Notes |
|---|---------|-------|
| 16 | **Variable declaration validation** | Variables used in argument positions (e.g. `$id`) must be declared in the operation's variable definitions (`query Foo($id: ID!)`). Undeclared variables should be rejected rather than silently substituted. |
| 17 | **Variable type coercion & nullability** | Declared variable types must be coercible to the argument type. Non-nullable variables without a default must be provided. |
| 18 | **Unknown field / argument validation** | Fields and arguments referenced in a query must exist on the schema type; unknown names should be rejected with a structured error. |
| 19 | **Required argument validation** | Non-nullable arguments without a default value must be supplied at the call site. |

---

## Suggested implementation order

```
Phase 1 — Core correctness   : #1, #2, #3  ✓
Phase 2 — Spec compliance    : #11, #6 ✓, #7, #8, #9, #10
Phase 3 — Type system        : #12
Phase 4 — Ergonomics         : #13, #14, #15
Phase 5 — Query validation   : #16, #17, #18, #19
```
