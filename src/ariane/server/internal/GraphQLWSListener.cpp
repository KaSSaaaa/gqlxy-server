#include "GraphQLWSListener.h"

#include <ariane/internal/utils/optional.h>
#include <future>

using namespace std;
using namespace ariane::graphql::server::internal;
using namespace ariane::graphql::internal;
using namespace oatpp::websocket;

GraphQLWSListener::GraphQLWSListener(const Schema& schema) : _schema(schema) {}

void GraphQLWSListener::onPing(const WebSocket& socket, const oatpp::String& message) {
    socket.sendPong(message);
}

void GraphQLWSListener::onPong(const WebSocket&, const oatpp::String&) {}

void GraphQLWSListener::onClose(const WebSocket&, v_uint16, const oatpp::String&) {
    cancelAllSubscriptions();
}

void GraphQLWSListener::readMessage(const WebSocket& socket,
                                    v_uint8 opcode,
                                    p_char8 data,
                                    oatpp::v_io_size size) {
    if (size == 0) {
        auto text = _messageBuffer.toString();
        _messageBuffer.setCurrentPosition(0);
        if (opcode == 1 && text) {
            dispatch(socket, text.getValue(""));
        }
    } else {
        _messageBuffer.writeSimple(data, size);
    }
}

void GraphQLWSListener::dispatch(const WebSocket& socket, const string& text) {
    nlohmann::json msg;
    try {
        msg = nlohmann::json::parse(text);
    } catch (...) {
        return;
    }

    auto type = msg.value("type", string{});

    if (type == "connection_init") {
        handleConnectionInit(socket, msg);
    } else if (type == "subscribe") {
        _protocol = Protocol::TransportWS;
        handleSubscribe(socket, msg.value("id", string{}),
                        msg.value("payload", nlohmann::json::object()));
    } else if (type == "start") {
        _protocol = Protocol::LegacyWS;
        handleStart(socket, msg.value("id", string{}),
                    msg.value("payload", nlohmann::json::object()));
    } else if (type == "complete" || type == "stop") {
        handleComplete(msg.value("id", string{}));
    } else if (type == "ping") {
        sendText(socket, {{"type", "pong"}});
    } else if (type == "connection_terminate") {
        cancelAllSubscriptions();
    }
}

void GraphQLWSListener::handleConnectionInit(const WebSocket& socket, const nlohmann::json&) {
    if (_initialized) {
        sendText(socket, {{"type", "connection_error"},
                          {"payload", {{"message", "Too many initialisation requests"}}}});
        return;
    }
    _initialized = true;
    sendText(socket, {{"type", "connection_ack"}});
}

void GraphQLWSListener::handleSubscribe(const WebSocket& socket,
                                         const string& id,
                                         const nlohmann::json& payload) {
    if (id.empty()) return;

    auto query = payload.value("query", string{});
    auto variables = payload.contains("variables") ? payload["variables"] : nlohmann::json::object();
    auto operationName = payload.value("operationName", string{});

    auto handle = make_shared<SubscriptionHandle>(
        _schema.Subscribe({.query = query, .variables = variables, .operationName = operationName}));

    auto future = async(launch::async, [this, id, handle, &socket]() mutable {
        while (auto result = handle->Next()) {
            if (!result.has_value()) break;
            sendText(socket, {
                {"id", id},
                {"type", "next"},
                {"payload", Serialize(result.value())}
            });
        }
        sendText(socket, {
            {"id", id},
            {"type", "complete"}
        });
    });

    _subscriptions[id] = {std::move(handle), std::move(future)};
}

void GraphQLWSListener::handleStart(const WebSocket& socket,
                                     const string& id,
                                     const nlohmann::json& payload) {
    if (id.empty()) return;

    auto query = payload.value("query", string{});
    auto variables = payload.contains("variables") ? payload["variables"] : nlohmann::json::object();
    auto operationName = payload.value("operationName", string{});

    auto handle = make_shared<SubscriptionHandle>(
        _schema.Subscribe({.query = query, .variables = variables, .operationName = operationName}));

    auto future = async(launch::async, [this, id, handle, &socket]() mutable {
        while (auto result = handle->Next()) {
            if (!result.has_value()) break;
            sendText(socket, {
                {"id", id},
                {"type", "data"},
                {"payload", Serialize(result.value())}
            });
        }
        sendText(socket, {
            {"id", id},
            {"type", "complete"}
        });
    });

    _subscriptions[id] = {std::move(handle), std::move(future)};
}

void GraphQLWSListener::handleComplete(const string& id) {
    auto it = _subscriptions.find(id);
    if (it == _subscriptions.end()) return;
    it->second.handle->Cancel();
    _subscriptions.erase(it);
}

void GraphQLWSListener::cancelAllSubscriptions() {
    for (auto& sub : _subscriptions | views::values) {
        sub.handle->Cancel();
    }
    _subscriptions.clear();
}

void GraphQLWSListener::shutdownAndWait() {
    cancelAllSubscriptions();
}

void GraphQLWSListener::sendText(const WebSocket& socket, const nlohmann::json& msg) const {
    lock_guard<mutex> lock(_sendMutex);
    socket.sendOneFrameText(msg.dump());
}

GraphQLWSInstanceListener::GraphQLWSInstanceListener(const Schema& schema)
    : _schema(schema) {

}

void GraphQLWSInstanceListener::onAfterCreate(const WebSocket& socket, const shared_ptr<const ParameterMap>&) {
    socket.setListener(make_shared<GraphQLWSListener>(_schema));
}

void GraphQLWSInstanceListener::onBeforeDestroy(const WebSocket& socket) {
    auto listener = std::dynamic_pointer_cast<GraphQLWSListener>(socket.getListener());
    if (listener) listener->shutdownAndWait();
}
