#include <atomic>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <condition_variable>
#include <fstream>
#include <future>
#include <gqlxy/client/client.h>
#include <gqlxy/client/links/http_link.h>
#include <gqlxy/client/links/ws_link.h>
#include <gqlxy/core/results.h>
#include <gqlxy/server/pubsub.h>
#include <gqlxy/server/resolver_args.h>
#include <gqlxy/server/resolvers.h>
#include <gqlxy/server/schema.h>
#include <gqlxy/server/standalone/standalone_server.h>
#include <gtest/gtest.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;
using namespace gqlxy;
using namespace gqlxy::server;
using namespace gqlxy::utils;
using namespace nlohmann;

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = net::ip::tcp;

struct BookData {
    string id, title, author;
    int year;
};

struct ReviewData {
    string id, bookId, author, content;
    int stars;
};

struct SseResult {
    vector<GraphQLResponse> events;
    bool completed = false;
};


class StandaloneServerE2ETest : public testing::Test {
protected:
    void SetUp() override {
        _books = {
            {"1", "The Pragmatic Programmer", "David Thomas & Andrew Hunt", 1999},
            {"2", "Clean Code", "Robert C. Martin", 2008},
            {"3", "Design Patterns", "Gang of Four", 1994},
        };
        _reviews = {
            {"1", "1", "Alice", "A must-read.", 5},
            {"2", "2", "Bob", "Opinionated.", 4},
        };
        _nextBookId = 4;
        _nextReviewId = 3;

        _pubSub = make_shared<PubSub>();

        // clang-format off
        _schema = make_shared<Schema>(SchemaOptions{
            .typeDefs = readFile(DEMO_SERVER_SCHEMA_PATH),
            .resolvers = {
                {"Query", Resolver{
                    {"books", FunctionResolver{[this](const ResolverArgs&) -> ValueResolver {
                        return to_vector(_books | views::transform([](const auto& b) { return ToResolver(b); }));
                    }}},
                    {"book", FunctionResolver{[this](const ResolverArgs& args) -> ValueResolver {
                        auto id = args.Args()["id"].get<string>();
                        for (const auto& b : _books)
                            if (b.id == id) return ToResolver(b);
                        return monostate{};
                    }}},
                    {"reviews", FunctionResolver{[this](const ResolverArgs& args) -> ValueResolver {
                        auto bookId = args.Args()["bookId"].get<string>();
                        return to_vector(_reviews | views::filter([bookId](const auto& r) {
                            return r.bookId == bookId;
                        }) | views::transform([](const auto& r) {
                            return ToResolver(r);
                        }));
                    }}}
                }},
                {"Mutation", Resolver{
                    {"addBook", FunctionResolver{[this](const ResolverArgs& args) -> ValueResolver {
                        BookData b{
                            to_string(_nextBookId++),
                            args.Args()["title"].get<string>(),
                            args.Args()["author"].get<string>(),
                            args.Args()["year"].get<int>()
                        };
                        _books.push_back(b);
                        _pubSub->Publish("BOOK_ADDED", ToResolver(b));
                        return ToResolver(b);
                    }}},
                    {"addReview", FunctionResolver{[this](const ResolverArgs& args) -> ValueResolver {
                        ReviewData r{
                            to_string(_nextReviewId++),
                            args.Args()["bookId"].get<string>(),
                            args.Args()["author"].get<string>(),
                            args.Args()["content"].get<string>(),
                            args.Args()["stars"].get<int>()
                        };
                        _reviews.push_back(r);
                        _pubSub->Publish("REVIEW_ADDED", ToResolver(r));
                        return ToResolver(r);
                    }}}
                }},
                {"Subscription", Resolver{
                    {"bookAdded", SubscriptionResolver{[this](const ResolverArgs&) {
                        return _pubSub->AsyncIterator({"BOOK_ADDED"});
                    }}},
                    {"reviewAdded", SubscriptionResolver{[this](const ResolverArgs& args) {
                        if (args.Args().contains("bookId") && !args.Args()["bookId"].is_null())
                            return _pubSub->AsyncIterator({"REVIEW_ADDED_" + args.Args()["bookId"].get<string>()});
                        return _pubSub->AsyncIterator({"REVIEW_ADDED"});
                    }}}
                }}
            },
            .allowIntrospection = true
        });
        // clang-format on

        _server = make_shared<StandaloneServer>(StandaloneServerOptions{
            .schema = *_schema,
            .port = _port
        });
        _server->StartAsync();
        WaitForPort(_port);
    }

