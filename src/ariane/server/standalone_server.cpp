#include <ariane/server/standalone_server.h>

#include <ariane/server/internal/GraphQLWSListener.h>
#include <ariane/server/internal/graphql_http_handler.h>
#include <oatpp-websocket/ConnectionHandler.hpp>
#include <oatpp/core/base/Environment.hpp>
#include <oatpp/network/Server.hpp>
#include <oatpp/network/tcp/server/ConnectionProvider.hpp>
#include <oatpp/web/server/HttpConnectionHandler.hpp>
#include <oatpp/web/server/HttpRouter.hpp>
#include <future>
#include <mutex>
#include <string>

using namespace std;
using namespace oatpp::web::server;
using namespace oatpp::base;

namespace ariane::graphql::server {

static mutex g_envMutex;
static int g_envRefCount = 0;

static void initEnv() {
    lock_guard lock(g_envMutex);
    if (g_envRefCount++ == 0) Environment::init();
}

static void destroyEnv() {
    lock_guard lock(g_envMutex);
    if (--g_envRefCount == 0) Environment::destroy();
}

struct StandaloneServer::Impl {
    StandaloneServerOptions options;
    shared_ptr<oatpp::network::Server> server;
    future<void> serverThread;

    explicit Impl(StandaloneServerOptions opts) : options(std::move(opts)) {}

    void run() {
        auto wsHandler = oatpp::websocket::ConnectionHandler::createShared();
        wsHandler->setSocketInstanceListener(
            make_shared<internal::GraphQLWSInstanceListener>(options.schema));

        auto httpHandler = make_shared<internal::GraphQLHttpController>(options.schema, wsHandler);

        auto router = HttpRouter::createShared();
        router->route("GET",     options.path.c_str(), httpHandler);
        router->route("POST",    options.path.c_str(), httpHandler);
        router->route("OPTIONS", options.path.c_str(), httpHandler);

        auto httpConnectionHandler = HttpConnectionHandler::createShared(router);
        auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared(
            {options.host.c_str(), options.port, oatpp::network::Address::IP_4});

        server = oatpp::network::Server::createShared(connectionProvider, httpConnectionHandler);
        server->run();
    }
};

StandaloneServer::StandaloneServer(StandaloneServerOptions options) {
    initEnv();
    _impl = make_unique<Impl>(std::move(options));
}

StandaloneServer::~StandaloneServer() {
    Stop();
    destroyEnv();
}

void StandaloneServer::Start() {
    _impl->run();
}

void StandaloneServer::StartAsync() {
    _impl->serverThread = std::async(launch::async, [this] { _impl->run(); });
}

void StandaloneServer::Stop() {
    if (_impl->server) {
        _impl->server->stop();
    }
}

string StandaloneServer::GetUrl() const {
    return "http://" + _impl->options.host + ":" + to_string(_impl->options.port) +
           _impl->options.path;
}

}
