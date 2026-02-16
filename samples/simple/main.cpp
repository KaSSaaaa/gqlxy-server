#include <ariane/resolvers.h>
#include <ariane/schema.h>

#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <string>

using namespace std;
using namespace ariane::graphql;

int main() {
    std::pair<std::string, ValueResolver> field1{"hello", []() { return "Hello, world!"; }};

    // clang-format off
    Schema schema(SchemaOptions {
        .typeDefs = R"(
            type Query {
                hello: String
                user: User
            }

            type User {
                id: Int
                name: String
                email: String
                world: String
            }
        )",
        .resolvers = {
            {"Query", Resolver {
                {"hello", []() { return "Hello, world!"; }},
                {"user", Resolver {
                    {"id", 123},
                    {"name", "John Doe"},
                    {"email", "john@example.com"},
                    field1,
                    {"world", []() {
                        return async(launch::async, []() -> ValueResolver { return "Async world!"; });
                    }}
                }}
            }}
        }
    });
    // clang-format on

    return 0;
}
