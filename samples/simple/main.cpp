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
            interface Node {
                id: ID!
            }

            type Query {
                hello: String
                user: User
            }

            type User {
                id: Int
                name: String
                email: String
                world: String
                coroutineWorld: String
                callbackWorld: String
            }

            union SearchResult = User | Node
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
                    }},
                    {"coroutineWorld", []() -> Task<ValueResolver> {
                        co_return "Coroutine world!";
                    }},
                    {"callbackWorld", [](const function<void(const ValueResolver&)>& callback) {
                        callback("Callback world!");
                    }},
                    {"list", initializer_list<ValueResolver> {
                        "item1",
                        2,
                        Resolver {
                            {"nestedField", "Nested value"}
                        },
                        [] {
                            return std::optional<ValueResolver>("Optional value");
                        }
                    }},
                    {"listFunction", []() {
                        return vector<ValueResolver> {
                            "item1",
                            Resolver {
                                {"nestedField", "Nested value"}
                            },
                            []() {
                                return std::optional<ValueResolver>("Optional value");
                            }
                        };
                    }}
                }}
            }}
        }
    });
    // clang-format on

    auto doc = schema.GetDocument();

    return 0;
}
