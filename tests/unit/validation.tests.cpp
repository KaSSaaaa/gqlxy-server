#include <ariane/ResolverArgs.h>
#include <ariane/resolvers.h>
#include <ariane/schema.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace ariane::graphql;
using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ResolveResult resolve(const string& typeDefs,
                             const Resolver& resolvers,
                             const string& query,
                             const json& variables = json::object()) {
    return Schema({.typeDefs = typeDefs, .resolvers = {{"Query", resolvers}}})
         .Resolve({.query = query, .variables = variables})
         .get();
}

static bool hasError(const ResolveResult& result, const string& substring) {
    if (!result.errors)
        return false;
    for (const auto& e : *result.errors)
        if (e.message.find(substring) != string::npos)
            return true;
    return false;
}

static bool noErrors(const ResolveResult& result) {
    return !result.errors.has_value() || result.errors->empty();
}

// ---------------------------------------------------------------------------
// Schemas reused across tests
// ---------------------------------------------------------------------------

static const string echoSchema = R"(
    type Query { echo(msg: String!): String }
)";

static const Resolver echoResolvers = {
     {"echo", [](const ResolverArgs& a) -> ValueResolver { return a.Args()["msg"].get<string>(); }}};

static const string userSchema = R"(
    type User { name: String }
    type Query { user(id: ID!): User }
)";

static const Resolver userResolvers = {
     {"user", [](const ResolverArgs&) -> ValueResolver { return Resolver{{"name", "Alice"}}; }}};

static const string twoFieldSchema = R"(
    type Query { a: String b: String }
)";

static const Resolver twoFieldResolvers = {
     {"a", [](const ResolverArgs&) -> ValueResolver { return "A"; }},
     {"b", [](const ResolverArgs&) -> ValueResolver { return "B"; }},
};

// ---------------------------------------------------------------------------
// #16 — Variable declaration validation
// ---------------------------------------------------------------------------

TEST(ValidationTest, UndeclaredVariableReturnsError) {
    auto result = resolve(echoSchema, echoResolvers, "{ echo(msg: $msg) }");
    EXPECT_TRUE(hasError(result, "$msg"));
    EXPECT_TRUE(hasError(result, "not declared"));
}

TEST(ValidationTest, DeclaredVariablePasses) {
    auto result = resolve(echoSchema, echoResolvers, "query($msg: String!) { echo(msg: $msg) }", {{"msg", "hi"}});
    EXPECT_TRUE(noErrors(result));
    EXPECT_EQ(result.data.value()["echo"], "hi");
}

TEST(ValidationTest, UndeclaredVariableInDirectiveReturnsError) {
    auto result = resolve(twoFieldSchema, twoFieldResolvers, "{ a @skip(if: $flag) b }");
    EXPECT_TRUE(hasError(result, "$flag"));
    EXPECT_TRUE(hasError(result, "not declared"));
}

TEST(ValidationTest, DeclaredDirectiveVariablePasses) {
    auto result = resolve(twoFieldSchema, twoFieldResolvers, "query($flag: Boolean!) { a @skip(if: $flag) b }",
                          {{"flag", false}});
    EXPECT_TRUE(noErrors(result));
}

// ---------------------------------------------------------------------------
// #17 — Variable nullability validation
// ---------------------------------------------------------------------------

TEST(ValidationTest, RequiredVariableNotProvidedReturnsError) {
    auto result = resolve(userSchema, userResolvers, "query($id: ID!) { user(id: $id) { name } }");
    EXPECT_TRUE(hasError(result, "$id"));
    EXPECT_TRUE(hasError(result, "was not provided"));
}

TEST(ValidationTest, RequiredVariableProvidedPasses) {
    auto result = resolve(userSchema, userResolvers, "query($id: ID!) { user(id: $id) { name } }", {{"id", "1"}});
    EXPECT_TRUE(noErrors(result));
}

TEST(ValidationTest, NullableVariableNotProvidedPasses) {
    const string schema = R"(
        type User { name: String }
        type Query { user(id: ID): User }
    )";
    auto result = resolve(schema, userResolvers, "query($id: ID) { user(id: $id) { name } }");
    EXPECT_TRUE(noErrors(result));
}

TEST(ValidationTest, RequiredVariableWithDefaultNotProvidedPasses) {
    const string schema = R"(
        type User { name: String }
        type Query { user(id: ID!): User }
    )";
    auto result = resolve(schema, userResolvers, "query($id: ID! = \"default\") { user(id: $id) { name } }");
    EXPECT_TRUE(noErrors(result));
}

// ---------------------------------------------------------------------------
// #18 — Unknown field / unknown argument
// ---------------------------------------------------------------------------

TEST(ValidationTest, UnknownFieldReturnsError) {
    auto result = resolve(twoFieldSchema, twoFieldResolvers, "{ nonExistent }");
    EXPECT_TRUE(hasError(result, "nonExistent"));
    EXPECT_TRUE(hasError(result, "Cannot query field"));
}

