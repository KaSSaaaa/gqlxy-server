#include <ariane/internal/utils/ranges.h>
#include <ariane/resolvers.h>
#include <ariane/schema.h>
#include <ariane/task.h>
#include <gtest/gtest.h>
#include <future>
#include <nlohmann/json.hpp>
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
                            return a.Args()["x"].get<int>() * a.Args()["x"].get<int>();
                        })}},
                        "query { square(x: 5) }");
    EXPECT_EQ(data["square"], 25);
}

TEST_F(ResolveTest, PassesStringArgToResolver) {
    auto data = resolve("type Query { greet(name: String!): String }",
                        {{"greet", FunctionResolver([](const ResolverArgs& a) {
                            return "Hello, " + a.Args()["name"].get<string>();
                        })}},
                        R"(query { greet(name: "Alice") })");
    EXPECT_EQ(data["greet"], "Hello, Alice");
}

TEST_F(ResolveTest, PassesBoolArgToResolver) {
    auto data = resolve("type Query { flag(on: Boolean!): Boolean }",
                        {{"flag", FunctionResolver([](const ResolverArgs& a) {
                            return a.Args()["on"].get<bool>();
                        })}},
                        "query { flag(on: true) }");
    EXPECT_EQ(data["flag"], true);
}

TEST_F(ResolveTest, PassesMultipleArgsToResolver) {
    auto data = resolve("type Query { add(a: Int!, b: Int!): Int }",
                        {{"add", FunctionResolver([](const ResolverArgs& r) {
                            return r.Args()["a"].get<int>() + r.Args()["b"].get<int>();
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
                    return a.Args()["msg"].get<string>();
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
                    return a.Args()["n"].get<int>() * 2;
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
                return args.Args().value("redact", false) ? nullopt : optional<ValueResolver>(monostate{});
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
                return internal::to_string(get<string>(v) | views::transform(::toupper));
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
                return optional<ValueResolver>(args.Args().value("with", string{}) + get<string>(v));
            }}
        }
    });
    auto result = schema.Resolve({.query = R"({ value @prefix(with: ">>> ") })"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["value"], ">>> hello world");
}

// ---------------------------------------------------------------------------
// Custom scalars
// ---------------------------------------------------------------------------

class JsonScalar : public ScalarType {
public:
    JsonScalar(const json& v) : ScalarType([=]() { return v.dump();}) {}

    static json Parse(const json& j) {
        return json::parse(j.get<string>());
    }
};

TEST_F(ResolveTest, CustomScalarTransformsValue) {
    Schema schema({
        .typeDefs  = "scalar JSON type Query { data: JSON }",
        .resolvers = {
            {"Query", Resolver{
                {"data", json({{"key", "value"}}).dump()}
            }}
        },
        .scalars   = {
            {"JSON", JsonScalar::Parse}
        }
    });
    auto result = schema.Resolve({.query = "{ data }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["data"], json({{"key", "value"}}).dump());
}

class UnixTime : public ScalarType {
public:
    UnixTime(int time) : ScalarType([=]() { return json {
        {"unix", time}
    }; }) {}

    static json Parse(const json& v) {
        return {{"unix", v.get<int>()}};
    }
};

TEST_F(ResolveTest, CustomScalarOnNestedField) {
    Schema schema({
        .typeDefs  = "scalar Timestamp type User { createdAt: Timestamp } type Query { user: User }",
        .resolvers = {
            {"Query", Resolver{
                {"user", Resolver{
                    {"createdAt", UnixTime(1704067200)}
                }}
            }}
        },
        .scalars = {
            {"Timestamp", UnixTime::Parse}
        }
    });
    auto result = schema.Resolve({.query = "{ user { createdAt } }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["user"]["createdAt"]["unix"], 1704067200);
}

TEST_F(ResolveTest, UnregisteredCustomScalarPassesThroughAsIs) {
    Schema schema({
        .typeDefs  = "scalar DateTime type Query { ts: DateTime }",
        .resolvers = {{"Query", Resolver{{"ts", "2024-01-01"}}}}
    });
    auto result = schema.Resolve({.query = "{ ts }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["ts"], "2024-01-01");
}

class DateScalar : public ScalarType {
public:
    DateScalar(const string& v) : ScalarType([=]() -> json { return {{"iso", v}}; }) {}

    static json Parse(const json& v) {
        return {{"iso", v.get<string>()}};
    }
};

TEST_F(ResolveTest, CustomScalarParsesVariableInput) {
    Schema schema({
        .typeDefs  = R"(
            scalar Date

            type Query {
                event(on: Date!): String
            }
        )",
        .resolvers = {{"Query", Resolver{{"event", FunctionResolver([](const ResolverArgs& r) -> ValueResolver {
            return r.Args()["on"]["iso"].get<string>();
        })}}}},
        .scalars = {
            {"Date", DateScalar::Parse}
        }
    });
    auto result = schema.Resolve({
        .query = "query($d: Date!) { event(on: $d) }",
        .variables = {{"d", "2024-06-01"}}
    }).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["event"], "2024-06-01");
}

TEST_F(ResolveTest, CustomScalarParsesLiteralInput) {
    Schema schema({
        .typeDefs  = R"(scalar Date type Query { event(on: Date!): String })",
        .resolvers = {
            {"Query", Resolver{
                {"event", FunctionResolver([](const ResolverArgs& r) -> ValueResolver {
                    return r.Args()["on"]["iso"].get<string>();
                })}
            }}
        },
        .scalars = {
            {"Date", DateScalar::Parse}
        }
    });
    auto result = schema.Resolve({.query = R"({ event(on: "2024-06-01") })"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["event"], "2024-06-01");
}

TEST_F(ResolveTest, CustomScalarReturnedDirectlyFromResolver) {
    Schema schema({
        .typeDefs  = "scalar JSON type Query { data: JSON }",
        .resolvers = {{"Query", Resolver{
            {"data", FunctionResolver([](const ResolverArgs&) -> ValueResolver {
                return JsonScalar(json {
                    {"key", "value"}
                });
            })}
        }}}
    });
    auto result = schema.Resolve({.query = "{ data }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["data"], json({{"key", "value"}}).dump());
}

TEST_F(ResolveTest, CustomScalarReturnedDirectlyWithTransform) {
    Schema schema({
        .typeDefs  = "scalar Timestamp type Query { ts: Timestamp }",
        .resolvers = {{"Query", Resolver{
            {"ts", FunctionResolver([](const ResolverArgs&) -> ValueResolver {
                return UnixTime(1704067200);
            })}
        }}}
    });
    auto result = schema.Resolve({.query = "{ ts }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["ts"]["unix"], 1704067200);
}

// ---------------------------------------------------------------------------
// Resolver context (#13)
// ---------------------------------------------------------------------------

struct RequestContext {
    string userId;
};

TEST_F(ResolveTest, ContextIsThreadedToResolver) {
    Schema schema({
        .typeDefs  = "type Query { me: String }",
        .resolvers = {{"Query", Resolver{
            {"me", FunctionResolver([](const ResolverArgs& r) -> ValueResolver {
                return r.Context<shared_ptr<RequestContext>>()->userId;
            })}
        }}}
    });
    auto ctx = make_shared<RequestContext>(RequestContext{"user-42"});
    auto result = schema.Resolve<shared_ptr<RequestContext>>({.query = "{ me }", .context = ctx}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["me"], "user-42");
}

TEST_F(ResolveTest, ContextIsThreadedToNestedResolver) {
    Schema schema({
        .typeDefs  = "type User { name: String } type Query { user: User }",
        .resolvers = {
            {"Query", Resolver{
                {"user", Resolver{
                    {"name", FunctionResolver([](const ResolverArgs& r) -> ValueResolver {
                        return "Hello " + r.Context<shared_ptr<RequestContext>>()->userId;
                    })}
                }}
            }}
        }
    });
    auto ctx = make_shared<RequestContext>(RequestContext{"alice"});
    auto result = schema.Resolve<shared_ptr<RequestContext>>({.query = "{ user { name } }", .context = ctx}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["user"]["name"], "Hello alice");
}

TEST_F(ResolveTest, EmptyContextDoesNotCrash) {
    Schema schema({
        .typeDefs  = "type Query { ping: String }",
        .resolvers = {{"Query", Resolver{{"ping", "pong"}}}}
    });
    auto result = schema.Resolve({.query = "{ ping }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["ping"], "pong");
}

// ---------------------------------------------------------------------------
// Operation name selection (#15)
// ---------------------------------------------------------------------------

class OperationNameTest : public testing::Test {
protected:
    const string typeDefs = "type Query { a: String b: String }";
    const Resolver queryResolvers = {{"a", "alpha"}, {"b", "beta"}};
};

TEST_F(OperationNameTest, SelectsNamedOperation) {
    Schema schema({.typeDefs = typeDefs, .resolvers = {{"Query", queryResolvers}}});
    auto result = schema.Resolve({
        .query = "query GetA { a } query GetB { b }",
        .operationName = "GetA"
    }).get();
    ASSERT_FALSE(result.errors.has_value());
    auto data = json::parse(result.data.value());
    EXPECT_EQ(data["a"], "alpha");
    EXPECT_FALSE(data.contains("b"));
}

TEST_F(OperationNameTest, ErrorOnUnknownOperationName) {
    Schema schema({.typeDefs = typeDefs, .resolvers = {{"Query", queryResolvers}}});
    auto result = schema.Resolve({
        .query = "query GetA { a }",
        .operationName = "NonExistent"
    }).get();
    ASSERT_TRUE(result.errors.has_value());
    EXPECT_NE(result.errors.value()[0].message.find("NonExistent"), string::npos);
}

TEST_F(OperationNameTest, ErrorOnMultipleOperationsWithoutName) {
    Schema schema({.typeDefs = typeDefs, .resolvers = {{"Query", queryResolvers}}});
    auto result = schema.Resolve({
        .query = "query GetA { a } query GetB { b }"
    }).get();
    ASSERT_TRUE(result.errors.has_value());
    EXPECT_NE(result.errors.value()[0].message.find("operationName"), string::npos);
}

TEST_F(OperationNameTest, SingleOperationWithoutNameStillWorks) {
    Schema schema({.typeDefs = typeDefs, .resolvers = {{"Query", queryResolvers}}});
    auto result = schema.Resolve({.query = "query GetA { a }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(json::parse(result.data.value())["a"], "alpha");
}

// ---------------------------------------------------------------------------
// Serial mutation execution (#14)
// ---------------------------------------------------------------------------

TEST(MutationTest, MutationFieldsExecuteInDocumentOrder) {
    vector<string> order;
    Schema schema({
        .typeDefs  = "type Mutation { first: String second: String third: String } type Query { noop: String }",
        .resolvers = {
            {"Mutation", Resolver{
                {"first",  FunctionResolver([&](const ResolverArgs&) -> ValueResolver { order.push_back("first");  return "1"; })},
                {"second", FunctionResolver([&](const ResolverArgs&) -> ValueResolver { order.push_back("second"); return "2"; })},
                {"third",  FunctionResolver([&](const ResolverArgs&) -> ValueResolver { order.push_back("third");  return "3"; })},
            }}
        }
    });
    auto result = schema.Resolve({.query = "mutation { first second third }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    ASSERT_EQ(order, (vector<string>{"first", "second", "third"}));
}

