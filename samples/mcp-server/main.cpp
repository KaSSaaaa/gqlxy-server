#include <atomic>
#include <fstream>
#include <gqlxy/core/utils/optional.h>
#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/server/pubsub.h>
#include <gqlxy/server/resolver_args.h>
#include <gqlxy/server/resolvers.h>
#include <gqlxy/server/schema.h>
#include <gqlxy/server/subscription.h>
#include <gqlxy/server/standalone/standalone_server.h>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::server;
using namespace gqlxy::utils;

struct Book {
    string id, title, author;
    int year;
    optional<double> rating;
};

static Resolver bookToResolver(const Book& b) {
    return Resolver {
        {"id", b.id},
        {"title", b.title},
        {"author", b.author},
        {"year", b.year},
        {"rating", b.rating}
    };
}

int main() {
    ifstream schemaFile(SCHEMA_PATH);
    if (!schemaFile) {
        cerr << "Cannot open schema: " << SCHEMA_PATH << "\n";
        return 1;
    }
    ostringstream ss;
    ss << schemaFile.rdbuf();
    const string typeDefs = ss.str();

    mutex mutex;
    atomic<int> nextId{4};
    PubSub pubsub;

    vector<Book> books = {
        Book{
            .id = "1",
            .title = "The Pragmatic Programmer",
            .author = "David Thomas & Andrew Hunt",
            .year = 1999,
            .rating = 4.8
        },
        Book{
            .id = "2",
            .title = "Clean Code",
            .author = "Robert C. Martin",
            .year = 2008,
            .rating = 4.5
        },
        Book{
            .id = "3",
            .title = "Design Patterns",
            .author = "Gang of Four",
            .year = 1994,
            .rating = 4.7
        },
    };

    // clang-format off
    Schema schema({
        .typeDefs = typeDefs,
        .resolvers = {
            {"Query", Resolver{
                {"books", FunctionResolver{[&](const ResolverArgs&) -> ValueResolver {
                    lock_guard lock(mutex);
                    return to_vector(books | views::transform(bookToResolver));
                }}},
                {"book", FunctionResolver{[&](const ResolverArgs& args) -> ValueResolver {
                    auto id = args.Args()["id"].get<string>();
                    lock_guard lock(mutex);
                    return and_then(to_optional(books, ranges::find_if(books, [&](const auto& b) {
                        return b.id == id;
                    })), [](const auto& value) {
                        return make_optional(bookToResolver(value));
                    });
                }}},
                {"booksByAuthor", FunctionResolver{[&](const ResolverArgs& args) -> ValueResolver {
                    auto author = args.Args()["author"].get<string>();
                    lock_guard lock(mutex);
                    return to_vector(books
                        | views::filter([&](const auto& b) { return b.author == author; })
                        | views::transform(bookToResolver));
                }}}
            }},
            {"Mutation", Resolver{
                {"addBook", FunctionResolver{[&](const ResolverArgs& args) -> ValueResolver {
                    auto& a = args.Args();
                    Book book{
                        .id = to_string(nextId++),
                        .title = a["title"].get<string>(),
                        .author = a["author"].get<string>(),
                        .year = a["year"].get<int>(),
                        .rating = a.contains("rating") && !a["rating"].is_null()
                                  ? optional{a["rating"].get<double>()} : nullopt
                    };
                    {lock_guard lock(mutex); books.push_back(book);}
                    pubsub.Publish("bookAdded", bookToResolver(book));
                    return bookToResolver(book);
                }}},
                {"deleteBook", FunctionResolver{[&](const ResolverArgs& args) -> ValueResolver {
                    auto id = args.Args()["id"].get<string>();
                    lock_guard lock(mutex);
                    auto beforeDelete = books.size();
                    books = to_vector(books | views::filter([&](const auto& b) { return b.id == id; }));
                    if (books.size() == beforeDelete) return false;
                    pubsub.Publish("bookDeleted", id);
                    return true;
                }}}
            }},
            {"Subscription", Resolver{
                {"bookAdded", SubscriptionResolver{[&](const ResolverArgs&) {
                    return pubsub.AsyncIterator({"bookAdded"});
                }}},
                {"bookDeleted", SubscriptionResolver{[&](const ResolverArgs&) {
                    return pubsub.AsyncIterator({"bookDeleted"});
                }}}
            }}
        },
        .allowIntrospection = true,
    });
    // clang-format on

    StandaloneServer server({
        .schema = schema,
        .port = 4010,
        .path = "/graphql",
        .mcp = McpServerOptions {
            .path = "/mcp",
            .policy = DefaultMcpPolicy::Enabled,
            .additionalTools = {
                mcp::McpTool{
                    .name = "Hello",
                    .description = "Say hello to the server",
                    .handler = [](const auto&) {
                        cerr << "Hello world!" << endl;
                        return SubscriptionHandle::SingleShot({});
                    }
                }
            }
        }
    });

    cout << format("GraphQL endpoint : {}", server.GetUrl()) << endl;
    cout << "MCP endpoint : http://0.0.0.0:4010/mcp" << endl;
    cout << "Press Ctrl+C to stop." << endl << endl;

    server.Start();
    return 0;
}

