# Ariane — Engine Roadmap

This document tracks what is already working and what remains to be built for a fully spec-compliant GraphQL execution engine (transport layer excluded).

---

## What works today

- SDL schema parsing — object, interface, union, enum, scalar, input types
- Rich `ValueResolver` variant accepting sync functions, `std::future`, coroutines, and callbacks
- Basic query execution: operation → selection set → field → nested resolver
- Partial introspection: `__schema` and `__typename`

---

## What's missing

### P0 — Breaks basic spec compliance

These must be fixed before the engine can be considered usable.

| # | Feature | Notes |
|---|---------|-------|
| 1 | **Shorthand / anonymous queries** | `{ hello }` has no `operation_type` AST node; the entire operation is silently skipped. Needs to be treated as an implicit `query`. |
| 2 | **Field arguments** | `FunctionResolver` is `std::function<ValueResolver()>` — resolvers receive nothing. Arguments from the query must be parsed and forwarded. |
| 3 | **Variable substitution** | `Schema::Resolve()` already accepts a `variables` map but never uses it. `$var` references in argument positions must be resolved at execution time. Depends on #2. |
| 4 | **JSON string escaping** | Strings are streamed raw; `"`, `\`, `\n`, etc. are not escaped, producing invalid JSON. |
| 5 | **`schema {}` definition parsing** | `schema { query: Query mutation: Mutation }` declarations are never parsed, so `Document::queryTypeName` etc. are always empty and `__schema.queryType` always returns null. |

---

### P1 — Required by the GraphQL specification

| # | Feature | Notes |
|---|---------|-------|
| 6 | **`__type` introspection** | The spec mandates `__type(name: String!): __Type` on every query root. Only `__schema` is injected today. Depends on #2. |
| 7 | **Named fragments** | `fragment Foo on Bar { … }` declarations and `…Foo` spreads are silently ignored during execution. |
| 8 | **Inline fragments** | `… on Bar { … }` inside selection sets are silently ignored. Depends on #7. |
| 9 | **Field aliases** | `alias: fieldName` is not handled; the field name is always used as the JSON response key. |
| 10 | **`@skip` / `@include` directives** | Runtime conditional field inclusion — must be evaluated before a field is resolved. Depends on #2. |
| 11 | **Per-field error handling** | Any resolver exception currently crashes the whole request. Field errors must be caught, the field set to `null`, and an entry appended to `errors[]`. |
| 12 | **Abstract type resolution (`__resolveType`)** | No mechanism exists to determine the concrete type of a union or interface field at runtime, making `__typename` and conditional fragment spreading unreliable. Depends on #8. |

---

### P2 — Completeness and developer ergonomics

| # | Feature | Notes |
|---|---------|-------|
| 13 | **Resolver context** | Resolvers have no way to receive a request-scoped context (auth, DB connection, etc.). A `Context` type should be threaded through the execution. |
| 14 | **Input object field parsing** | `INPUT_OBJECT` types are registered in the document but their `inputFields` vector is never populated — `ParseType` doesn't call `ParseFields` for them. |
| 15 | **Argument default values** | `ParseInputValue` never reads the `default_value` SDL node. `InputValueDefinition::defaultValue` is always empty. |
| 16 | **SDL description parsing** | Block-string and single-line descriptions are never captured into `TypeDefinition::description`, `FieldDefinition::description`, etc. |
| 17 | **Built-in types auto-registration** | `String`, `Int`, `Float`, `Boolean`, `ID` scalars and the `__Schema`, `__Type`, `__Field`, `__InputValue`, `__EnumValue`, `__Directive` meta-types should be auto-injected so introspection clients (e.g. GraphiQL) work out of the box. |
| 18 | **Serial mutation execution** | The spec requires top-level mutation fields to execute strictly in document order. |
| 19 | **Directive declarations in SDL** | `directive @foo(args) on LOCATIONS` is never parsed into `Document::directives`. |
| 20 | **Operation name selection** | `Resolve()` should accept an `operationName` parameter and select the matching named operation when the document contains multiple operations. |

---

## Suggested implementation order

```
Phase 1 — Core correctness   : #1, #4, #5, #2, #3
Phase 2 — Spec compliance    : #11, #6, #7, #8, #9, #10
Phase 3 — Type system        : #12, #14, #15, #17, #19
Phase 4 — Ergonomics         : #13, #16, #18, #20
```
