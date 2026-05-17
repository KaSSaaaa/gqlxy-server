#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <filesystem>
#include <fstream>
#include <future>
#include <gqlxy/client/client.h>
#include <gqlxy/client/links/http_link.h>
#include <gqlxy/client/links/ws_link.h>
#include <gqlxy/core/results.h>
#include <gqlxy/server/resolver_args.h>
#include <gqlxy/server/resolvers.h>
#include <gqlxy/server/schema.h>
#include <gqlxy/server/standalone/standalone_server.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <thread>

using namespace std;
using namespace std::chrono;
using namespace gqlxy;
using namespace gqlxy::server;
using namespace nlohmann;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::beast;

static constexpr int TlsPort = 14002;

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

class StandaloneServerTlsE2ETest : public testing::Test {
  protected:
    void SetUp() override {
        const auto tmp = filesystem::temp_directory_path();
        _certPath = (tmp / "gqlxy_test.cert.pem").string();
        _keyPath = (tmp / "gqlxy_test.key.pem").string();
        writeTempFile(_certPath, CertPem);
        writeTempFile(_keyPath, KeyPem);

        _tlsSchema = make_shared<Schema>(SchemaOptions{
            .typeDefs = R"(
                type Query  { hello: String, echo(msg: String!): String }
                type Mutation { greet(name: String!): String }
                type Subscription { ticks: Int }
            )",
            .resolvers = {
                {"Query", Resolver{
                    {"hello", "world"},
                    {"echo", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                        return r.Args()["msg"].get<string>();
                    }}}
                }},
                {"Mutation", Resolver{
                    {"greet", FunctionResolver{[](const ResolverArgs& r) -> ValueResolver {
                        return "Hello, " + r.Args()["name"].get<string>() + "!";
                    }}}
                }}
            }
        });

        _tlsServer = make_shared<StandaloneServer>(StandaloneServerOptions {
            .schema = *_tlsSchema,
            .port = TlsPort,
            .tls = TlsOptions {
                .certPath = _certPath,
                .keyPath = _keyPath
            }
        });
        _tlsServer->StartAsync();
        waitForTlsPort();
    }

    void TearDown() override {
        _tlsServer->Stop();
        _tlsServer.reset();
        _tlsSchema.reset();
        filesystem::remove(_certPath);
        filesystem::remove(_keyPath);
    }

    shared_ptr<Schema> _tlsSchema;
    shared_ptr<StandaloneServer> _tlsServer;
    string _certPath;
    string _keyPath;

    static string HttpsUrl() {
        return format("https://127.0.0.1:{}/graphql", TlsPort);
    }

    static string WssUrl() {
        return format("wss://127.0.0.1:{}/graphql", TlsPort);
    }

    static Client MakeHttpsClient() {
        return Client({
            .link = make_shared<HttpLink>(HttpLinkOptions {
                .url = HttpsUrl(),
                .caCert = CertPem,
            })
        });
    }

    static Client MakeWssClient() {
        return Client({
            .link = make_shared<WsLink>(WsLinkOptions {
                .url = WssUrl(),
                .caCert = CertPem,
            })
        });
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

    static void writeTempFile(const string& path, const char* content) {
        ofstream f(path);
        f << content;
    }

    static void waitForTlsPort() {
        for (int i = 0; i < 50; ++i) {
            try {
                io_context ioc;
                ssl::context ctx {ssl::context::tls_client};
                ctx.set_verify_mode(ssl::verify_none);
                ssl_stream<tcp_stream> stream {ioc, ctx};
                get_lowest_layer(stream).expires_after(milliseconds(200));
                get_lowest_layer(stream).connect(tcp::resolver(ioc).resolve("127.0.0.1", to_string(TlsPort)));
                stream.handshake(ssl::stream_base::client);
                return;
            } catch (...) {
                this_thread::sleep_for(milliseconds(20));
            }
        }
    }
};

TEST_F(StandaloneServerTlsE2ETest, GetUrlReturnsHttpsScheme) {
    EXPECT_EQ(_tlsServer->GetUrl(), "https://0.0.0.0:14002/graphql");
}

TEST_F(StandaloneServerTlsE2ETest, HttpsQueryHelloReturnsWorld) {
    auto client = MakeHttpsClient();
    auto res = await(client.Query({.query = "{ hello }"}));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    EXPECT_EQ(res.data.value()["hello"], "world");
}

TEST_F(StandaloneServerTlsE2ETest, HttpsQueryEchoReturnsArgument) {
    auto client = MakeHttpsClient();
    auto res = await(client.Query({
        .query = "query($m: String!) { echo(msg: $m) }",
        .variables = {{"m", "ping"}}
    }));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    EXPECT_EQ(res.data.value()["echo"], "ping");
}

TEST_F(StandaloneServerTlsE2ETest, HttpsMutationGreetReturnsGreeting) {
    auto client = MakeHttpsClient();
    auto res = await(client.Mutation({.query = R"(mutation { greet(name: "TLS") })"}));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    EXPECT_EQ(res.data.value()["greet"], "Hello, TLS!");
}

TEST_F(StandaloneServerTlsE2ETest, WssConnectionAckReceived) {
    auto client = MakeWssClient();
    auto res = await(client.Query({.query = "{ hello }"}));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    EXPECT_EQ(res.data.value()["hello"], "world");
}

TEST_F(StandaloneServerTlsE2ETest, WssQueryOverSecureConnection) {
    auto client = MakeWssClient();
    auto res = await(client.Query({.query = "{ hello }"}));
    ASSERT_FALSE(res.errors) << res.errors->front().message;
    EXPECT_EQ(res.data.value()["hello"], "world");
}
