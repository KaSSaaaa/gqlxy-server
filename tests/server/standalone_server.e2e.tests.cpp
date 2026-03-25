#include <gqlxy/ResolverArgs.h>
#include <gqlxy/pubsub.h>
#include <gqlxy/resolvers.h>
#include <gqlxy/schema.h>
#include <gqlxy/server/standalone_server.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <fstream>
#include <future>
#include <sstream>
#include <thread>
#include <vector>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::server;
using json = nlohmann::json;

namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = beast::websocket;
namespace net   = boost::asio;
using tcp       = net::ip::tcp;

// ─── Data types ───────────────────────────────────────────────────────────────

struct BookData    { string id, title, author; int year; };
struct ReviewData  { string id, bookId, author, content; int stars; };

static Resolver ToResolver(const BookData& b) {
    return Resolver{{"id", b.id}, {"title", b.title}, {"author", b.author}, {"year", b.year}};
}
static Resolver ToResolver(const ReviewData& r) {
    return Resolver{{"id", r.id}, {"bookId", r.bookId}, {"author", r.author}, {"content", r.content}, {"stars", r.stars}};
}

// ─── HTTP helper ──────────────────────────────────────────────────────────────

static string readFile(const char* path) {
    ifstream f(path);
    ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static json httpPost(int port, const string& path, const string& query,
                     const json& variables = json::object()) {
    net::io_context ioc;
    beast::tcp_stream stream(ioc);
    stream.expires_after(chrono::seconds(5));
    stream.connect(tcp::resolver(ioc).resolve("127.0.0.1", to_string(port)));

    json payload = {{"query", query}};
    if (!variables.empty()) payload["variables"] = variables;

    http::request<http::string_body> req{http::verb::post, path, 11};
    req.set(http::field::host, "localhost");
    req.set(http::field::content_type, "application/json");
    req.set(http::field::connection, "close");
    req.body() = payload.dump();
    req.prepare_payload();

    http::write(stream, req);

    beast::flat_buffer buf;
    http::response<http::string_body> res;
    http::read(stream, buf, res);

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    return json::parse(res.body());
}

// ─── SSE helper ───────────────────────────────────────────────────────────────

struct SseEvent {
    string event;
    json   data;
};

static vector<SseEvent> httpSSE(int port, const string& path, const string& query,
                                const json& variables = json::object()) {
    net::io_context ioc;
    beast::tcp_stream stream(ioc);
    stream.expires_after(chrono::seconds(10));
    stream.connect(tcp::resolver(ioc).resolve("127.0.0.1", to_string(port)));

    json payload = {{"query", query}};
    if (!variables.empty()) payload["variables"] = variables;

    http::request<http::string_body> req{http::verb::post, path, 11};
    req.set(http::field::host, "localhost");
    req.set(http::field::content_type, "application/json");
    req.set(http::field::accept, "text/event-stream");
    req.set(http::field::connection, "close");
    req.body() = payload.dump();
    req.prepare_payload();

    http::write(stream, req);

    beast::flat_buffer buf;
    http::response<http::string_body> res;
    http::read(stream, buf, res);

    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);

    const string& raw = res.body();

    vector<SseEvent> events;
    string currentEvent, currentData;
    istringstream ss(raw);
    string line;
    while (getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) {
            if (!currentEvent.empty() || !currentData.empty()) {
                SseEvent evt;
                evt.event = currentEvent;
                if (!currentData.empty()) evt.data = json::parse(currentData);
                events.push_back(std::move(evt));
                currentEvent.clear();
                currentData.clear();
            }
        } else if (line.starts_with("event: ")) {
            currentEvent = line.substr(7);
        } else if (line.starts_with("data: ")) {
            currentData = line.substr(6);
        }
    }
    if (!currentEvent.empty() || !currentData.empty()) {
        SseEvent evt{currentEvent, currentData.empty() ? json{} : json::parse(currentData)};
        events.push_back(std::move(evt));
    }
    return events;
}

// ─── WebSocket client (graphql-transport-ws) ──────────────────────────────────

class WsClient {
    net::io_context              _ioc;
    ws::stream<beast::tcp_stream> _ws;

public:
    WsClient(int port, const string& path, const string& subprotocol)
        : _ioc(), _ws(_ioc)
    {
        _ws.next_layer().expires_after(chrono::seconds(5));
        _ws.next_layer().connect(tcp::resolver(_ioc).resolve("127.0.0.1", to_string(port)));
        _ws.next_layer().expires_never();

        _ws.set_option(ws::stream_base::decorator([&](ws::request_type& req) {
            req.set(http::field::sec_websocket_protocol, subprotocol);
        }));
        _ws.handshake("localhost", path);
    }

