#include <ariane/resolvers.h>
#include <ariane/schema.h>
#include <ariane/task.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
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
    const string searchSchema = R"(
        type Query {
            search: SearchResult
        }

        union SearchResult = Book | Movie

        type Book {
            title: String
        }

        type Movie {
            director: String
        }
    )";

    static optional<string> searchTypeResolver(const Resolver& r) {
        return r.contains("title") ? "Book" : "Movie";
    }

    const string twoFieldSchema = "type Query { a: String b: String }";
    const Resolver twoFieldResolvers = {{"a", "alpha"}, {"b", "beta"}};

    static json resolve(const string& typeDefs,
                        Resolver queryResolvers,
                        const string& query) {
        Schema schema({
            .typeDefs  = typeDefs,
            .resolvers = {
                {"Query", queryResolvers}
            }
        });
        auto result = schema.Resolve({
            .query = query
        }).get();
        EXPECT_FALSE(result.errors.has_value()) << "Unexpected errors: " << result.errors.value()[0].message;
        return json::parse(result.data.value());
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
    auto data = resolve(R"(
        type Query {
            hello: String
         }
    )",
    {{"hello", "world"}},
    R"(
        query {
            __typename
            hello
        }
    )");
    EXPECT_EQ(data["__typename"], "Query");
    EXPECT_EQ(data["hello"], "world");
}

TEST_F(ResolveTest, ResolvesNestedTypename) {
    auto data = resolve(R"(
        type Query {
            user: User
        }
        type User {
            id: Int
        }
    )",
    {
        {"user", Resolver{
            {"id", 7}
        }}
    },
    R"(
        query {
            user {
                __typename
                id
            }
        }
    )");
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
                        {{"hello", FunctionResolver([](const ResolverArgs&) { return "from fn"; })}},
                        "query { hello }");
    EXPECT_EQ(data["hello"], "from fn");
}

TEST_F(ResolveTest, ResolvesAsyncFunctionResolver) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", AsyncFunctionResolver([](const ResolverArgs&) {
                            return async(launch::async, []() -> ValueResolver {
                                return "async";
                            });
                        })}},
                        "query { hello }");
    EXPECT_EQ(data["hello"], "async");
}

TEST_F(ResolveTest, ResolvesCoroutineResolver) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", CoroutineResolver([](const ResolverArgs&) -> Task<ValueResolver> {
                            co_return "coroutine";
                        })}},
                        "query { hello }");
    EXPECT_EQ(data["hello"], "coroutine");
}

TEST_F(ResolveTest, ResolvesCallbackResolver) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", CallbackResolver([](const ResolverArgs&, const function<void(const ValueResolver&)>& cb) {
                            cb("callback");
                        })}},
                        "query { hello }");
    EXPECT_EQ(data["hello"], "callback");
}

TEST_F(ResolveTest, ResolvesFunctionReturningNestedObject) {
    auto data = resolve(R"(
        type Query { user: User }
        type User { id: Int }
    )",
    {{"user", FunctionResolver([](const ResolverArgs&) {
        return Resolver{{"id", 99}};
    })}}, "query { user { id } }");

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

// ---------------------------------------------------------------------------
// Shorthand / anonymous queries (#1)
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, ResolvesShorthandQuery) {
    auto data = resolve("type Query { hello: String }",
                        {{"hello", "world"}},
                        "{ hello }");
    EXPECT_EQ(data["hello"], "world");
}

// ---------------------------------------------------------------------------
// Field arguments (#2)
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, PassesIntArgToResolver) {
    auto data = resolve("type Query { square(x: Int!): Int }",
                        {{"square", FunctionResolver([](const ResolverArgs& a) {
                            return a.args["x"].get<int>() * a.args["x"].get<int>();
                        })}},
                        "query { square(x: 5) }");
    EXPECT_EQ(data["square"], 25);
}

TEST_F(ResolveTest, PassesStringArgToResolver) {
    auto data = resolve("type Query { greet(name: String!): String }",
                        {{"greet", FunctionResolver([](const ResolverArgs& a) {
                            return "Hello, " + a.args["name"].get<string>();
                        })}},
                        R"(query { greet(name: "Alice") })");
    EXPECT_EQ(data["greet"], "Hello, Alice");
}

TEST_F(ResolveTest, PassesBoolArgToResolver) {
    auto data = resolve("type Query { flag(on: Boolean!): Boolean }",
                        {{"flag", FunctionResolver([](const ResolverArgs& a) {
                            return a.args["on"].get<bool>();
                        })}},
                        "query { flag(on: true) }");
    EXPECT_EQ(data["flag"], true);
}

