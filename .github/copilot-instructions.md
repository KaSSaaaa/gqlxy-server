---
description: 'Instructions for writing C++ code following idiomatic C++ practices, OOP principles and community standards'
applyTo: '**/*.cpp, **/*.h, **/*.cppm, **/*.ixx'
---

# C++ Development Instructions

Communicate at a senior C++ engineer level, assuming deep knowledge of OOP and modern C++ best practices.
You also master GraphQL.

## General Guidelines

- Favor clarity and simplicity over cleverness.
- Only make high-confidence suggestions when reviewing code.
- Prefer `std::unique_ptr` / `std::shared_ptr` over raw pointers; avoid raw pointers unless unavoidable.
- Use `std::optional` for values that may be absent.
- Pass parameters by `const&` by default.
- Use forward declarations where possible.
- Use `#include <...>` for all headers except the one paired with the current `.cpp` file.
- Always declare a virtual destructor in interface/abstract classes.
- Never return `nullptr`; prefer `std::optional` or throw.

## Libraries and Dependencies

- Prefer standard library facilities; reach for well-known third-party libraries only when the standard library falls short.

## Architecture and Design

- Follow the existing architecture and design patterns in the codebase.
- Apply established design patterns where they genuinely simplify the problem.
- Avoid over-engineering; resist adding abstraction layers without a clear reason.
- Apply SOLID, Clean Code, and good OOD principles.

## Tests

- Use Google Test with test fixtures.
- Write unit tests for all new features and bug fixes.
- Keep tests clear, concise, and self-documenting.

## Code quality

- Don't comment the code unless necessary. Code must be readable by itself
- Favor structs instead of multiple parameters to improve code readability and extensibility
- Functions have to be concise. 20 lines max.