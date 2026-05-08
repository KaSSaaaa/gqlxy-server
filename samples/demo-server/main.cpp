#include <atomic>
#include <fstream>
#include <gqlxy/server/pubsub.h>
#include <gqlxy/server/resolver_args.h>
#include <gqlxy/server/resolvers.h>
#include <gqlxy/server/schema.h>
#include <gqlxy/server/standalone/standalone_server.h>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::server;
using json = nlohmann::json;

// ─── In-memory data store ────────────────────────────────────────────────────

struct Book {
    string id, title, author;
    int year;
    optional<double> rating;
};

struct Review {
    string id, bookId, author, content;
    int stars;
};

class DateScalar : public ScalarType {
public:
    DateScalar(const string& v) : ScalarType([=]() -> json { return v; }) {}

    static json Parse(const json& v) {
        return v.get<string>();
    }
};

static mutex g_mutex;
static vector<Book> g_books = {
    {"1", "The Pragmatic Programmer", "David Thomas & Andrew Hunt", 1999, 4.8},
    {"2", "Clean Code", "Robert C. Martin", 2008, 4.5},
    {"3", "Design Patterns", "Gang of Four", 1994, 4.7},
};
static vector<Review> g_reviews = {
    {"1", "1", "Alice", "A must-read for every developer.", 5},
    {"2", "2", "Bob", "Opinionated but very valuable.", 4},
};
static atomic<int> g_nextBookId{4};
static atomic<int> g_nextReviewId{3};

// ─── Resolver helpers ────────────────────────────────────────────────────────

static Resolver bookToResolver(const Book& b) {
    // clang-format off
    return Resolver{
        {"id",     b.id},
        {"title",  b.title},
        {"author", b.author},
        {"year",   b.year},
        {"rating", b.rating.has_value() ? ValueResolver{b.rating.value()} : ValueResolver{monostate{}}}
    };
    // clang-format on
}

static Resolver reviewToResolver(const Review& r) {
    // clang-format off
    return Resolver{
        {"id",      r.id},
        {"bookId",  r.bookId},
        {"author",  r.author},
        {"content", r.content},
        {"stars",   r.stars}
    };
    // clang-format on
}

// ─── main ────────────────────────────────────────────────────────────────────

int main() {
    ifstream schemaFile(SCHEMA_PATH);
    if (!schemaFile) {
        cerr << "Cannot open schema: " << SCHEMA_PATH << "\n";
        return 1;
    }
    ostringstream ss;
    ss << schemaFile.rdbuf();
    const string typeDefs = ss.str();
    PubSub pubsub;

    auto timeChanged = async(launch::async, [&]() {
        while (true) {
            auto now = chrono::system_clock::now();
            time_t t = chrono::system_clock::to_time_t(now);
            ostringstream time;
            time << put_time(localtime(&t), "%Y-%m-%dT%H:%M:%S");
            pubsub.Publish("TIME_CHANGED", DateScalar(time.str()));
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
    });

    // clang-format off
    Schema schema({
        .typeDefs = typeDefs,
        .resolvers = {
            {"Query", Resolver{
                {"books", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
                    lock_guard lock(g_mutex);
                    vector<ValueResolver> result;
                    result.reserve(g_books.size());
                    for (const auto& b : g_books) result.push_back(bookToResolver(b));
                    return result;
                }}},
                {"book", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
                    auto id = args.Args()["id"].get<string>();
                    lock_guard lock(g_mutex);
                    for (const auto& b : g_books)
                        if (b.id == id) return bookToResolver(b);
                    return monostate{};
                }}},
                {"reviews", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
                    auto bookId = args.Args()["bookId"].get<string>();
                    lock_guard lock(g_mutex);
                    vector<ValueResolver> result;
                    for (const auto& r : g_reviews)
                        if (r.bookId == bookId) result.push_back(reviewToResolver(r));
                    return result;
                }}}
            }},
            {"Mutation", Resolver{
                {"addBook", FunctionResolver{[&pubsub](const ResolverArgs& args) -> ValueResolver {
                    Book b{
                        .id = to_string(g_nextBookId++),
                        .title = args.Args()["title"].get<string>(),
                        .author = args.Args()["author"].get<string>(),
                        .year = args.Args()["year"].get<int>(),
                        .rating = nullopt
                    };
                    {
                        lock_guard lock(g_mutex);
                        g_books.push_back(b);
                    }
                    pubsub.Publish("BOOK_ADDED", bookToResolver(b));
                    return bookToResolver(b);
                }}},
                {"addReview", FunctionResolver{[&pubsub](const ResolverArgs& args) -> ValueResolver {
                    Review r{
                        .id = to_string(g_nextReviewId++),
                        .bookId = args.Args()["bookId"].get<string>(),
                        .author = args.Args()["author"].get<string>(),
                        .content = args.Args()["content"].get<string>(),
                        .stars = args.Args()["stars"].get<int>()
                    };
                    {
                        lock_guard lock(g_mutex);
                        g_reviews.push_back(r);
                    }
                    pubsub.Publish("REVIEW_ADDED_" + r.bookId, reviewToResolver(r));
                    pubsub.Publish("REVIEW_ADDED", reviewToResolver(r));
                    return reviewToResolver(r);
                }}}
            }},
            {"Subscription", Resolver{
                {"reviewAdded", SubscriptionResolver{[&pubsub](const ResolverArgs& args) {
                    if (args.Args().contains("bookId") && !args.Args()["bookId"].is_null()) {
                        auto bookId = args.Args()["bookId"].get<string>();
                        return pubsub.AsyncIterator({"REVIEW_ADDED_" + bookId});
                    }
                    return pubsub.AsyncIterator({"REVIEW_ADDED"});
                }}},
                {"bookAdded", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                    return pubsub.AsyncIterator({"BOOK_ADDED"});
                }}},
                {"timeChanged", SubscriptionResolver{[&pubsub](const ResolverArgs&) {
                    return pubsub.AsyncIterator({"TIME_CHANGED"});
                }}}
            }}
        },
        .allowIntrospection = true,
    });
    // clang-format on

    StandaloneServer server({.schema = schema, .port = 4000});

    cout << "🚀 GQLXY demo server ready at " << server.GetUrl() << "\n";
    cout << "   Protocols: HTTP POST/GET · graphql-transport-ws · graphql-ws · graphql-sse\n";
    cout << "   Press Ctrl+C to stop.\n\n";

    server.Start();
    return 0;
}
