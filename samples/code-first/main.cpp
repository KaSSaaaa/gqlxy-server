#include <gqlxy/server/reflexion.h>
#include <gqlxy/server/resolver_args.h>
#include <gqlxy/server/schema_builder.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using namespace std;
using namespace gqlxy;
using namespace nlohmann;

// ─── Code-first types ────────────────────────────────────────────────────────
// Plain aggregates: field names, types, and values are all derived automatically
// via Boost.PFR — no macros, no manual field lists.

struct Node {
    GQL_TYPE(Node)
    std::string id;
};

struct Book {
    GQL_TYPE(Book)
    std::string id;
    std::string title;
    std::string author;
    std::optional<double> rating;
};

struct Movie {
    GQL_TYPE(Movie)
    std::string id;
    std::string title;
    int durationMinutes;
};

template<>
struct gqlxy::GQLImplements<Book> {
    using Interfaces = std::tuple<Node>;
};
template<>
struct gqlxy::GQLImplements<Movie> {
    using Interfaces = std::tuple<Node>;
};

// ─── In-memory data store ────────────────────────────────────────────────────

static vector<Book> g_books = {
    {.id = "b1", .title = "Clean Code", .author = "Robert C. Martin", .rating = 4.5},
    {.id = "b2", .title = "The Pragmatic Programmer", .author = "David Thomas & Andrew Hunt", .rating = 4.8},
};
static vector<Movie> g_movies = {
    {.id = "m1", .title = "The Imitation Game", .durationMinutes = 114},
};

static optional<ValueResolver> FindNode(const string& id) {
    for (const auto& b : g_books)
        if (b.id == id) return ToResolver(b);
    for (const auto& m : g_movies)
        if (m.id == id) return ToResolver(m);
    return nullopt;
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {
    cout << "=== GQLXY — code-first sample ===" << endl << endl;

    const string rootTypeDefs = R"(
        type Query {
            books: [Book!]!
            book(id: ID!): Book
            node(id: ID!): Node
            search(term: String!): SearchResult
        }
    )";

    // clang-format off
    SchemaBuilder builder;
    builder.AddTypeDefs(rootTypeDefs)
        .AddType<Book>()
        .AddType<Movie>()
        .AddUnion<Book, Movie>("SearchResult")
        .AddQuery("books", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
            vector<ValueResolver> result;
            for (const auto& b : g_books) result.push_back(ToResolver(b));
            return result;
        }})
        .AddQuery("book", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
            auto id = args.Args()["id"].get<string>();
            for (const auto& b : g_books)
                if (b.id == id) return ToResolver(b);
            return monostate{};
        }})
        .AddQuery("node", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
            return FindNode(args.Args()["id"].get<string>()).value_or(ValueResolver{monostate{}});
        }})
        .AddQuery("search", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
            auto term = args.Args()["term"].get<string>();
            for (const auto& b : g_books)
                if (b.title.find(term) != string::npos) return ToResolver(b);
            for (const auto& m : g_movies)
                if (m.title.find(term) != string::npos) return ToResolver(m);
            return monostate{};
        }});
    // clang-format on

    Schema schema = builder.Build({.allowIntrospection = true});

    const string query = R"(
        query {
            books { id title author rating }
            node(id: "b1") { __typename id }
            search(term: "Imitation") {
                __typename
                ... on Book { title author }
                ... on Movie { title durationMinutes }
            }
        }
    )";

    auto result = schema.Resolve({.query = query}).get();

    if (result.errors.has_value()) {
        for (const auto& e : result.errors.value())
            cerr << "Error: " << e.message << endl;
        return 1;
    }

    cout << result.data.value().dump(2) << endl;
    return 0;
}