    ~WsClient() {
        beast::error_code ec;
        _ws.close(ws::close_code::normal, ec);
    }

    void send(const json& msg) {
        _ws.write(net::buffer(msg.dump()));
    }

    json recv() {
        beast::flat_buffer buf;
        beast::error_code ec;
        _ws.read(buf, ec);
        if (ec) return {};
        return json::parse(beast::buffers_to_string(buf.data()));
    }
};

// ─── Test fixture (server starts once for the whole suite) ───────────────────

static vector<BookData>   s_books;
static vector<ReviewData> s_reviews;
static atomic<int>        s_nextBookId{4};
static atomic<int>        s_nextReviewId{3};
static PubSub*            s_pubsub  = nullptr;
static Schema*            s_schema  = nullptr;
static StandaloneServer*  s_server  = nullptr;
static constexpr int      s_port    = 14001;
static constexpr auto     s_path    = "/graphql";

static void waitForPort(int port) {
    for (int i = 0; i < 50; ++i) {
        try {
            net::io_context ioc;
            beast::tcp_stream stream(ioc);
            stream.expires_after(chrono::milliseconds(100));
            stream.connect(tcp::resolver(ioc).resolve("127.0.0.1", to_string(port)));
            return;
        } catch (...) {
            this_thread::sleep_for(chrono::milliseconds(20));
        }
    }
}

class StandaloneServerE2ETest : public testing::Test {
public:
    static void SetUpTestSuite() {
        s_books = {
            {"1", "The Pragmatic Programmer", "David Thomas & Andrew Hunt", 1999},
            {"2", "Clean Code", "Robert C. Martin", 2008},
            {"3", "Design Patterns", "Gang of Four", 1994},
        };
        s_reviews = {
            {"1", "1", "Alice", "A must-read.", 5},
            {"2", "2", "Bob", "Opinionated.", 4},
        };
        s_nextBookId   = 4;
        s_nextReviewId = 3;

        s_pubsub = new PubSub();

        // clang-format off
        s_schema = new Schema({
            .typeDefs = readFile(DEMO_SERVER_SCHEMA_PATH),
            .resolvers = {
                {"Query", Resolver{
                    {"books", FunctionResolver{[](const ResolverArgs&) -> ValueResolver {
                        vector<ValueResolver> result;
                        for (const auto& b : s_books) result.push_back(ToResolver(b));
                        return result;
                    }}},
                    {"book", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
                        auto id = args.Args()["id"].get<string>();
                        for (const auto& b : s_books)
                            if (b.id == id) return ToResolver(b);
                        return monostate{};
                    }}},
                    {"reviews", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
                        auto bookId = args.Args()["bookId"].get<string>();
                        vector<ValueResolver> result;
                        for (const auto& r : s_reviews)
                            if (r.bookId == bookId) result.push_back(ToResolver(r));
                        return result;
                    }}}
                }},
                {"Mutation", Resolver{
                    {"addBook", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
                        BookData b{
                            to_string(s_nextBookId++),
                            args.Args()["title"].get<string>(),
                            args.Args()["author"].get<string>(),
                            args.Args()["year"].get<int>()
                        };
                        s_books.push_back(b);
                        s_pubsub->Publish("BOOK_ADDED", ToResolver(b));
                        return ToResolver(b);
                    }}},
                    {"addReview", FunctionResolver{[](const ResolverArgs& args) -> ValueResolver {
                        ReviewData r{
                            to_string(s_nextReviewId++),
                            args.Args()["bookId"].get<string>(),
                            args.Args()["author"].get<string>(),
                            args.Args()["content"].get<string>(),
                            args.Args()["stars"].get<int>()
                        };
                        s_reviews.push_back(r);
                        s_pubsub->Publish("REVIEW_ADDED", ToResolver(r));
                        return ToResolver(r);
                    }}}
                }},
                {"Subscription", Resolver{
                    {"bookAdded", SubscriptionResolver{[](const ResolverArgs&) {
                        return s_pubsub->AsyncIterator({"BOOK_ADDED"});
                    }}},
                    {"reviewAdded", SubscriptionResolver{[](const ResolverArgs& args) {
                        if (args.Args().contains("bookId") && !args.Args()["bookId"].is_null())
                            return s_pubsub->AsyncIterator({"REVIEW_ADDED_" + args.Args()["bookId"].get<string>()});
                        return s_pubsub->AsyncIterator({"REVIEW_ADDED"});
                    }}}
                }}
            },
            .allowIntrospection = true
        });
        // clang-format on

        s_server = new StandaloneServer({.schema = *s_schema, .port = s_port});
        s_server->StartAsync();
        waitForPort(s_port);
    }

    static void TearDownTestSuite() {
        s_server->Stop();
        delete s_server;  s_server = nullptr;
        delete s_schema;  s_schema = nullptr;
        delete s_pubsub;  s_pubsub = nullptr;
    }
};

