#pragma once

#include <gqlxy/schema.h>
#include <gqlxy/subscription.h>
#include <nlohmann/json.hpp>
#include <oatpp-websocket/ConnectionHandler.hpp>
#include <oatpp-websocket/WebSocket.hpp>
#include <oatpp/core/data/stream/BufferStream.hpp>

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace gqlxy::server::internal {

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
    const Schema& _schema;
    oatpp::data::stream::BufferOutputStream _messageBuffer;
    std::mutex _sendMutex;

    struct ActiveSubscription {
        std::shared_ptr<SubscriptionHandle> handle;
        std::future<void> thread;
    };
    std::unordered_map<std::string, ActiveSubscription> _subscriptions;

    void Handle(const WebSocket& socket, const std::string& text);
    void HandleConnectionInit(const WebSocket& socket, const nlohmann::json& msg);
    void HandleSubscribe(const WebSocket& socket, const std::string& id, const nlohmann::json& payload);
    void HandleStart(const WebSocket& socket, const std::string& id, const nlohmann::json& payload);
    void StartSubscription(const WebSocket& socket, const std::string& id, const nlohmann::json& payload, const std::string& type);
    void StartHandle(SubscriptionHandle&& h, const WebSocket& socket, const std::string& id, const std::string& type);
    void HandleComplete(const std::string& id);

    void sendText(const WebSocket& socket, const nlohmann::json& msg);
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
