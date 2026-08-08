#include <gqlxy/server/reflexion.h>
#include <gtest/gtest.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace gqlxy;

namespace {

struct Node {
    GQL_TYPE(Node)
    string id;
};

struct Review {
    GQL_TYPE(Review)
    string id;
    string content;
};

struct Book {
    GQL_TYPE(Book)
    string id;
    string title;
    optional<double> rating;
    vector<Review> reviews;
};
}

template<>
struct gqlxy::GQLImplements<Book> {
    using Interfaces = tuple<Node>;
};

template<>
struct gqlxy::GQLFieldOverrides<Book> {
    static const unordered_map<string, GQLFieldOptions>& Overrides() {
        static const unordered_map<string, GQLFieldOptions> overrides {
            {"rating", GQLFieldOptions {.type = "Float"}},
        };
        return overrides;
    }
};

TEST(Reflection, SdlTypeDefMapsScalarsWithIdConvention) {
    const string sdl = SdlTypeDef<Review>();
    EXPECT_EQ(sdl, "type Review {\n  id: ID!\n  content: String!\n}\n");
}

TEST(Reflection, SdlTypeDefAppliesOverridesAndNestedListAndImplementsClause) {
    const string sdl = SdlTypeDef<Book>();
    EXPECT_EQ(
        sdl, "type Book implements Node {\n"
             "  id: ID!\n"
             "  title: String!\n"
             "  rating: Float\n"
             "  reviews: [Review!]!\n"
             "}\n");
}

TEST(Reflection, SdlInterfaceDefEmitsInterfaceKeyword) {
    EXPECT_EQ(SdlInterfaceDef<Node>(), "interface Node {\n  id: ID!\n}\n");
}

TEST(Reflection, ToResolverConvertsScalarFieldsAndStampsDiscriminator) {
    Book book {.id = "1", .title = "Clean Code", .rating = 4.5, .reviews = {}};
    Resolver resolver = ToResolver(book);

    EXPECT_EQ(resolver["id"].As<string>(), "1");
    EXPECT_EQ(resolver["title"].As<string>(), "Clean Code");
    EXPECT_DOUBLE_EQ(resolver["rating"].As<double>(), 4.5);
    EXPECT_EQ(resolver[kGQLTypeNameKey].As<string>(), "Book");
}

TEST(Reflection, ToResolverHandlesEmptyOptionalAsNull) {
    Book book {.id = "1", .title = "Clean Code", .rating = nullopt, .reviews = {}};
    Resolver resolver = ToResolver(book);
    EXPECT_TRUE(resolver["rating"].IsNull());
}

TEST(Reflection, ToResolverRecursesIntoNestedReflectableVector) {
    Book book {
        .id = "1", .title = "Clean Code", .rating = nullopt, .reviews = {Review {.id = "r1", .content = "Great"}}};
    Resolver resolver = ToResolver(book);

    const auto& reviews = resolver["reviews"].As<vector<ValueResolver>>();
    ASSERT_EQ(reviews.size(), 1u);
    const Resolver& review = reviews[0].As<Resolver>();
    EXPECT_EQ(review.at("id").As<string>(), "r1");
    EXPECT_EQ(review.at(kGQLTypeNameKey).As<string>(), "Review");
}

TEST(Reflection, GQLValueOfResolvesUnionVariantToConcreteResolver) {
    Book book {.id = "1", .title = "Clean Code", .rating = nullopt, .reviews = {}};
    variant<Book, Review> value = book;

    ValueResolver resolved = GQLValueOf(value);
    EXPECT_EQ(resolved.As<Resolver>().at(kGQLTypeNameKey).As<string>(), "Book");
}