TEST_F(ResolveTest, PassesMultipleArgsToResolver) {
    auto data = resolve("type Query { add(a: Int!, b: Int!): Int }",
                        {{"add", FunctionResolver([](const ResolverArgs& r) {
                            return r.args["a"].get<int>() + r.args["b"].get<int>();
                        })}},
                        "query { add(a: 3, b: 4) }");
    EXPECT_EQ(data["add"], 7);
}

// ---------------------------------------------------------------------------
// Variable substitution (#3)
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, SubstitutesVariableInArgument) {
    Schema schema({
        .typeDefs  = "type Query { echo(msg: String!): String }",
        .resolvers = {
            {"Query", Resolver{
                {"echo", FunctionResolver([](const ResolverArgs& a) {
                    return a.args["msg"].get<string>();
                })}
            }}
        }
    });

    auto result = schema.Resolve({
        .query = R"(
            query($msg: String!) { echo(msg: $msg) }
        )",
        .variables = {{"msg", "hello variables"}}
    }).get();
    ASSERT_FALSE(result.errors.has_value()) << result.errors.value()[0].message;
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["echo"], "hello variables");
}

TEST_F(ResolveTest, SubstitutesIntVariable) {
    Schema schema({
        .typeDefs  = "type Query { double(n: Int!): Int }",
        .resolvers = {
            {"Query", Resolver{
                {"double", FunctionResolver([](const ResolverArgs& a) {
                    return a.args["n"].get<int>() * 2;
                })}
            }}
        }
    });

    auto result = schema.Resolve({
        .query = R"(query($n: Int!) { double(n: $n) })",
        .variables = {{"n", 6}}
    }).get();
    ASSERT_FALSE(result.errors.has_value()) << result.errors.value()[0].message;
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["double"], 12);
}

// ---------------------------------------------------------------------------
// Field aliases (#9)
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, RespectsFieldAlias) {
    auto data = resolve("type Query { greeting: String }",
                        {{"greeting", "hello"}},
                        "query { myGreeting: greeting }");
    EXPECT_EQ(data["myGreeting"], "hello");
    EXPECT_FALSE(data.contains("greeting"));
}

TEST_F(ResolveTest, MultipleAliasesForSameField) {
    auto data = resolve("type Query { msg: String }",
                        {{"msg", "hi"}},
                        "query { a: msg b: msg }");
    EXPECT_EQ(data["a"], "hi");
    EXPECT_EQ(data["b"], "hi");
    EXPECT_FALSE(data.contains("msg"));
}

// ---------------------------------------------------------------------------
// Per-field error handling (#11)
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, FieldErrorSetsNullAndRecordsError) {
    Schema schema({
        .typeDefs  = "type Query { boom: String }",
        .resolvers = {{"Query", Resolver{
            {"boom", FunctionResolver([](const ResolverArgs&) -> ValueResolver {
                throw runtime_error("resolver exploded");
            })}
        }}}
    });
    auto result = schema.Resolve({.query = "query { boom }"}).get();
    ASSERT_TRUE(result.data.has_value());
    ASSERT_TRUE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["boom"], nullptr);
    const auto& errors = result.errors.value();
    EXPECT_EQ(errors[0].message, "resolver exploded");
    EXPECT_EQ(errors[0].path[0], "boom");
}

TEST_F(ResolveTest, OtherFieldsResolveAfterFieldError) {
    Schema schema({
        .typeDefs  = "type Query { boom: String ok: String }",
        .resolvers = {{"Query", Resolver{
            {"boom", FunctionResolver([](const ResolverArgs&) -> ValueResolver {
                throw runtime_error("boom");
            })},
            {"ok", "fine"}
        }}}
    });
    auto result = schema.Resolve({.query = "query { boom ok }"}).get();
    ASSERT_TRUE(result.data.has_value());
    ASSERT_TRUE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["boom"], nullptr);
    EXPECT_EQ(data["ok"], "fine");
}

TEST_F(ResolveTest, NestedFieldErrorIncludesFullPath) {
    Schema schema({
        .typeDefs  = "type Query { user: User } type User { name: String }",
        .resolvers = {{"Query", Resolver{
            {"user", Resolver{
                {"name", FunctionResolver([](const ResolverArgs&) -> ValueResolver {
                    throw runtime_error("name failed");
                })}
            }}
        }}}
    });
    auto result = schema.Resolve({.query = "query { user { name } }"}).get();
    ASSERT_TRUE(result.errors.has_value());
    const auto& errors = result.errors.value();
    EXPECT_EQ(errors[0].path[0], "user");
    EXPECT_EQ(errors[0].path[1], "name");
}

