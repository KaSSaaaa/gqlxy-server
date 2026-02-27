#include <ariane/resolvers.h>
#include <ariane/schema.h>
#include <ariane/task.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <future>
#include <optional>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class ResolveTest : public testing::Test {
protected:
    static json resolve(const string& typeDefs,
                        Resolver queryResolvers,
                        const string& query) {
        Schema schema({.typeDefs  = typeDefs,
                       .resolvers = {{"Query", std::move(queryResolvers)}}});
        auto result = schema.Resolve(query).get();
        EXPECT_TRUE(result.errors.empty()) << "Unexpected errors: " << result.errors;
        return json::parse(result.data);
    }
};

// ---------------------------------------------------------------------------
// Scalar types
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, ResolvesScalarString) {
    auto data = resolve("type Query { msg: String }",
                        {{"msg", "hello"}},
                        "query { msg }");
    EXPECT_EQ(data["msg"], "hello");
}

TEST_F(ResolveTest, ResolvesScalarInt) {
    auto data = resolve("type Query { count: Int }",
                        {{"count", 42}},
                        "query { count }");
    EXPECT_EQ(data["count"], 42);
}

TEST_F(ResolveTest, ResolvesScalarUInt64) {
    auto data = resolve("type Query { big: Int }",
                        {{"big", uint64_t(9999999999ULL)}},
                        "query { big }");
    EXPECT_EQ(data["big"].get<uint64_t>(), 9999999999ULL);
}

TEST_F(ResolveTest, ResolvesScalarDouble) {
    auto data = resolve("type Query { pi: Float }",
                        {{"pi", 3.14}},
                        "query { pi }");
    EXPECT_DOUBLE_EQ(data["pi"].get<double>(), 3.14);
}

TEST_F(ResolveTest, ResolvesScalarFloat) {
    auto data = resolve("type Query { x: Float }",
                        {{"x", 1.5f}},
                        "query { x }");
    EXPECT_FLOAT_EQ(data["x"].get<float>(), 1.5f);
}

TEST_F(ResolveTest, ResolvesScalarBoolTrue) {
    auto data = resolve("type Query { active: Boolean }",
                        {{"active", true}},
                        "query { active }");
    EXPECT_EQ(data["active"], true);
}

TEST_F(ResolveTest, ResolvesScalarBoolFalse) {
    auto data = resolve("type Query { active: Boolean }",
                        {{"active", false}},
                        "query { active }");
    EXPECT_EQ(data["active"], false);
}

TEST_F(ResolveTest, ResolvesNullValue) {
    auto data = resolve("type Query { nothing: String }",
                        {{"nothing", nullopt}},
                        "query { nothing }");
    EXPECT_TRUE(data["nothing"].is_null());
}

// ---------------------------------------------------------------------------
// String escaping
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, EscapesSpecialCharactersInStrings) {
    const string value = R"(say "hello"\nnewline)";
    auto data = resolve("type Query { msg: String }",
                        {{"msg", value}},
                        "query { msg }");
    EXPECT_EQ(data["msg"].get<string>(), value);
}

// ---------------------------------------------------------------------------
// Nested objects and selection sets
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, ResolvesNestedObject) {
    auto data = resolve(R"(
        type Query { user: User }
        type User { id: Int name: String }
    )",
                        {{"user", Resolver{{"id", 1}, {"name", "Alice"}}}},
                        "query { user { id name } }");
    EXPECT_EQ(data["user"]["id"], 1);
    EXPECT_EQ(data["user"]["name"], "Alice");
}

TEST_F(ResolveTest, SelectionSetOnlyReturnsRequestedFields) {
    auto data = resolve(R"(
        type Query { user: User }
        type User { id: Int name: String email: String }
    )",
                        {{"user", Resolver{{"id", 1}, {"name", "Alice"}, {"email", "a@b.com"}}}},
                        "query { user { id } }");
    EXPECT_TRUE(data["user"].contains("id"));
    EXPECT_FALSE(data["user"].contains("name"));
    EXPECT_FALSE(data["user"].contains("email"));
}

