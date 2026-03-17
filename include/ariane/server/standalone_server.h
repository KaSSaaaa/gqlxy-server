#pragma once

#include <ariane/schema.h>
#include <cstdint>
#include <memory>
#include <string>

namespace ariane::graphql::server {

struct StandaloneServerOptions {
    Schema& schema;
    std::string host = "0.0.0.0";
    uint16_t port = 4000;
    std::string path = "/graphql";
};

class StandaloneServer {
public:
    explicit StandaloneServer(StandaloneServerOptions options);
    ~StandaloneServer();

    StandaloneServer(const StandaloneServer&) = delete;
    StandaloneServer& operator=(const StandaloneServer&) = delete;
    StandaloneServer(StandaloneServer&&) = delete;
    StandaloneServer& operator=(StandaloneServer&&) = delete;

    // Blocks the calling thread until Stop() is called from another thread.
    void Start();

    // Starts the server on a background thread and returns immediately.
    void StartAsync();

    void Stop();

    std::string GetUrl() const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}