TEST_F(ResolveTest, NoErrorsKeyWhenAllFieldsSucceed) {
    auto result = Schema({
        .typeDefs  = "type Query { ok: String }",
        .resolvers = {{"Query", Resolver{{"ok", "yes"}}}}
    }).Resolve({.query = "query { ok }"}).get();
    EXPECT_FALSE(result.errors.has_value());
}

// ---------------------------------------------------------------------------
// Abstract type resolution (#12)
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, TypeResolverDeterminesConcreteType) {
    Schema schema({
        .typeDefs  = searchSchema,
        .resolvers = {
            {"Query", Resolver{
                {"search", Resolver{
                    {"title", "The Hobbit"}
                }}
            }},
            {"SearchResult", Resolver {
                {"__resolveType", searchTypeResolver}
            }}
        }
    });
    auto result = schema.Resolve({
        .query = R"({
            search {
                ... on Book {
                    title
                }
                ... on Movie {
                    director
                }
            }
        })"
    }).get();
    EXPECT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["search"]["title"], "The Hobbit");
    EXPECT_FALSE(data["search"].contains("director"));
}

TEST_F(ResolveTest, TypeResolverFiltersMismatchedInlineFragment) {
    Schema schema({
        .typeDefs  = searchSchema,
        .resolvers = {
            {"Query", Resolver{
                {"search", Resolver{
                    {"director", "Spielberg"}}
                }}
            },
            {"SearchResult", Resolver{
                {"__resolveType", searchTypeResolver}
            }}
        }
    });
    auto result = schema.Resolve({
        .query = R"({
            search {
                ... on Book {
                    title
                }
                ... on Movie {
                    director
                }
            }
        })"
    }).get();
    EXPECT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["search"]["director"], "Spielberg");
    EXPECT_FALSE(data["search"].contains("title"));
}

TEST_F(ResolveTest, TypeNameReturnsConcreteTypeFromTypeResolver) {
    Schema schema({
        .typeDefs  = searchSchema,
        .resolvers = {
            {"Query", Resolver{
                {"search", Resolver{
                    {"title", "Dune"}}
                }}
            },
            {"SearchResult", Resolver{
                {"__resolveType", searchTypeResolver}
            }}
        }
    });
    auto data = json::parse(schema.Resolve({
        .query = R"({
            search {
                __typename
            }
        })"
    }).get().data.value());
    EXPECT_EQ(data["search"]["__typename"], "Book");
}

TEST_F(ResolveTest, NamedFragmentFilteredByTypeCondition) {
    Schema schema({
        .typeDefs  = searchSchema,
        .resolvers = {
            {"Query", Resolver{
                {"search", Resolver{
                    {"title", "1984"}
                }}
            }},
            {"SearchResult", Resolver{
                {"__resolveType", searchTypeResolver}
            }}
        }
    });
    auto result = schema.Resolve({
        .query = R"(
            fragment BookInfo on Book {
                title
            }

            fragment MovieInfo on Movie {
                director
            }

            {
                search {
                    ...BookInfo
                    ...MovieInfo
                }
            }
        )"
    }).get();
    EXPECT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["search"]["title"], "1984");
    EXPECT_FALSE(data["search"].contains("director"));
}

// ---------------------------------------------------------------------------
// Directives — @skip / @include (#10)
// ---------------------------------------------------------------------------

TEST_F(ResolveTest, SkipDirectiveWithTrueOmitsField) {
    auto data = resolve(twoFieldSchema, twoFieldResolvers, "{ a @skip(if: true) b }");
    EXPECT_FALSE(data.contains("a"));
    EXPECT_EQ(data["b"], "beta");
}

TEST_F(ResolveTest, SkipDirectiveWithFalseIncludesField) {
    auto data = resolve(twoFieldSchema, twoFieldResolvers, "{ a @skip(if: false) b }");
    EXPECT_EQ(data["a"], "alpha");
    EXPECT_EQ(data["b"], "beta");
}

TEST_F(ResolveTest, IncludeDirectiveWithTrueIncludesField) {
    auto data = resolve(twoFieldSchema, twoFieldResolvers, "{ a @include(if: true) b }");
    EXPECT_EQ(data["a"], "alpha");
    EXPECT_EQ(data["b"], "beta");
}

TEST_F(ResolveTest, IncludeDirectiveWithFalseOmitsField) {
    auto data = resolve(twoFieldSchema, twoFieldResolvers, "{ a @include(if: false) b }");
    EXPECT_FALSE(data.contains("a"));
    EXPECT_EQ(data["b"], "beta");
}

