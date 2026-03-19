#pragma once

#include <ariane/schema.h>
#include <memory>
#include <string>

namespace oatpp::network {
class Server;
}

namespace ariane::graphql::server {

struct StandaloneServerOptions {
    Schema& schema;
    std::string host = "0.0.0.0";
    uint16_t port = 4000;
    std::string path = "/graphql";
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
    std::shared_ptr<oatpp::network::Server> _server;
    std::future<void> _serverThread;
    std::atomic_bool _running;
};

}
