#pragma once

#include <gqlxy/server/mcp/mcp_policy.h>
#include <gqlxy/server/mcp/mcp_tool.h>
#include <gqlxy/server/schema.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gqlxy::mcp {
class McpToolRegistry;
}

namespace oatpp::network {
class Server;
}

namespace gqlxy::server {

struct TlsOptions {
    std::string certPath;
    std::string keyPath;
};

struct McpServerOptions {
    std::string path;
    DefaultMcpPolicy policy = DefaultMcpPolicy::Disabled;
    std::vector<mcp::McpTool> additionalTools = {};
};

struct StandaloneServerOptions {
    Schema& schema;
    std::string host = "0.0.0.0";
    uint16_t port = 4000;
    std::string path = "/graphql";
    std::optional<TlsOptions> tls = std::nullopt;
    std::optional<McpServerOptions> mcp = std::nullopt;
};

class StandaloneServer {
public:
    explicit StandaloneServer(const StandaloneServerOptions& options);
    ~StandaloneServer();

    void Start();
    void StartAsync();

    void Stop();

    std::string GetUrl() const;

private:
    StandaloneServerOptions _options;
    std::unique_ptr<mcp::McpToolRegistry> _mcpRegistry;
    std::shared_ptr<oatpp::network::Server> _server;
    std::future<void> _serverThread;
    std::atomic_bool _running;
};

}