    void TearDown() override {
        _server->Stop();
        _server.reset();
        _schema.reset();
        _pubSub.reset();
    }

    static Resolver ToResolver(const BookData& b) {
        return Resolver{
            {"id", b.id},
            {"title", b.title},
            {"author", b.author},
            {"year", b.year}
        };
    }
    static Resolver ToResolver(const ReviewData& r) {
        return Resolver{
            {"id", r.id},
            {"bookId", r.bookId},
            {"author", r.author},
            {"content", r.content},
            {"stars", r.stars}
        };
    }

    static string readFile(const char* path) {
        ifstream f(path);
        ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    static GraphQLResponse await(Observable<GraphQLResponse> obs) {
        auto p = make_shared<promise<GraphQLResponse>>();
        obs.subscribe(
            [p](const GraphQLResponse& r) { p->set_value(r); },
            [p](const exception_ptr& e) { p->set_exception(e); },
            [p]() {
                try { p->set_exception(make_exception_ptr(runtime_error("Observable completed without a value"))); }
                catch (...) {}
            });
        return p->get_future().get();
    }
    static SseResult syncSubscribeHttp(Client& client, const string& query,
                                       const json& variables = nullptr) {
        SseResult out;
        mutex mtx;
        condition_variable cv;

        client.Subscribe({.query = query, .variables = variables})
            .subscribe(
                [&](const GraphQLResponse& r) {
                    lock_guard lock(mtx);
                    out.events.push_back(r);
                },
                [&](const exception_ptr&) {
                    lock_guard lock(mtx);
                    out.completed = true;
                    cv.notify_one();
                },
                [&]() {
                    lock_guard lock(mtx);
                    out.completed = true;
                    cv.notify_one();
                });

        unique_lock lock(mtx);
        cv.wait_for(lock, seconds(10), [&] { return out.completed; });
        return out;
    }

    vector<BookData> _books;
    vector<ReviewData> _reviews;
    atomic<int> _nextBookId {4};
    atomic<int> _nextReviewId {3};
    shared_ptr<PubSub> _pubSub = nullptr;
    shared_ptr<Schema> _schema = nullptr;
    shared_ptr<StandaloneServer> _server = nullptr;
    const uint16_t _port = 14001;

    string ServerUrl() {
        return format("http://127.0.0.1:{}/graphql", _port);
    }

    string WsUrl() {
        return format("ws://127.0.0.1:{}/graphql", _port);
    }

    Client MakeHttpClient() {
        return Client({.link = make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl()})});
    }

    Client MakeHttpClientNoTransforms() {
        return Client({.link = make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl()}), .documentTransforms = {}});
    }

    Client MakeWsClient() {
        return Client({.link = make_shared<WsLink>(WsLinkOptions{.url = WsUrl()})});
    }

    static void WaitForPort(int port) {
        for (int i = 0; i < 50; i++) {
            try {
                net::io_context ioc;
                beast::tcp_stream stream(ioc);
                stream.expires_after(milliseconds(100));
                stream.connect(tcp::resolver(ioc).resolve("127.0.0.1", to_string(port)));
                return;
            } catch (...) {
                this_thread::sleep_for(milliseconds(20));
            }
        }
    }
};

TEST_F(StandaloneServerE2ETest, HttpQueryBooksContainsAllSeededBooks) {
    auto client = MakeHttpClient();
    auto res = await(client.Query({.query = "{ books { id title author year } }"}));
    auto books = res.data.value()["books"];

    ASSERT_FALSE(res.errors) << res.errors->front().message;
    EXPECT_GE(books.size(), 3u);

    auto findBook = [&](const string& id) {
        return ranges::find_if(books, [&](const auto& b) { return b["id"] == id; });
    };

    auto it1 = findBook("1");
    ASSERT_NE(it1, books.end());
    EXPECT_EQ((*it1)["title"], "The Pragmatic Programmer");
    EXPECT_EQ((*it1)["year"], 1999);

    auto it2 = findBook("2");
    ASSERT_NE(it2, books.end());
    EXPECT_EQ((*it2)["title"], "Clean Code");
}

