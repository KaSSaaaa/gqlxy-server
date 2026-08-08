#include <gqlxy/server/reflexion.h>
#include <gqlxy/server/resolver_args.h>
#include <gqlxy/server/schema_builder.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <vector>

using namespace std;
using namespace gqlxy;

namespace {

struct Node {
    static constexpr const char* GQLName = "Node";
    string id;
};

struct Book {
    static constexpr const char* GQLName = "Book";
    string id;
    string title;
    string author;
    optional<double> rating;
};

struct Movie {
    static constexpr const char* GQLName = "Movie";
    string id;
    string title;
    int durationMinutes;
};

}

template<>
struct gqlxy::GQLImplements<Book> {
    using Interfaces = tuple<Node>;
};
template<>
struct gqlxy::GQLImplements<Movie> {
    using Interfaces = tuple<Node>;
};

class CodeFirstEndToEndTest : public testing::Test {
protected:
    vector<Book> books = {
        {.id = "b1", .title = "Clean Code", .author = "Robert C. Martin", .rating = 4.5},
    };
    vector<Movie> movies = {
        {.id = "m1", .title = "The Imitation Game", .durationMinutes = 114},
    };

    void SetUp() override {
        // clang-format off
        SchemaBuilder builder;
        builder.AddTypeDefs(R"(
                type Query {
                    books: [Book!]!
                    node(id: ID!): Node
                    search(term: String!): SearchResult
                }
            )")
            .AddType<Book>()
            .AddType<Movie>()
            .AddUnion<Book, Movie>("SearchResult")
            .AddQuery("books", FunctionResolver{[this](const ResolverArgs&) -> ValueResolver {
                vector<ValueResolver> result;
                for (const auto& b : books) result.push_back(ToResolver(b));
                return result;
            }})
            .AddQuery("node", FunctionResolver{[this](const ResolverArgs& args) -> ValueResolver {
                auto id = args.Args()["id"].get<string>();
                for (const auto& b : books) if (b.id == id) return ToResolver(b);
                for (const auto& m : movies) if (m.id == id) return ToResolver(m);
                return monostate{};
            }})
            .AddQuery("search", FunctionResolver{[this](const ResolverArgs& args) -> ValueResolver {
                auto term = args.Args()["term"].get<string>();
                for (const auto& b : books) if (b.title.find(term) != string::npos) return ToResolver(b);
                for (const auto& m : movies) if (m.title.find(term) != string::npos) return ToResolver(m);
                return monostate{};
            }});
        // clang-format on
        schema = make_unique<Schema>(builder.Build({.allowIntrospection = true}));
    }

    unique_ptr<Schema> schema;
};

TEST_F(CodeFirstEndToEndTest, QueriesReflectedObjectFields) {
    auto result = schema->Resolve({.query = "query { books { id title author rating } }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(result.data.value()["books"][0]["title"], "Clean Code");
    EXPECT_DOUBLE_EQ(result.data.value()["books"][0]["rating"].get<double>(), 4.5);
}

TEST_F(CodeFirstEndToEndTest, ResolvesInterfaceFieldToConcreteTypeAutomatically) {
    auto result = schema
                      ->Resolve({.query = R"(
        query {
            node(id: "b1") { __typename id }
        }
    )"})
                      .get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(result.data.value()["node"]["__typename"], "Book");
    EXPECT_EQ(result.data.value()["node"]["id"], "b1");
}

TEST_F(CodeFirstEndToEndTest, ResolvesUnionFieldToConcreteTypeAutomatically) {
    auto result = schema
                      ->Resolve({.query = R"(
        query {
            search(term: "Imitation") {
                __typename
                ... on Book { title author }
                ... on Movie { title durationMinutes }
            }
        }
    )"})
                      .get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(result.data.value()["search"]["__typename"], "Movie");
    EXPECT_EQ(result.data.value()["search"]["durationMinutes"], 114);
}
