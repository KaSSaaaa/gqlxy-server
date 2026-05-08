#include <gqlxy/server/resolver_args.h>
#include <gqlxy/server/resolvers.h>
#include <gqlxy/server/schema.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using namespace std;
using namespace gqlxy;

static void printResult(const string& label, const GraphQLResponse& result) {
    cout << "--- " << label << " ---" << endl;
    if (result.errors.has_value()) {
        for (const auto& e : result.errors.value())
            cerr << "Error: " << e.message << endl;
        return;
    }
    cout << result.data.value().dump(2) << endl << endl;
}

int main() {
    cout << "=== GQLXY — Fragments sample ===" << endl << endl;

    // clang-format off
    Schema schema({
        .typeDefs = R"(
            type Query {
                user(id: ID!): User
                post(id: ID!): Post
            }
            type User {
                id: ID!
                name: String!
                email: String!
                bio: String
            }
            type Post {
                id: ID!
                title: String!
                body: String!
                author: User!
            }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"user", FunctionResolver([](const ResolverArgs& args) -> ValueResolver {
                    static const unordered_map<string, Resolver> users = {
                        {"1", {{"id","1"}, {"name","Alice"},   {"email","alice@example.com"}, {"bio","GraphQL enthusiast"}}},
                        {"2", {{"id","2"}, {"name","Bob"},     {"email","bob@example.com"},   {"bio","C++ developer"}}},
                    };
                    auto it = users.find(args.Args()["id"].get<string>());
                    if (it != users.end()) return it->second;
                    return nullopt;
                })},
                {"post", FunctionResolver([](const ResolverArgs& args) -> ValueResolver {
                    if (args.Args()["id"].get<string>() != "42") return nullopt;
                    return Resolver{
                        {"id",    "42"},
                        {"title", "Fragments in GraphQL"},
                        {"body",  "Fragments let you reuse selection sets across queries."},
                        {"author", Resolver{
                            {"id",    "1"},
                            {"name",  "Alice"},
                            {"email", "alice@example.com"},
                            {"bio",   "GraphQL enthusiast"},
                        }},
                    };
                })},
            }}
        }
    });
    // clang-format on

    // 1. Named fragment spread on a top-level field
    printResult("Named fragment on user", schema.Resolve({
        .query = R"(
            fragment UserCard on User {
                id
                name
                email
            }
            query GetUser($id: ID!) {
                user(id: $id) {
                    ...UserCard
                    bio
                }
            }
        )",
        .variables = {{"id", "2"}}
    }).get());

    // 2. Same fragment reused on a nested field
    printResult("Fragment reused on nested author field", schema.Resolve({
        .query = R"(
            fragment UserCard on User {
                id
                name
                email
            }
            query GetPost($id: ID!) {
                post(id: $id) {
                    title
                    body
                    author {
                        ...UserCard
                    }
                }
            }
        )",
        .variables = {{"id", "42"}}
    }).get());

    // 3. Inline fragment
    printResult("Inline fragment on user", schema.Resolve({
        .query = R"(
            query GetUser($id: ID!) {
                user(id: $id) {
                    id
                    ... on User {
                        name
                        bio
                    }
                }
            }
        )",
        .variables = {{"id", "1"}}
    }).get());

    return 0;
}