TEST_F(StandaloneServerE2ETest, HttpQueryBookByIdReturnsCorrectBook) {
    auto client = MakeHttpClient();
    auto res    = await(client.Query({.query = "query($id: ID!) { book(id: $id) { id title author } }", .variables = {{"id", "3"}}}));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    auto book = res.data.value()["book"];
    EXPECT_EQ(book["id"], "3");
    EXPECT_EQ(book["title"], "Design Patterns");
    EXPECT_EQ(book["author"], "Gang of Four");
}

TEST_F(StandaloneServerE2ETest, HttpQueryBookByIdReturnsNullForMissingId) {
    auto client = MakeHttpClient();
    auto res = await(client.Query({
        .query = "query($id: ID!) { book(id: $id) { title } }",
        .variables = {
            {"id", "9999"}
        }
    }));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    EXPECT_TRUE(res.data.value()["book"].is_null());
}

TEST_F(StandaloneServerE2ETest, HttpQueryReviewsForBook) {
    auto client = MakeHttpClient();
    auto res = await(client.Query({
        .query = R"({ reviews(bookId: "1") { id author stars } })"
    }));
    auto reviews = res.data.value()["reviews"];
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    ASSERT_GE(reviews.size(), 1u);

    auto it = ranges::find_if(reviews, [](const auto& r) { return r["author"] == "Alice"; });
    ASSERT_NE(it, reviews.end());
    EXPECT_EQ((*it)["stars"], 5);
}

TEST_F(StandaloneServerE2ETest, HttpMutationAddBookReturnsNewBook) {
    auto client = MakeHttpClient();
    auto res = await(client.Mutation({
        .query = R"(
            mutation {
                addBook(title: "SICP", author: "Abelson & Sussman", year: 1996) {
                    id
                    title
                    author
                    year
                }
            }
        )"
    }));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    auto book = res.data.value()["addBook"];
    EXPECT_EQ(book["title"], "SICP");
    EXPECT_EQ(book["author"], "Abelson & Sussman");
    EXPECT_EQ(book["year"], 1996);
    EXPECT_FALSE(book["id"].get<string>().empty());
}

TEST_F(StandaloneServerE2ETest, HttpMutationAddReviewReturnsNewReview) {
    auto client = MakeHttpClient();
    auto res = await(client.Mutation({
        .query = R"(
            mutation {
                addReview(bookId: "1", author: "Dave", content: "Excellent!", stars: 5) {
                    id
                    bookId
                    author
                    stars
                }
            }
        )"
    }));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    auto review = res.data.value()["addReview"];
    EXPECT_EQ(review["bookId"], "1");
    EXPECT_EQ(review["author"], "Dave");
    EXPECT_EQ(review["stars"], 5);
    EXPECT_FALSE(review["id"].get<string>().empty());
}

TEST_F(StandaloneServerE2ETest, WsSubscriptionBookAdded) {
    auto client = MakeWsClient();

    optional<GraphQLResponse> received;
    mutex mtx;
    condition_variable cv;

    auto sub = client.Subscribe({.query = "subscription { bookAdded { id title } }"})
        .subscribe(
            [&](const GraphQLResponse& r) {
                lock_guard lock(mtx);
                received = r;
                cv.notify_one();
            },
            [&](const exception_ptr&) { cv.notify_one(); },
            [&]() { cv.notify_one(); });

    auto pub = async(launch::async, [this] {
        this_thread::sleep_for(milliseconds(50));
        _pubSub->Publish("BOOK_ADDED", Resolver{{"id", "99"}, {"title", "Async Book"}});
    });

    unique_lock lock(mtx);
    cv.wait_for(lock, seconds(5), [&] { return received.has_value(); });
    pub.get();
    sub.Unsubscribe();

    ASSERT_TRUE(received.has_value());
    ASSERT_FALSE(received->errors) << received->errors->front().message;
    auto bookAdded = received->data.value()["bookAdded"];
    EXPECT_EQ(bookAdded["id"], "99");
    EXPECT_EQ(bookAdded["title"], "Async Book");
}

