# Contributing to Ariane

Thank you for your interest in contributing to Ariane! This document explains how to get involved.

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally:
   ```sh
   git clone https://github.com/<your-username>/ariane-graphql-server.git
   cd ariane-graphql-server
   ```
3. **Create a branch** from `main` for your work:
   ```sh
   git checkout -b feat/my-feature
   ```
4. **Build** and make sure everything passes before you start:
   ```sh
   cmake --preset arm64-debug
   cmake --build out/build/arm64-debug
   ctest --test-dir out/build/arm64-debug --output-on-failure
   ```

## Making Changes

### Coding Conventions

- **C++20** — the project requires `cxx_std_20`.
- **Header-only public API** — all public headers live in `include/ariane/`.
- **Functions ≤ 20 lines** — keep functions concise and focused.
- **Prefer structs over multiple parameters** for better readability and extensibility.
- **`const&` by default** — pass arguments by const reference unless there is a reason not to.
- **`std::optional` over raw pointers** — no raw owning pointers.
- **Minimal comments** — code should be readable on its own. Only comment when something genuinely needs clarification.
- **DRY** — avoid code duplication.

### Formatting

The project uses `clang-format`.

Use `// clang-format off` / `// clang-format on` guards around deeply-nested resolver literals if needed.

### Commit Style

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short summary>

<optional body>
```

**Types:** `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`, `ci`

**Examples:**
```
feat(resolvers): add support for union types
fix(parser): handle empty input document gracefully
docs(readme): update build instructions for Linux
test(introspection): cover __type with interfaces
```

- Keep the summary line under 72 characters.
- Use the imperative mood ("add support", not "added support").
- Reference issue numbers in the body when applicable (e.g., `Closes #42`).

## Submitting a Pull Request

1. **Push** your branch to your fork:
   ```sh
   git push origin feat/my-feature
   ```
2. **Open a Pull Request** against `main` on the upstream repository.
3. Fill out the PR template — it includes a checklist to help you.
4. Make sure CI passes (build + tests).
5. A maintainer will review your PR. Be open to feedback and iterate if needed.

## Reporting Bugs & Requesting Features

Use the [issue templates](https://github.com/KaSSaaaa/ariane-graphql-server/issues/new/choose) — there are separate forms for bug reports and feature requests.

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