TEST(ValidationTest, KnownFieldPasses) {
    auto result = resolve(twoFieldSchema, twoFieldResolvers, "{ a }");
    EXPECT_TRUE(noErrors(result));
}

TEST(ValidationTest, UnknownNestedFieldReturnsError) {
    auto result =
         resolve(userSchema, userResolvers, "query($id: ID!) { user(id: $id) { nonExistent } }", {{"id", "1"}});
    EXPECT_TRUE(hasError(result, "nonExistent"));
    EXPECT_TRUE(hasError(result, "Cannot query field"));
}

TEST(ValidationTest, UnknownArgumentReturnsError) {
    auto result = resolve(echoSchema, echoResolvers, "{ echo(msg: \"hi\", unknownArg: \"x\") }");
    EXPECT_TRUE(hasError(result, "unknownArg"));
    EXPECT_TRUE(hasError(result, "Unknown argument"));
}

TEST(ValidationTest, KnownArgumentPasses) {
    auto result = resolve(echoSchema, echoResolvers, "{ echo(msg: \"hello\") }");
    EXPECT_TRUE(noErrors(result));
    EXPECT_EQ(result.data.value()["echo"], "hello");
}

// ---------------------------------------------------------------------------
// #19 — Required argument not provided
// ---------------------------------------------------------------------------

TEST(ValidationTest, RequiredArgNotProvidedReturnsError) {
    auto result = resolve(userSchema, userResolvers, "{ user { name } }");
    EXPECT_TRUE(hasError(result, "id"));
    EXPECT_TRUE(hasError(result, "is required"));
}

TEST(ValidationTest, RequiredArgProvidedPasses) {
    auto result = resolve(userSchema, userResolvers, "{ user(id: \"1\") { name } }");
    EXPECT_TRUE(noErrors(result));
}

TEST(ValidationTest, OptionalArgNotProvidedPasses) {
    const string schema = R"(
        type User { name: String }
        type Query { user(id: ID): User }
    )";
    auto result = resolve(schema, userResolvers, "{ user { name } }");
    EXPECT_TRUE(noErrors(result));
}

TEST(ValidationTest, RequiredArgViaVariableProvidedPasses) {
    auto result = resolve(userSchema, userResolvers, "query($id: ID!) { user(id: $id) { name } }", {{"id", "42"}});
    EXPECT_TRUE(noErrors(result));
}

TEST(ValidationTest, RequiredArgViaVariableNotProvidedReturnsError) {
    auto result = resolve(userSchema, userResolvers, "query($id: ID!) { user(id: $id) { name } }");
    // #17 catches the missing $id first; the result is still an error
    EXPECT_TRUE(result.errors.has_value() && !result.errors->empty());
}

// ---------------------------------------------------------------------------
// Meta-fields are exempt from validation
// ---------------------------------------------------------------------------

TEST(ValidationTest, TypenameMetaFieldIsExempt) {
    auto result = resolve(twoFieldSchema, twoFieldResolvers, "{ a __typename }");
    EXPECT_TRUE(noErrors(result));
}

// ---------------------------------------------------------------------------
// Fragment spreads and inline fragments
// ---------------------------------------------------------------------------

TEST(ValidationTest, InlineFragmentFieldIsValidated) {
    const string schema = R"(
        type Query { search: SearchResult }
        union SearchResult = Book | Movie
        type Book { title: String }
        type Movie { director: String }
    )";
    Resolver resolvers = {{"search", [](const ResolverArgs&) -> ValueResolver {
                               return Resolver{{"__resolveType", "Book"}, {"title", "GraphQL in Action"}};
                           }}};

    auto valid = resolve(schema, resolvers, "{ search { ... on Book { title } } }");
    EXPECT_TRUE(noErrors(valid));

    auto invalid = resolve(schema, resolvers, "{ search { ... on Book { nonExistent } } }");
    EXPECT_TRUE(hasError(invalid, "nonExistent"));
}

TEST(ValidationTest, FragmentSpreadFieldIsValidated) {
    const string schema = R"(
        type Query { search: SearchResult }
        union SearchResult = Book | Movie
        type Book { title: String }
        type Movie { director: String }
    )";
    Resolver resolvers = {{"search", [](const ResolverArgs&) -> ValueResolver {
                               return Resolver{{"__resolveType", "Book"}, {"title", "GraphQL in Action"}};
                           }}};

    const string validQuery = R"(
        { search { ...BookInfo } }
        fragment BookInfo on Book { title }
    )";
    EXPECT_TRUE(noErrors(resolve(schema, resolvers, validQuery)));

    const string invalidQuery = R"(
        { search { ...BookInfo } }
        fragment BookInfo on Book { nonExistent }
    )";
    EXPECT_TRUE(hasError(resolve(schema, resolvers, invalidQuery), "nonExistent"));
}
