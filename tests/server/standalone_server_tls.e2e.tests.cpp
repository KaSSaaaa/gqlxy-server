#include <gqlxy/ResolverArgs.h>
#include <gqlxy/resolvers.h>
#include <gqlxy/schema.h>
#include <gqlxy/server/standalone_server.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>

#include <filesystem>
#include <fstream>
#include <future>
#include <thread>

using namespace std;
using namespace std::filesystem;
using namespace gqlxy;
using namespace gqlxy::server;
using json = nlohmann::json;

namespace beast = boost::beast;
namespace http = beast::http;
namespace ws = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

static constexpr int TlsPort = 14002;
static constexpr auto TlsPath = "/graphql";
static constexpr auto CertPem = R"(-----BEGIN CERTIFICATE-----
MIIDlTCCAn2gAwIBAgIUOLxr3q7Wd/pto1+2MsW4fdRheCIwDQYJKoZIhvcNAQEL
BQAwWjELMAkGA1UEBhMCVVMxCzAJBgNVBAgMAkNBMRQwEgYDVQQHDAtMb3MgQW5n
ZWxlczEOMAwGA1UECgwFQmVhc3QxGDAWBgNVBAMMD3d3dy5leGFtcGxlLmNvbTAe
Fw0yMTA3MDYwMTQ5MjVaFw00ODExMjEwMTQ5MjVaMFoxCzAJBgNVBAYTAlVTMQsw
CQYDVQQIDAJDQTEUMBIGA1UEBwwLTG9zIEFuZ2VsZXMxDjAMBgNVBAoMBUJlYXN0
MRgwFgYDVQQDDA93d3cuZXhhbXBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IB
DwAwggEKAoIBAQCz0GwgnxSBhygxBdhTHGx5LDLIJSuIDJ6nMwZFvAjdhLnB/vOT
Lppr5MKxqQHEpYdyDYGD1noBoz4TiIRj5JapChMgx58NLq5QyXkHV/ONT7yi8x05
P41c2F9pBEnUwUxIUG1Cb6AN0cZWF/wSMOZ0w3DoBhnl1sdQfQiS25MTK6x4tATm
Wm9SJc2lsjWptbyIN6hFXLYPXTwnYzCLvv1EK6Ft7tMPc/FcJpd/wYHgl8shDmY7
rV+AiGTxUU35V0AzpJlmvct5aJV/5vSRRLwT9qLZSddE9zy/0rovC5GML6S7BUC4
lIzJ8yxzOzSStBPxvdrOobSSNlRZIlE7gnyNAgMBAAGjUzBRMB0GA1UdDgQWBBR+
dYtY9zmFSw9GYpEXC1iJKHC0/jAfBgNVHSMEGDAWgBR+dYtY9zmFSw9GYpEXC1iJ
KHC0/jAPBgNVHRMBAf8EBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBzKrsiYywl
RKeB2LbddgSf7ahiQMXCZpAjZeJikIoEmx+AmjQk1bam+M7WfpRAMnCKooU+Utp5
TwtijjnJydkZHFR6UH6oCWm8RsUVxruao/B0UFRlD8q+ZxGd4fGTdLg/ztmA+9oC
EmrcQNdz/KIxJj/fRB3j9GM4lkdaIju47V998Z619E/6pt7GWcAySm1faPB0X4fL
FJ6iYR2r/kJLoppPqL0EE49uwyYQ1dKhXS2hk+IIfA9mBn8eAFb/0435A2fXutds
qhvwIOmAObCzcoKkz3sChbk4ToUTqbC0TmFAXI5Upz1wnADzjpbJrpegCA3pmvhT
7356drqnCGY9
-----END CERTIFICATE-----)";