TEST_F(ResolveTest, UnknownFieldOmittedFromResult) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", "world"}},
                        "query { hello nonExistent }");
    EXPECT_TRUE(data.contains("hello"));
    EXPECT_FALSE(data.contains("nonExistent"));
}

TEST_F(ResolveTest, ResolvesTypename) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", "world"}},
                        "query { __typename hello }");
    EXPECT_EQ(data["__typename"], "Query");
    EXPECT_EQ(data["hello"], "world");
}

TEST_F(ResolveTest, ResolvesNestedTypename) {
    auto data = resolve(R"(
        type Query { user: User }
        type User { id: Int }
    )",
                        {{"user", Resolver{{"id", 7}}}},
                        "query { user { __typename id } }");
    EXPECT_EQ(data["user"]["__typename"], "User");
    EXPECT_EQ(data["user"]["id"], 7);
}

// ---------------------------------------------------------------------------
// Lists
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, ResolvesListOfScalars) {
    auto data = resolve("type Query { tags: [String] }",
                        {{"tags", vector<ValueResolver>{"a", "b", "c"}}},
                        "query { tags }");
    ASSERT_TRUE(data["tags"].is_array());
    ASSERT_EQ(data["tags"].size(), 3);
    EXPECT_EQ(data["tags"][0], "a");
    EXPECT_EQ(data["tags"][2], "c");
}

TEST_F(ResolveTest, ResolvesListOfObjects) {
    auto data = resolve(R"(
        type Query { users: [User] }
        type User { id: Int }
    )",
                        {{"users", vector<ValueResolver>{
                                       Resolver{{"id", 1}},
                                       Resolver{{"id", 2}},
                                   }}},
                        "query { users { id } }");
    ASSERT_TRUE(data["users"].is_array());
    ASSERT_EQ(data["users"].size(), 2);
    EXPECT_EQ(data["users"][0]["id"], 1);
    EXPECT_EQ(data["users"][1]["id"], 2);
}

TEST_F(ResolveTest, ResolvesEmptyList) {
    auto data = resolve("type Query { tags: [String] }",
                        {{"tags", vector<ValueResolver>{}}},
                        "query { tags }");
    ASSERT_TRUE(data["tags"].is_array());
    EXPECT_EQ(data["tags"].size(), 0);
}

// ---------------------------------------------------------------------------
// Callable resolver variants
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, ResolvesFunctionResolver) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", []() -> ValueResolver { return "from fn"; }}},
                        "query { hello }");
    EXPECT_EQ(data["hello"], "from fn");
}

TEST_F(ResolveTest, ResolvesAsyncFunctionResolver) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", []() {
                              return async(launch::async,
                                          []() -> ValueResolver { return "async"; });
                          }}},
                        "query { hello }");
    EXPECT_EQ(data["hello"], "async");
}

TEST_F(ResolveTest, ResolvesCoroutineResolver) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", []() -> Task<ValueResolver> { co_return "coroutine"; }}},
                        "query { hello }");
    EXPECT_EQ(data["hello"], "coroutine");
}

TEST_F(ResolveTest, ResolvesCallbackResolver) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", [](const function<void(const ValueResolver&)>& cb) {
                              cb("callback");
                          }}},
                        "query { hello }");
    EXPECT_EQ(data["hello"], "callback");
}

TEST_F(ResolveTest, ResolvesFunctionReturningNestedObject) {
    auto data = resolve(R"(
        type Query { user: User }
        type User { id: Int }
    )",
                        {{"user", []() -> ValueResolver {
                              return Resolver{{"id", 99}};
                          }}},
                        "query { user { id } }");
    EXPECT_EQ(data["user"]["id"], 99);
}

// ---------------------------------------------------------------------------
// Multiple fields at the root
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, ResolvesMultipleRootFields) {
    auto data = resolve("type Query { a: String b: Int }",
                        {{"a", "hello"}, {"b", 42}},
                        "query { a b }");
    EXPECT_EQ(data["a"], "hello");
    EXPECT_EQ(data["b"], 42);
}
