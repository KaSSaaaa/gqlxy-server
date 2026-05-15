#include <gqlxy/server/standalone/standalone_server.h>

#include <future>
#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/server/internal/mcp/create_mcp_tools.h>
#include <gqlxy/server/standalone/internal/graphql_controller.h>
#include <gqlxy/server/standalone/internal/graphql_ws_listener.h>
#include <gqlxy/server/standalone/internal/mcp_controller.h>
#include <mutex>
#include <oatpp-openssl/Config.hpp>
#include <oatpp-openssl/server/ConnectionProvider.hpp>
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
using namespace gqlxy::internal;
using namespace gqlxy::mcp;
using namespace gqlxy::utils;
using namespace oatpp::web::server;
using namespace oatpp::base;
using namespace oatpp::network;
using namespace oatpp::openssl;

namespace gqlxy::server {

static mutex EnvMutex;
static int EnvRefCount = 0;

static void initEnv() {
    lock_guard lock(EnvMutex);
    if (EnvRefCount++ == 0) Environment::init();
}

static void destroyEnv() {
    lock_guard lock(EnvMutex);
    if (--EnvRefCount == 0) Environment::destroy();
}

static shared_ptr<ServerConnectionProvider> CreateConnectionProvider(const StandaloneServerOptions& options) {
    Address address {options.host, options.port, Address::IP_4};
    if (options.tls) {
        return oatpp::openssl::server::ConnectionProvider::createShared(
            Config::createDefaultServerConfigShared(options.tls->certPath, options.tls->keyPath), address);
    }
    return tcp::server::ConnectionProvider::createShared(address);
}

StandaloneServer::StandaloneServer(const StandaloneServerOptions& options) : _options(options) {
    initEnv();
    if (options.mcp) {
        _mcpRegistry = make_unique<McpToolRegistry>(concat(
            CreateMcpTools(options.schema, options.mcp->policy),
            options.mcp->additionalTools));
    }
}

StandaloneServer::~StandaloneServer() {
    Stop();
    destroyEnv();
}

void StandaloneServer::Start() {
    auto objectMapper = oatpp::parser::json::mapping::ObjectMapper::createShared();
    auto wsHandler = oatpp::websocket::ConnectionHandler::createShared();
    wsHandler->setSocketInstanceListener(make_shared<GraphQLWSInstanceListener>(_options.schema));

    auto router = HttpRouter::createShared();
    router->addController(make_shared<GraphQLController>(_options.path, objectMapper, wsHandler, _options.schema));

    if (_mcpRegistry != nullptr && !_mcpRegistry->IsEmpty())
        router->addController(make_shared<McpController>(_options.mcp->path, objectMapper, *_mcpRegistry));

    auto httpConnectionHandler = HttpConnectionHandler::createShared(router);
    auto connectionProvider = CreateConnectionProvider(_options);

    _server = Server::createShared(connectionProvider, httpConnectionHandler);
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
    return format("{}://{}:{}{}", _options.tls ? "https" : "http", _options.host, _options.port, _options.path);
}

}
