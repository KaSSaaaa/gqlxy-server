#include <ariane/schema.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static Schema usersSchema() {
    return Schema({
        .typeDefs = R"(
            type Query { user(id: ID!): User }
            type User { id: ID name: String }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"user", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                    return Resolver{{"id", r.Args()["id"].get<string>()}, {"name", string{"Alice"}}};
                }}}
            }},
            {"User", Resolver{{"id", string{}}, {"name", string{}}}}
        }
    });
}

static Schema postsSchema() {
    return Schema({
        .typeDefs = R"(
            type Query { post(id: ID!): Post }
            type Post { id: ID title: String }
        )",
        .resolvers = {
            {"Query", Resolver{
                {"post", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                    return Resolver{{"id", r.Args()["id"].get<string>()}, {"title", string{"Hello world"}}};
                }}}
            }},
            {"Post", Resolver{{"id", string{}}, {"title", string{}}}}
        }
    });
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(StitchTest, CombinesQueryFields) {
    auto stitched = usersSchema().Stitch(postsSchema());

    auto r1 = stitched.Resolve({.query = "{ user(id: \"1\") { name } }"}).get();
    auto r2 = stitched.Resolve({.query = "{ post(id: \"42\") { title } }"}).get();

    ASSERT_FALSE(r1.errors.has_value());
    ASSERT_FALSE(r2.errors.has_value());
    EXPECT_EQ(json::parse(r1.data.value())["user"]["name"], "Alice");
    EXPECT_EQ(json::parse(r2.data.value())["post"]["title"], "Hello world");
}

TEST(StitchTest, CombinesTypes) {
    auto stitched = usersSchema().Stitch(postsSchema());

    auto result = stitched.Resolve({.query = R"(
        {
            __type(name: "User")  { name }
            __type(name: "Post")  { name }
        }
    )"}).get();

    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["__type"]["name"], "Post");
}

TEST(StitchTest, IntrospectionReflectsMergedTypes) {
    auto stitched = usersSchema().Stitch(postsSchema());

    auto result = stitched.Resolve({.query = "{ __schema { types { name } } }"}).get();

    ASSERT_FALSE(result.errors.has_value());
    auto types = json::parse(result.data.value())["__schema"]["types"];
    auto hasType = [&](const string& name) {
        return ranges::any_of(types, [&](const auto& t) { return t["name"] == name; });
    };
    EXPECT_TRUE(hasType("User"));
    EXPECT_TRUE(hasType("Post"));
    EXPECT_TRUE(hasType("Query"));
}

TEST(StitchTest, IsChainable) {
    Schema tagsSchema({
        .typeDefs = R"(
            type Query { tag: String }
        )",
        .resolvers = {{"Query", Resolver{{"tag", string{"c++"}}}}},
    });

    auto stitched = usersSchema().Stitch(postsSchema()).Stitch(tagsSchema);

    auto r = stitched.Resolve({.query = "{ user(id:\"1\"){name} post(id:\"1\"){title} tag }"}).get();

    ASSERT_FALSE(r.errors.has_value());
    auto data = json::parse(r.data.value());
    EXPECT_EQ(data["user"]["name"], "Alice");
    EXPECT_EQ(data["post"]["title"], "Hello world");
    EXPECT_EQ(data["tag"], "c++");
}

TEST(StitchTest, ErrorOnDuplicateQueryField) {
    Schema other({
        .typeDefs = R"(type Query { user(id: ID!): String })",
        .resolvers = {{"Query", Resolver{{"user", string{"x"}}}}},
    });
    EXPECT_THROW(usersSchema().Stitch(other), runtime_error);
}

TEST(StitchTest, ErrorOnDuplicateType) {
    Schema other({
        .typeDefs = R"(
            type Query { other: String }
            type User { id: ID }
        )",
        .resolvers = {{"Query", Resolver{{"other", string{"x"}}}}},
    });
    EXPECT_THROW(usersSchema().Stitch(other), runtime_error);
}
