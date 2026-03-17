#pragma once

#include <ariane/schema.h>
#include <ariane/subscription.h>
#include <nlohmann/json.hpp>
#include <oatpp-websocket/ConnectionHandler.hpp>
#include <oatpp-websocket/WebSocket.hpp>
#include <oatpp/core/data/stream/BufferStream.hpp>

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace ariane::graphql::server::internal {

class GraphQLWSListener : public oatpp::websocket::WebSocket::Listener {
public:
    explicit GraphQLWSListener(const Schema& schema);

    void onPing(const WebSocket& socket, const oatpp::String& message) override;
    void onPong(const WebSocket& socket, const oatpp::String& message) override;
    void onClose(const WebSocket& socket, v_uint16 code, const oatpp::String& message) override;
    void readMessage(const WebSocket& socket,
                     v_uint8 opcode,
                     p_char8 data,
                     oatpp::v_io_size size) override;

    void cancelAllSubscriptions();
    void shutdownAndWait();

private:
    enum class Protocol { Unknown, TransportWS, LegacyWS };

    const Schema& _schema;
    oatpp::data::stream::BufferOutputStream _messageBuffer;
    mutable std::mutex _sendMutex;
    Protocol _protocol = Protocol::Unknown;
    bool _initialized = false;

    struct ActiveSubscription {
        std::shared_ptr<SubscriptionHandle> handle;
        std::future<void> thread;
    };
    std::unordered_map<std::string, ActiveSubscription> _subscriptions;

    void dispatch(const WebSocket& socket, const std::string& text);
    void handleConnectionInit(const WebSocket& socket, const nlohmann::json& msg);
    void handleSubscribe(const WebSocket& socket, const std::string& id, const nlohmann::json& payload);
    void handleStart(const WebSocket& socket, const std::string& id, const nlohmann::json& payload);
    void handleComplete(const std::string& id);

    void sendText(const WebSocket& socket, const nlohmann::json& msg) const;
};

class GraphQLWSInstanceListener : public oatpp::websocket::ConnectionHandler::SocketInstanceListener {
public:
    explicit GraphQLWSInstanceListener(const Schema& schema);
    virtual ~GraphQLWSInstanceListener() = default;

    void onAfterCreate(const WebSocket& socket,
                       const std::shared_ptr<const ParameterMap>& params) override;

    void onBeforeDestroy(const WebSocket& socket) override;

private:
    const Schema& _schema;
};

}
