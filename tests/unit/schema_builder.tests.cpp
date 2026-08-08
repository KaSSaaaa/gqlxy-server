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
};

struct Movie {
    static constexpr const char* GQLName = "Movie";
    string id;
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

static Schema BuildTestSchema() {
    SchemaBuilder builder;
    builder
        .AddTypeDefs(R"(
                type Query {
                    book: Book!
                    node(id: ID!): Node
                    search: SearchResult
                }
            )")
        .AddType<Book>()
        .AddUnion<Book, Movie>("SearchResult")
        .AddQuery("book", FunctionResolver {[](const ResolverArgs&) -> ValueResolver {
                      return ToResolver(Book {.id = "1", .title = "Clean Code"});
                  }})
        .AddQuery("node", FunctionResolver {[](const ResolverArgs&) -> ValueResolver {
                      return ToResolver(Movie {.id = "2", .durationMinutes = 100});
                  }})
        .AddQuery("search", FunctionResolver {[](const ResolverArgs&) -> ValueResolver {
                      return ToResolver(Movie {.id = "2", .durationMinutes = 100});
                  }});
    return builder.Build({.allowIntrospection = true});
}

TEST(SchemaBuilder, BuildsTypeDefsForRegisteredTypesAndUnions) {
    Schema schema = BuildTestSchema();
    auto result = schema.Resolve({.query = "query { book { id title } }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(result.data.value()["book"]["title"], "Clean Code");
}

TEST(SchemaBuilder, AutoRegistersInterfaceResolveType) {
    Schema schema = BuildTestSchema();
    auto result = schema.Resolve({.query = "query { node(id: \"2\") { __typename id } }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(result.data.value()["node"]["__typename"], "Movie");
    EXPECT_EQ(result.data.value()["node"]["id"], "2");
}

TEST(SchemaBuilder, AutoRegistersUnionResolveType) {
    Schema schema = BuildTestSchema();
    auto result = schema.Resolve({.query = "query { search { __typename ... on Movie { durationMinutes } } }"}).get();
    ASSERT_FALSE(result.errors.has_value());
    EXPECT_EQ(result.data.value()["search"]["__typename"], "Movie");
    EXPECT_EQ(result.data.value()["search"]["durationMinutes"], 100);
}