static constexpr auto KeyPem = R"(-----BEGIN PRIVATE KEY-----
MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCz0GwgnxSBhygx
BdhTHGx5LDLIJSuIDJ6nMwZFvAjdhLnB/vOTLppr5MKxqQHEpYdyDYGD1noBoz4T
iIRj5JapChMgx58NLq5QyXkHV/ONT7yi8x05P41c2F9pBEnUwUxIUG1Cb6AN0cZW
F/wSMOZ0w3DoBhnl1sdQfQiS25MTK6x4tATmWm9SJc2lsjWptbyIN6hFXLYPXTwn
YzCLvv1EK6Ft7tMPc/FcJpd/wYHgl8shDmY7rV+AiGTxUU35V0AzpJlmvct5aJV/
5vSRRLwT9qLZSddE9zy/0rovC5GML6S7BUC4lIzJ8yxzOzSStBPxvdrOobSSNlRZ
IlE7gnyNAgMBAAECggEAY0RorQmldGx9D7M+XYOPjsWLs1px0cXFwGA20kCgVEp1
kleBeHt93JqJsTKwOzN2tswl9/ZrnIPWPUpcbBlB40ggjzQk5k4jBY50Nk2jsxuV
9A9qzrP7AoqhAYTQjZe42SMtbkPZhEeOyvCqxBAi6csLhcv4eB4+In0kQo7dfvLs
Xu/3WhSsuAWqdD9EGnhD3n+hVTtgiasRe9318/3R9DzP+IokoQGOtXm+1dsfP0mV
8XGzQHBpUtJNn0yi6SC4kGEQuKkX33zORlSnZgT5VBLofNgra0THd7x3atOx1lbr
V0QizvCdBa6j6FwhOQwW8UwgOCnUbWXl/Xn4OaofMQKBgQDdRXSMyys7qUMe4SYM
Mdawj+rjv0Hg98/xORuXKEISh2snJGKEwV7L0vCn468n+sM19z62Axz+lvOUH8Qr
hLkBNqJvtIP+b0ljRjem78K4a4qIqUlpejpRLw6a/+44L76pMJXrYg3zdBfwzfwu
b9NXdwHzWoNuj4v36teGP6xOUwKBgQDQCT52XX96NseNC6HeK5BgWYYjjxmhksHi
stjzPJKySWXZqJpHfXI8qpOd0Sd1FHB+q1s3hand9c+Rxs762OXlqA9Q4i+4qEYZ
qhyRkTsl+2BhgzxmoqGd5gsVT7KV8XqtuHWLmetNEi+7+mGSFf2iNFnonKlvT1JX
4OQZC7ntnwKBgH/ORFmmaFxXkfteFLnqd5UYK5ZMvGKTALrWP4d5q2BEc7HyJC2F
+5lDR9nRezRedS7QlppPBgpPanXeO1LfoHSA+CYJYEwwP3Vl83Mq/Y/EHgp9rXeN
L+4AfjEtLo2pljjnZVDGHETIg6OFdunjkXDtvmSvnUbZBwG11bMnSAEdAoGBAKFw
qwJb6FNFM3JnNoQctnuuvYPWxwM1yjRMqkOIHCczAlD4oFEeLoqZrNhpuP8Ij4wd
GjpqBbpzyVLNP043B6FC3C/edz4Lh+resjDczVPaUZ8aosLbLiREoxE0udfWf2dU
oBNnrMwwcs6jrRga7Kr1iVgUSwBQRAxiP2CYUv7tAoGBAKdPdekPNP/rCnHkKIkj
o13pr+LJ8t+15vVzZNHwPHUWiYXFhG8Ivx7rqLQSPGcuPhNss3bg1RJiZAUvF6fd
e6QS4EZM9dhhlO2FmPQCJMrRVDXaV+9TcJZXCbclQnzzBus9pwZZyw4Anxo0vmir
nOMOU6XI4lO9Xge/QDEN4Y2R
-----END PRIVATE KEY-----)";

static ssl::context CreateTlsClientCtx() {
    ssl::context ctx {ssl::context::tls_client};
    ctx.set_verify_mode(ssl::verify_none);
    return ctx;
}

static json httpsPost(const string& query, const json& variables = json::object()) {
    net::io_context ioc;
    auto ctx = CreateTlsClientCtx();
    beast::ssl_stream<beast::tcp_stream> stream {ioc, ctx};

    beast::get_lowest_layer(stream).expires_after(chrono::seconds(5));
    beast::get_lowest_layer(stream).connect(tcp::resolver(ioc).resolve("127.0.0.1", to_string(TlsPort)));
    stream.handshake(ssl::stream_base::client);

    json payload = {
        {"query", query}
    };
    if (!variables.empty()) payload["variables"] = variables;

    http::request<http::string_body> req {http::verb::post, TlsPath, 11};
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
    stream.shutdown(ec);

    return json::parse(res.body());
}

class WssClient {
    net::io_context _ioc;
    ssl::context _ctx;
    ws::stream<beast::ssl_stream<beast::tcp_stream>> _ws;

  public:
    WssClient()
        : _ctx(ssl::context::tls_client),
          _ws(_ioc, _ctx) {
        _ctx.set_verify_mode(ssl::verify_none);

        beast::get_lowest_layer(_ws).expires_after(chrono::seconds(5));
        beast::get_lowest_layer(_ws).connect(tcp::resolver(_ioc).resolve("127.0.0.1", to_string(TlsPort)));
        beast::get_lowest_layer(_ws).expires_never();

        _ws.next_layer().handshake(ssl::stream_base::client);

        _ws.set_option(ws::stream_base::decorator([&](ws::request_type& req) {
            req.set(http::field::sec_websocket_protocol, "graphql-transport-ws");
        }));
        _ws.handshake("localhost", TlsPath);
    }

