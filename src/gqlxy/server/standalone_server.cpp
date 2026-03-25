#include <gqlxy/server/standalone_server.h>

#include "internal/AppComponents.h"

#include <gqlxy/server/internal/GraphQLController.h>
#include <gqlxy/server/internal/GraphQLWSListener.h>
#include <future>
#include <mutex>
#include <oatpp-websocket/ConnectionHandler.hpp>
#include <oatpp/core/base/Environment.hpp>
#include <oatpp/network/Server.hpp>
#include <oatpp/network/tcp/server/ConnectionProvider.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>
#include <oatpp/web/server/HttpConnectionHandler.hpp>
#include <oatpp/web/server/HttpRouter.hpp>
#include <string>

using namespace std;
using namespace gqlxy::server::internal;
using namespace oatpp::web::server;
using namespace oatpp::base;

namespace gqlxy::server {

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

StandaloneServer::StandaloneServer(const StandaloneServerOptions& options) : _options(options) {
    initEnv();
}

StandaloneServer::~StandaloneServer() {
    Stop();
    destroyEnv();
}

void StandaloneServer::Start() {
    OATPP_CREATE_COMPONENT(shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper)(
        oatpp::parser::json::mapping::ObjectMapper::createShared());
    OATPP_CREATE_COMPONENT(shared_ptr<oatpp::websocket::ConnectionHandler>, wsHandler)(
        oatpp::websocket::ConnectionHandler::createShared());
    wsHandler.getObject()->setSocketInstanceListener(make_shared<GraphQLWSInstanceListener>(_options.schema));
    OATPP_CREATE_COMPONENT(Schema, schema)(_options.schema);

    auto graphqlController = make_shared<GraphQLController>(_options.path);

    auto router = HttpRouter::createShared();
    router->addController(graphqlController);

    auto httpConnectionHandler = HttpConnectionHandler::createShared(router);
    auto connectionProvider = oatpp::network::tcp::server::ConnectionProvider::createShared(
        {_options.host, _options.port, oatpp::network::Address::IP_4});

    _server = oatpp::network::Server::createShared(connectionProvider, httpConnectionHandler);
    _running.store(true);
    _server->run([this]() { return _running.load(); });
    connectionProvider->stop();
    httpConnectionHandler->stop();
}

void StandaloneServer::StartAsync() {
    _serverThread = std::async(launch::async, [this] { Start(); });
}

void StandaloneServer::Stop() {
    if (_server != nullptr) {
        _running.store(false);
        _serverThread.wait();
    }
}

string StandaloneServer::GetUrl() const {
    return format("http://{}:{}{}", _options.host, _options.port, _options.path);
}

}
