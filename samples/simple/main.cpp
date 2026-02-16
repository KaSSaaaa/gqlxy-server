#include <iostream>
#include <map>
#include <string>
#include <functional>
#include <memory>
#include <future>
#include <ariane/resolvers.h>
#include <ariane/schema.h>

using namespace std;
using namespace ariane::graphql;

int main()
{
    std::pair<std::string, ValueResolver> field1 { "hello", []() { return "Hello, world!"; }};
    auto resolvers = Resolver {
        {"Query", Resolver {
            {"hello", []() { return "Hello, world!"; }},
            {"user", Resolver {
                {"id", 123},
                {"name", "John Doe"},
                {"email", "john@example.com"},
                field1,
                {"world", []() { return async(launch::async, []() -> ValueResolver { return "Async world!"; }); }},
            }}
        }}
    };

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
                    {"world", []() { return async(launch::async, []() -> ValueResolver { return "Async world!"; }); }},
                }}
            }}
        }
    });
    
    return 0;
}