#include <gqlxy/resolver_args.h>
#include <gqlxy/schema.h>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy;
using json = nlohmann::json;

static Schema makeUsersSchema() {
    // clang-format off
    return Schema({
        .typeDefs = R"(
            type Query {
                user(id: ID!): User
                users: [User]
            }
            type User { id: ID name: String email: String }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"user", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                    auto id = r.Args()["id"].get<string>();
                    return Resolver{
                        {"id", id},
                        {"name", "Alice"},
                        {"email", "alice@example.com"}
                    };
                }}},
                {"users", vector<ValueResolver>{
                    Resolver{
                        {"id", "1"},
                        {"name", "Alice"},
                        {"email", "alice@example.com"}
                    },
                    Resolver{
                        {"id", "2"},
                        {"name", "Bob"},
                        {"email", "bob@example.com"}
                    }
                }}
            }}
        }
    });
    // clang-format on
}

static Schema makePostsSchema() {
    // clang-format off
    return Schema({
        .typeDefs = R"(
            type Query {
                post(id: ID!): Post
                posts: [Post]
            }
            type Post { id: ID title: String authorId: ID }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"post", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                    auto id = r.Args()["id"].get<string>();
                    return Resolver{
                        {"id", id},
                        {"title", "Hello GQLXY"},
                        {"authorId", "1"}
                    };
                }}},
                {"posts", vector<ValueResolver>{
                    Resolver{
                        {"id", "1"},
                        {"title", "Hello GQLXY"},
                        {"authorId", "1"}
                    },
                    Resolver{
                        {"id", "2"},
                        {"title", "Schema Stitching"},
                        {"authorId", "2"}
                    }
                }}
            }}
        }
    });
    // clang-format on
}

static void run(const Schema& schema, const string& label, const string& query) {
    auto result = schema.Resolve({.query = query}).get();
    cout << "=== " << label << " ===" << endl;
    if (result.errors)
        for (const auto& e : *result.errors)
            cerr << "Error: " << e.message << endl;
    else
        cout << result.data.value().dump(2) << endl;
    cout << endl;
}

int main() {
    cout << "=== GQLXY — schema stitching sample ===" << endl << endl;

    auto stitched = makeUsersSchema().Stitch(makePostsSchema());

    run(stitched, "users",  "{ users { id name email } }");
    run(stitched, "posts",  "{ posts { id title authorId } }");
    run(stitched, "combined", R"(
        {
            user(id: "1") { name email }
            post(id: "1") { title }
        }
    )");

    return 0;
}
