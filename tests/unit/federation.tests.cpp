#include <ariane/schema.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

static const string UserTypeDefs = R"(
    type User @key(fields: "id") {
        id: ID!
        name: String
    }
    type Query {
        me: User
    }
)";

static const string MultiKeyTypeDefs = R"(
    type User @key(fields: "id") @key(fields: "email") {
        id: ID!
        email: String
        name: String
    }
    type Product @key(fields: "sku") {
        sku: ID!
        title: String
        price: Float
    }
    type Query {
        dummy: String
    }
)";

static Schema MakeFederatedSchema(const string& typeDefs, const Resolver& resolvers = {}) {
    return Schema {
        SchemaOptions {
           .typeDefs = typeDefs,
           .resolvers = resolvers,
           .federation = true,
       }
    };
}

static Resolver WithRef(const string& typeName, FunctionResolver fn) {
    return {
        {typeName, Resolver {
            {"__resolveReference", fn}
        }}
    };
}

// --- _service ----------------------------------------------------------------

TEST(Federation, ServiceQueryReturnsSdl) {
    auto schema = MakeFederatedSchema(UserTypeDefs, WithRef("User", [](auto&) -> ValueResolver { return {}; }));
    auto result = schema.Resolve({
        .query = "{ _service { sdl } }"
    }).get();
    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["_service"]["sdl"], UserTypeDefs);
}

// --- _entities ---------------------------------------------------------------

TEST(Federation, EntitiesQueryDispatchesToEntityResolver) {
    auto schema = MakeFederatedSchema(UserTypeDefs, WithRef("User", [](const ResolverArgs& r) -> ValueResolver {
        return Resolver{
            {"id", r.Args()["id"].get<string>()},
            {"name", "Alice"}
        };
    }));
    auto result = schema.Resolve({
        .query = R"(
            query($reps: [_Any!]!) {
                _entities(representations: $reps) {
                    ... on User {
                        id
                        name
                    }
                }
            })",
        .variables = {
            {"reps", {
                {
                    {"__typename", "User"},
                    {"id", "42"}
                }
            }}
        }
    }).get();

    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    ASSERT_EQ(data["_entities"].size(), 1);
    EXPECT_EQ(data["_entities"][0]["id"], "42");
    EXPECT_EQ(data["_entities"][0]["name"], "Alice");
}

TEST(Federation, EntitiesQueryReturnsNullForUnknownType) {
    auto schema = MakeFederatedSchema(UserTypeDefs, WithRef("User", [](const ResolverArgs&) -> ValueResolver {
        return Resolver{{"id", "1"}};
    }));
    auto result = schema.Resolve({
        .query = R"(
            query($reps: [_Any!]!) {
                _entities(representations: $reps) {
                    ... on User {
                        id
                    }
                }
            }
        )",
        .variables = {
            {"reps", {
                {
                    {"__typename", "Ghost"},
                    {"id", "99"}
                }
            }}
        }
    }).get();

    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_TRUE(data["_entities"][0].is_null());
}

TEST(Federation, EntitiesQueryHandlesMultipleRepresentations) {
    auto schema = MakeFederatedSchema(UserTypeDefs, WithRef("User", [](const ResolverArgs& r) -> ValueResolver {
        auto id = r.Args()["id"].get<string>();
        return Resolver{
            {"id", id},
            {"name", "user-" + id}
        };
    }));
    auto result = schema.Resolve({
        .query = R"(
            query($reps: [_Any!]!) {
                _entities(representations: $reps) {
                    ... on User {
                        id
                        name
                    }
                }
            }
        )",
        .variables = {
            {"reps", {
                {
                    {"__typename", "User"},
                    {"id", "1"}
                },
                {
                    {"__typename", "User"},
                    {"id", "2"}
                }
            }}
        }
    }).get();

    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    ASSERT_EQ(data["_entities"].size(), 2);
    EXPECT_EQ(data["_entities"][0]["id"], "1");
    EXPECT_EQ(data["_entities"][1]["id"], "2");
}

// --- @key detection ----------------------------------------------------------

TEST(Federation, EntityUnionContainsAllEntities) {
    Resolver resolvers = WithRef("User",    [](auto&) -> ValueResolver { return {}; });
    resolvers.merge(WithRef("Product", [](auto&) -> ValueResolver { return {}; }));

    auto schema = MakeFederatedSchema(MultiKeyTypeDefs, resolvers);
    auto result = schema.Resolve({
        .query = R"({
            __type(name: "_Entity") {
                possibleTypes {
                    name
                }
            }
        })"
    }).get();

    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["__type"]["possibleTypes"].size(), 2);
}

TEST(Federation, ServiceTypeExistsInIntrospection) {
    auto schema = MakeFederatedSchema(UserTypeDefs, WithRef("User", [](auto&) -> ValueResolver { return {}; }));
    auto result = schema.Resolve({
        .query = R"({
            __type(name: "_Service") {
                name
                fields {
                    name
                }
            }
        })"
    }).get();

    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["__type"]["name"], "_Service");
    EXPECT_EQ(data["__type"]["fields"][0]["name"], "sdl");
}

TEST(Federation, KeyDirectiveAppearsInIntrospection) {
    auto schema = MakeFederatedSchema(UserTypeDefs, WithRef("User", [](auto&) -> ValueResolver { return {}; }));
    auto result = schema.Resolve({
        .query = R"({
            __schema {
                directives {
                    name
                }
            }
        })"
    }).get();

    ASSERT_FALSE(result.errors.has_value());
    auto directives = json::parse(result.data.value())["__schema"]["directives"];
    bool hasKey = false;
    for (const auto& d : directives)
        if (d["name"] == "key") { hasKey = true; break; }
    EXPECT_TRUE(hasKey);
}

// --- Composition hints -------------------------------------------------------

TEST(Federation, ThrowsWhenKeyTypeHasNoEntityResolver) {
    EXPECT_THROW(MakeFederatedSchema(UserTypeDefs, {}), std::runtime_error);
}

TEST(Federation, NoEntitiesFieldWithoutEntities) {
    auto schema = Schema {{
        .typeDefs = R"(
            type Query {
                hello: String
            }
        )",
        .resolvers = {
            {"Query", Resolver {
                {"hello", "world"}
            }}
        },
        .federation = true,
    }};
    ASSERT_FALSE(schema.Resolve({.query = "{ _service { sdl } }"}).get().errors.has_value());
    EXPECT_TRUE(schema.Resolve({.query = "{ _entities(representations: []) { __typename } }"}).get().errors.has_value());
}

// --- Non-federation schemas unaffected ---------------------------------------

TEST(Federation, NonFederatedSchemaHasNoServiceField) {
    auto schema = Schema{{
         .typeDefs = R"(
             type Query {
                 hello: String
             }
         )",
         .resolvers = {
             {"Query", Resolver {
                 {"hello", "world"}
             }}
         },
    }};
    EXPECT_TRUE(schema.Resolve({.query = "{ _service { sdl } }"}).get().errors.has_value());
}