// ─── HTTP query tests ─────────────────────────────────────────────────────────

TEST_F(StandaloneServerE2ETest, HttpQueryBooksContainsAllSeededBooks) {
    auto res  = httpPost(s_port, s_path, "{ books { id title author year } }");
    auto books = res["data"]["books"];

    ASSERT_FALSE(res.contains("errors")) << res.dump();
    EXPECT_GE(books.size(), 3u);

    auto findBook = [&](const string& id) {
        return find_if(books.begin(), books.end(), [&](const auto& b) { return b["id"] == id; });
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
    auto res = httpPost(s_port, s_path,
                        "query($id: ID!) { book(id: $id) { id title author } }",
                        {{"id", "3"}});
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    auto book = res["data"]["book"];
    EXPECT_EQ(book["id"], "3");
    EXPECT_EQ(book["title"], "Design Patterns");
    EXPECT_EQ(book["author"], "Gang of Four");
}

TEST_F(StandaloneServerE2ETest, HttpQueryBookByIdReturnsNullForMissingId) {
    auto res = httpPost(s_port, s_path,
                        "query($id: ID!) { book(id: $id) { title } }",
                        {{"id", "9999"}});
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    EXPECT_TRUE(res["data"]["book"].is_null());
}

TEST_F(StandaloneServerE2ETest, HttpQueryReviewsForBook) {
    auto res     = httpPost(s_port, s_path, R"({ reviews(bookId: "1") { id author stars } })");
    auto reviews = res["data"]["reviews"];
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    ASSERT_GE(reviews.size(), 1u);

    auto it = find_if(reviews.begin(), reviews.end(),
                      [](const auto& r) { return r["author"] == "Alice"; });
    ASSERT_NE(it, reviews.end());
    EXPECT_EQ((*it)["stars"], 5);
}

// ─── HTTP mutation tests ──────────────────────────────────────────────────────

TEST_F(StandaloneServerE2ETest, HttpMutationAddBookReturnsNewBook) {
    auto res = httpPost(s_port, s_path,
                        R"(mutation { addBook(title: "SICP", author: "Abelson & Sussman", year: 1996) { id title author year } })");
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    auto book = res["data"]["addBook"];
    EXPECT_EQ(book["title"], "SICP");
    EXPECT_EQ(book["author"], "Abelson & Sussman");
    EXPECT_EQ(book["year"], 1996);
    EXPECT_FALSE(book["id"].get<string>().empty());
}

TEST_F(StandaloneServerE2ETest, HttpMutationAddReviewReturnsNewReview) {
    auto res = httpPost(s_port, s_path,
                        R"(mutation { addReview(bookId: "1", author: "Dave", content: "Excellent!", stars: 5) { id bookId author stars } })");
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    auto review = res["data"]["addReview"];
    EXPECT_EQ(review["bookId"], "1");
    EXPECT_EQ(review["author"], "Dave");
    EXPECT_EQ(review["stars"], 5);
    EXPECT_FALSE(review["id"].get<string>().empty());
}

// ─── WebSocket subscription tests ────────────────────────────────────────────

TEST_F(StandaloneServerE2ETest, WsSubscriptionBookAdded) {
    WsClient ws(s_port, s_path, "graphql-transport-ws");

    ws.send({{"type", "connection_init"}});
    auto ack = ws.recv();
    ASSERT_EQ(ack["type"], "connection_ack") << ack.dump();

    ws.send({
        {"type", "subscribe"},
        {"id", "1"},
        {"payload", {{"query", "subscription { bookAdded { id title } }"}}}
    });

    // Publish after a short delay so the subscription is registered
    auto pub = async(launch::async, [] {
        this_thread::sleep_for(chrono::milliseconds(50));
        s_pubsub->Publish("BOOK_ADDED", Resolver{{"id", "99"}, {"title", "Async Book"}});
    });

    auto msg = ws.recv();
    pub.get();

    ASSERT_EQ(msg["type"], "next") << msg.dump();
    auto d = msg.dump();
    auto bookAdded = msg["payload"]["data"]["bookAdded"];
    EXPECT_EQ(bookAdded["id"], "99");
    EXPECT_EQ(bookAdded["title"], "Async Book");
}

TEST_F(StandaloneServerE2ETest, WsSubscriptionReviewAdded) {
    WsClient ws(s_port, s_path, "graphql-transport-ws");

    ws.send({{"type", "connection_init"}});
    ASSERT_EQ(ws.recv()["type"], "connection_ack");

    ws.send({
        {"type", "subscribe"},
        {"id", "2"},
        {"payload", {{"query", "subscription { reviewAdded { id author stars } }"}}}
    });

    auto pub = async(launch::async, [] {
        this_thread::sleep_for(chrono::milliseconds(50));
        s_pubsub->Publish("REVIEW_ADDED",
                          Resolver{{"id", "55"}, {"author", "Carol"}, {"stars", 4}});
    });

    auto msg = ws.recv();
    pub.get();

    ASSERT_EQ(msg["type"], "next") << msg.dump();
    auto reviewAdded = msg["payload"]["data"]["reviewAdded"];
    EXPECT_EQ(reviewAdded["id"], "55");
    EXPECT_EQ(reviewAdded["author"], "Carol");
    EXPECT_EQ(reviewAdded["stars"], 4);
}

// ─── Introspection tests ──────────────────────────────────────────────────────

TEST_F(StandaloneServerE2ETest, HttpIntrospectionReturnsCorrectRootTypes) {
    auto res = httpPost(s_port, s_path,
                        "{ __schema { queryType { name } mutationType { name } subscriptionType { name } } }");
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    auto schema = res["data"]["__schema"];
    EXPECT_EQ(schema["queryType"]["name"], "Query");
    EXPECT_EQ(schema["mutationType"]["name"], "Mutation");
    EXPECT_EQ(schema["subscriptionType"]["name"], "Subscription");
}

TEST_F(StandaloneServerE2ETest, HttpFullIntrospectionQueryReturnsNoErrors) {
    auto res   = httpPost(s_port, s_path, readFile(INTROSPECTION_QUERY_PATH));
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    auto types = res["data"]["__schema"]["types"];

    auto hasType = [&](const string& name) {
        return any_of(types.begin(), types.end(), [&](const auto& t) { return t["name"] == name; });
    };
    EXPECT_TRUE(hasType("Book"));
    EXPECT_TRUE(hasType("Review"));
    EXPECT_TRUE(hasType("Query"));
    EXPECT_TRUE(hasType("Mutation"));
    EXPECT_TRUE(hasType("Subscription"));
}

// ─── SSE tests ────────────────────────────────────────────────────────────────

TEST_F(StandaloneServerE2ETest, SseQueryReturnsNextThenComplete) {
    auto events = httpSSE(s_port, s_path, "{ books { id title } }");
    ASSERT_GE(events.size(), 2u) << "Expected at least 'next' and 'complete' events";

    EXPECT_EQ(events.front().event, "next");
    ASSERT_FALSE(events.front().data.contains("errors")) << events.front().data.dump();
    auto books = events.front().data["data"]["books"];
    ASSERT_TRUE(books.is_array());
    EXPECT_GE(books.size(), 2u);

    EXPECT_EQ(events.back().event, "complete");
}

TEST_F(StandaloneServerE2ETest, SseMutationReturnsNextThenComplete) {
    auto events = httpSSE(s_port, s_path,
                          R"(mutation { addBook(title:"SSE Test", author:"Y", year:2025) { title author } })");
    ASSERT_GE(events.size(), 2u);

    EXPECT_EQ(events.front().event, "next");
    ASSERT_FALSE(events.front().data.contains("errors")) << events.front().data.dump();
    auto book = events.front().data["data"]["addBook"];
    EXPECT_EQ(book["title"], "SSE Test");
    EXPECT_EQ(book["author"], "Y");

    EXPECT_EQ(events.back().event, "complete");
}