TEST_F(ResolveTest, SkipDirectiveWithVariable) {
    Schema schema({
        .typeDefs  = twoFieldSchema,
        .resolvers = {{"Query", twoFieldResolvers}}
    });
    auto result = schema.Resolve({
        .query = "query($s: Boolean!) { a @skip(if: $s) b }",
        .variables = {{"s", true}}
    }).get();
    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_FALSE(data.contains("a"));
    EXPECT_EQ(data["b"], "beta");
}

TEST_F(ResolveTest, IncludeDirectiveWithVariable) {
    Schema schema({
        .typeDefs  = twoFieldSchema,
        .resolvers = {{"Query", twoFieldResolvers}}
    });
    auto result = schema.Resolve({
        .query = "query($show: Boolean!) { a @include(if: $show) b }",
        .variables = {{"show", false}}
    }).get();
    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_FALSE(data.contains("a"));
    EXPECT_EQ(data["b"], "beta");
}

TEST_F(ResolveTest, SkipDirectiveOnInlineFragment) {
    auto data = resolve(
        "type Query { user: User } type User { id: Int name: String }",
        {{"user", Resolver{{"id", 1}, {"name", "Alice"}}}},
        R"({ user { id ... on User @skip(if: true) { name } } })");
    EXPECT_TRUE(data["user"].contains("id"));
    EXPECT_FALSE(data["user"].contains("name"));
}

TEST_F(ResolveTest, IncludeDirectiveOnInlineFragment) {
    auto data = resolve(
        "type Query { user: User } type User { id: Int name: String }",
        {{"user", Resolver{{"id", 1}, {"name", "Alice"}}}},
        R"({ user { id ... on User @include(if: false) { name } } })");
    EXPECT_TRUE(data["user"].contains("id"));
    EXPECT_FALSE(data["user"].contains("name"));
}

TEST_F(ResolveTest, SkipDirectiveOnNamedFragment) {
    Schema schema({
        .typeDefs  = "type Query { user: User } type User { id: Int name: String }",
        .resolvers = {{"Query", Resolver{
            {"user", Resolver{{"id", 2}, {"name", "Bob"}}}
        }}}
    });
    auto result = schema.Resolve({
        .query = R"(
            fragment NameInfo on User { name }
            { user { id ...NameInfo @skip(if: true) } }
        )"
    }).get();
    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["user"]["id"], 2);
    EXPECT_FALSE(data["user"].contains("name"));
}

TEST_F(ResolveTest, CustomDirectiveIsEvaluated) {
    Schema schema({
        .typeDefs  = twoFieldSchema,
        .resolvers = {{"Query", twoFieldResolvers}},
        .directives = {
            {"secret", [](const ResolverArgs& args, const ValueResolver&) -> optional<ValueResolver> {
                return args.args.value("redact", false) ? nullopt : optional<ValueResolver>(monostate{});
            }}
        }
    });
    auto result = schema.Resolve({.query = "{ a @secret(redact: true) b }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_FALSE(data.contains("a"));
    EXPECT_EQ(data["b"], "beta");
}

TEST_F(ResolveTest, CustomDirectiveCanBeOverridden) {
    Schema schema({
        .typeDefs  = twoFieldSchema,
        .resolvers = {{"Query", twoFieldResolvers}},
        .directives = {
            {"skip", [](const ResolverArgs&, const ValueResolver& v) -> optional<ValueResolver> {
                return optional<ValueResolver>(v);  // never skip
            }}
        }
    });
    auto result = schema.Resolve({.query = "{ a @skip(if: true) b }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["a"], "alpha");
}

TEST_F(ResolveTest, DirectiveCanTransformFieldValue) {
    Schema schema({
        .typeDefs  = "type Query { greeting: String }",
        .resolvers = {{"Query", Resolver{{"greeting", "hello"}}}},
        .directives = {
            {"uppercase", [](const ResolverArgs&, const ValueResolver& v) -> optional<ValueResolver> {
                auto str = get<string>(v);
                transform(str.begin(), str.end(), str.begin(), ::toupper);
                return optional<ValueResolver>(str);
            }}
        }
    });
    auto result = schema.Resolve({.query = "{ greeting @uppercase }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["greeting"], "HELLO");
}

TEST_F(ResolveTest, DirectiveTransformCanAccessArgs) {
    Schema schema({
        .typeDefs  = "type Query { value: String }",
        .resolvers = {{"Query", Resolver{{"value", "hello world"}}}},
        .directives = {
            {"prefix", [](const ResolverArgs& args, const ValueResolver& v) -> optional<ValueResolver> {
                return optional<ValueResolver>(args.args.value("with", string{}) + get<string>(v));
            }}
        }
    });
    auto result = schema.Resolve({.query = R"({ value @prefix(with: ">>> ") })"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["value"], ">>> hello world");
}
