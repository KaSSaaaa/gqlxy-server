# GraphQL Introspection Sample

This sample demonstrates GraphQL introspection queries using the Ariane GraphQL server.

## Features

- Full introspection query support
- JSON-formatted output using nlohmann::json
- `__typename` field support for type information
- Schema exploration at runtime

## Building

```bash
cmake --preset arm64-debug
ninja -C out/build/arm64-debug
```

## Running

```bash
./out/build/arm64-debug/samples/introspection/introspection
```

## Example Output

The sample executes two queries:

1. **Introspection Query**: Returns the complete schema structure including:
   - All types (Query, User, Role)
   - Fields and their types
   - Enum values
   - Field arguments

2. **Regular Query with __typename**: Demonstrates using `__typename` to get runtime type information for queried objects.