TEST_F(StandaloneServerE2ETest, WsSubscriptionReviewAdded) {
    auto client = MakeWsClient();

    optional<GraphQLResponse> received;
    mutex mtx;
    condition_variable cv;

    auto sub = client.Subscribe({.query = "subscription { reviewAdded { id author stars } }"})
        .subscribe(
            [&](const GraphQLResponse& r) {
                lock_guard lock(mtx);
                received = r;
                cv.notify_one();
            },
            [&](const exception_ptr&) { cv.notify_one(); },
            [&]() { cv.notify_one(); });

    auto pub = async(launch::async, [this] {
        this_thread::sleep_for(milliseconds(50));
        _pubSub->Publish("REVIEW_ADDED", Resolver{
            {"id", "55"},
            {"author", "Carol"},
            {"stars", 4}
        });
    });

    unique_lock lock(mtx);
    cv.wait_for(lock, seconds(5), [&] { return received.has_value(); });
    pub.get();
    sub.Unsubscribe();

    ASSERT_TRUE(received.has_value());
    ASSERT_FALSE(received->errors) << received->errors->front().message;
    auto reviewAdded = received->data.value()["reviewAdded"];
    EXPECT_EQ(reviewAdded["id"], "55");
    EXPECT_EQ(reviewAdded["author"], "Carol");
    EXPECT_EQ(reviewAdded["stars"], 4);
}

TEST_F(StandaloneServerE2ETest, HttpIntrospectionReturnsCorrectRootTypes) {
    auto client = MakeHttpClientNoTransforms();
    auto res = await(client.Query({
        .query = "{ __schema { queryType { name } mutationType { name } subscriptionType { name } } }"
    }));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    auto schema = res.data.value()["__schema"];
    EXPECT_EQ(schema["queryType"]["name"], "Query");
    EXPECT_EQ(schema["mutationType"]["name"], "Mutation");
    EXPECT_EQ(schema["subscriptionType"]["name"], "Subscription");
}

TEST_F(StandaloneServerE2ETest, HttpFullIntrospectionQueryReturnsNoErrors) {
    auto client = MakeHttpClientNoTransforms();
    auto out = syncSubscribeHttp(client, readFile(INTROSPECTION_QUERY_PATH));

    ASSERT_GE(out.events.size(), 1u);
    auto& res = out.events.front();
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    auto types = res.data.value()["__schema"]["types"];

    auto hasType = [&](const string& name) {
        return any_of(types.begin(), types.end(), [&](const auto& t) { return t["name"] == name; });
    };
    EXPECT_TRUE(hasType("Book"));
    EXPECT_TRUE(hasType("Review"));
    EXPECT_TRUE(hasType("Query"));
    EXPECT_TRUE(hasType("Mutation"));
    EXPECT_TRUE(hasType("Subscription"));
}

TEST_F(StandaloneServerE2ETest, SseQueryReturnsNextThenComplete) {
    auto client = MakeHttpClient();
    auto out = syncSubscribeHttp(client, "{ books { id title } }");

    ASSERT_GE(out.events.size(), 1u) << "Expected at least one event";
    ASSERT_TRUE(out.completed);

    ASSERT_FALSE(out.events.front().errors) << out.events.front().errors->front().message;
    auto books = out.events.front().data.value()["books"];
    ASSERT_TRUE(books.is_array());
    EXPECT_GE(books.size(), 2u);
}

TEST_F(StandaloneServerE2ETest, SseMutationReturnsNextThenComplete) {
    auto client = MakeHttpClient();
    auto out = syncSubscribeHttp(client, R"(mutation { addBook(title:"SSE Test", author:"Y", year:2025) { title author } })");

    ASSERT_GE(out.events.size(), 1u);
    ASSERT_TRUE(out.completed);

    ASSERT_FALSE(out.events.front().errors) << out.events.front().errors->front().message;
    auto book = out.events.front().data.value()["addBook"];
    EXPECT_EQ(book["title"], "SSE Test");
    EXPECT_EQ(book["author"], "Y");
}