    ~WssClient() {
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

static shared_ptr<Schema> TlsSchema = nullptr;
static shared_ptr<StandaloneServer> TlsServer = nullptr;
static string CertPath;
static string KeyPath;

static void writeTempFile(const string& path, const char* content) {
    ofstream f(path);
    f << content;
}

static void waitForTlsPort() {
    for (int i = 0; i < 50; ++i) {
        try {
            net::io_context ioc;
            auto ctx = CreateTlsClientCtx();
            beast::ssl_stream<beast::tcp_stream> stream {ioc, ctx};
            beast::get_lowest_layer(stream).expires_after(chrono::milliseconds(200));
            beast::get_lowest_layer(stream).connect(tcp::resolver(ioc).resolve("127.0.0.1", to_string(TlsPort)));
            stream.handshake(ssl::stream_base::client);
            return;
        } catch (...) {
            this_thread::sleep_for(chrono::milliseconds(20));
        }
    }
}

class StandaloneServerTlsE2ETest : public testing::Test {
  public:
    static void SetUpTestSuite() {
        const auto tmp = temp_directory_path();
        CertPath = (tmp / "gqlxy_test.cert.pem").string();
        KeyPath = (tmp / "gqlxy_test.key.pem").string();
        writeTempFile(CertPath, CertPem);
        writeTempFile(KeyPath, KeyPem);

        TlsSchema = make_shared<Schema>(SchemaOptions{
            .typeDefs = R"(
                type Query  { hello: String, echo(msg: String!): String }
                type Mutation { greet(name: String!): String }
                type Subscription { ticks: Int }
            )",
            .resolvers = {
                {"Query", Resolver {
                    {"hello", "world"},
                    {"echo", FunctionResolver {[](const ResolverArgs& r) -> ValueResolver {
                        return r.Args()["msg"].get<string>();
                    }}}
                }},
                {"Mutation", Resolver {
                    {"greet", FunctionResolver {[](const ResolverArgs& r) -> ValueResolver {
                        return "Hello, " + r.Args()["name"].get<string>() + "!";
                    }}}
                }}
            }
        });

        TlsServer = make_shared<StandaloneServer>(StandaloneServerOptions{
            .schema = *TlsSchema,
            .port = TlsPort,
            .tls = TlsOptions {
                .certPath = CertPath,
                .keyPath = KeyPath
            }
        });
        TlsServer->StartAsync();
        waitForTlsPort();
    }

    static void TearDownTestSuite() {
        TlsServer->Stop();
        TlsServer.reset();
        TlsSchema.reset();
        filesystem::remove(CertPath);
        filesystem::remove(KeyPath);
    }
};

TEST_F(StandaloneServerTlsE2ETest, GetUrlReturnsHttpsScheme) {
    EXPECT_EQ(TlsServer->GetUrl(), "https://0.0.0.0:14002/graphql");
}

TEST_F(StandaloneServerTlsE2ETest, HttpsQueryHelloReturnsWorld) {
    auto res = httpsPost("{ hello }");
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    EXPECT_EQ(res["data"]["hello"], "world");
}

TEST_F(StandaloneServerTlsE2ETest, HttpsQueryEchoReturnsArgument) {
    auto res = httpsPost("query($m: String!) { echo(msg: $m) }", {{"m", "ping"}});
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    EXPECT_EQ(res["data"]["echo"], "ping");
}

TEST_F(StandaloneServerTlsE2ETest, HttpsMutationGreetReturnsGreeting) {
    auto res = httpsPost(R"(mutation { greet(name: "TLS") })");
    ASSERT_FALSE(res.contains("errors")) << res.dump();
    EXPECT_EQ(res["data"]["greet"], "Hello, TLS!");
}

TEST_F(StandaloneServerTlsE2ETest, WssConnectionAckReceived) {
    WssClient wss;
    wss.send({{"type", "connection_init"}});
    auto ack = wss.recv();
    EXPECT_EQ(ack["type"], "connection_ack") << ack.dump();
}

TEST_F(StandaloneServerTlsE2ETest, WssQueryOverSecureConnection) {
    WssClient wss;

    wss.send({{"type", "connection_init"}});
    ASSERT_EQ(wss.recv()["type"], "connection_ack");

    wss.send({
        {"type", "subscribe"},
        {"id", "1"},
        {"payload", {
            {"query", "{ hello }"}
        }}
    });

    auto msg = wss.recv();
    ASSERT_EQ(msg["type"], "next") << msg.dump();
    EXPECT_EQ(msg["payload"]["data"]["hello"], "world");
}
