#include <gqlxy/server/resolver_args.h>
#include <gqlxy/server/schema.h>
#include <iostream>
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy;
using json = nlohmann::json;

// A minimal federated subgraph exposing User and Product entities.
// It answers the two federation protocol queries:
//   { _service { sdl } }
//   { _entities(representations: [...]) { ... } }

static const string TypeDefs = R"(
    type User @key(fields: "id") {
        id: ID!
        name: String!
        email: String!
    }

    type Product @key(fields: "sku") {
        sku: ID!
        title: String!
        price: Float!
    }

    type Query {
        user(id: ID!): User
        product(sku: ID!): Product
    }
)";

static const unordered_map<string, Resolver> Users = {
    {"1", Resolver{
        {"id", "1"},
        {"name", "Alice"},
        {"email", "alice@example.com"}
    }},
    {"2", Resolver{
        {"id", "2"},
        {"name", "Bob"},
        {"email", "bob@example.com"}
    }},
};

static const unordered_map<string, Resolver> Products = {
    {"widget-a", Resolver{
        {"sku", "widget-a"},
        {"title", "Widget A"},
        {"price", 9.99}
    }},
    {"gadget-b", Resolver{
        {"sku", "gadget-b"},
        {"title", "Gadget B"},
        {"price", 49.99}
    }},
};

static void run(const Schema& schema, const string& label, const string& query,
                const json& variables = {}) {
    auto result = schema.Resolve({
        .query = query,
        .variables = variables
    }).get();
    cout << "=== " << label << " ===" << endl;
    if (result.errors)
        for (const auto& e : *result.errors)
            cerr << "Error: " << e.message << endl;
    else
        cout << result.data.value().dump(2) << endl;
    cout << endl;
}

int main() {
    cout << "=== GQLXY — Apollo Federation subgraph sample ===" << endl << endl;

    // clang-format off
    auto schema = Schema{SchemaOptions{
        .typeDefs   = TypeDefs,
        .resolvers  = {
            {"Query", Resolver{
                {"user", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                    auto id = r.Args()["id"].get<string>();
                    auto it = Users.find(id);
                    return it != Users.end() ? ValueResolver(it->second) : ValueResolver(monostate{});
                }}},
                {"product", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                    auto sku = r.Args()["sku"].get<string>();
                    auto it = Products.find(sku);
                    return it != Products.end() ? ValueResolver(it->second) : ValueResolver(monostate{});
                }}}
            }},
            {"User", Resolver{
                {"__resolveReference", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                    auto id = r.Args()["id"].get<string>();
                    auto it = Users.find(id);
                    return it != Users.end() ? ValueResolver(it->second) : ValueResolver(monostate{});
                }}}
            }},
            {"Product", Resolver{
                {"__resolveReference", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                    auto sku = r.Args()["sku"].get<string>();
                    auto it = Products.find(sku);
                    return it != Products.end() ? ValueResolver(it->second) : ValueResolver(monostate{});
                }}}
            }}
        },
        .federation = true,
    }};
    // clang-format on

    // 1. Normal field queries
    run(schema, "user(id: \"1\")", R"({ user(id: "1") { id name email } })");
    run(schema, "product(sku: ...)", R"({ product(sku: "gadget-b") { sku title price } })");

    // 2. Federation _service query (returns the SDL for the gateway to consume)
    run(schema, "_service", R"({ _service { sdl } })");

    // 3. Federation _entities query (used by the gateway to resolve entity references)
    run(schema, "_entities — User + Product", R"(
            query ($reps: [_Any!]!) {
                _entities(representations: $reps) {
                    ... on User {
                        id
                        name
                    }
                    ... on Product {
                        sku
                        title
                    }
                }
            }
        )",
        json {
            {"reps", json::array({
                {
                    {"__typename", "User"},
                    {"id", "2"}
                },
                {
                    {"__typename", "Product"},
                    {"sku", "widget-a"}
                }
            })
        }}
    );

    return 0;
}
