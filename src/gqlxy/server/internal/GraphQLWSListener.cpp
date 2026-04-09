#include "GraphQLWSListener.h"

#include <gqlxy/internal/utils/optional.h>
#include <future>

using namespace std;
using namespace gqlxy::server::internal;
using namespace gqlxy::internal;
using namespace oatpp::websocket;
using namespace nlohmann;

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
        string text = _messageBuffer.toString();
        _messageBuffer.setCurrentPosition(0);
        Handle(socket, text);
    } else {
        _messageBuffer.writeSimple(data, size);
    }
}

void GraphQLWSListener::Handle(const WebSocket& socket, const string& text) {
    auto msg = json::parse(text);

    auto type = msg["type"];

    if (type == "connection_init") HandleConnectionInit(socket, msg);
    else if (type == "subscribe") HandleSubscribe(socket, msg["id"], msg["payload"]);
    else if (type == "start") HandleStart(socket, msg["id"], msg["payload"]);
    else if (type == "complete" || type == "stop") HandleComplete(msg["id"]);
    else if (type == "ping")
        sendText(socket, {
            {"type", "pong"}
        });
    else if (type == "connection_terminate") cancelAllSubscriptions();
}

void GraphQLWSListener::HandleConnectionInit(const WebSocket& socket, const json&) {
    sendText(socket, {
        {"type", "connection_ack"}
    });
}

void GraphQLWSListener::StartSubscription(const WebSocket& socket,
                                          const string& id,
                                          const json& payload,
                                          const string& type) {
    if (id.empty()) return;

    auto query = payload.value("query", "");
    SchemaResolveArgs args = {
        .query = query,
        .variables = payload.contains("variables") ? payload["variables"] : json::object(),
        .operationName = payload.value("operationName", "")
    };

    auto handle = make_shared<SubscriptionHandle>(IsSubscription(query)
        ? _schema.Subscribe(args)
        : SubscriptionHandle::SingleShot(_schema.Resolve(args).get()));

    auto future = async(launch::async, [this, id, handle, &socket, type]() {
        while (auto result = handle->Next()) {
            if (!result.has_value()) break;
            sendText(socket, {
                {"id", id},
                {"type", type},
                {"payload", Serialize(result.value())}
            });
        }
        sendText(socket, {
            {"id", id},
            {"type", "complete"}
        });
    });

    _subscriptions[id] = ActiveSubscription {
        std::move(handle), std::move(future)
    };
}

void GraphQLWSListener::HandleSubscribe(const WebSocket& socket, const string& id, const json& payload) {
    StartSubscription(socket, id, payload, "next");
}

void GraphQLWSListener::HandleStart(const WebSocket& socket, const string& id, const json& payload) {
    StartSubscription(socket, id, payload, "data");
}

void GraphQLWSListener::HandleComplete(const string& id) {
    auto it = _subscriptions.find(id);
    if (it == _subscriptions.end())
        return;
    it->second.handle->Cancel();
    _subscriptions.erase(it);
}

void GraphQLWSListener::cancelAllSubscriptions() {
    for (const auto& [handle, _] : _subscriptions | views::values)
        handle->Cancel();
    _subscriptions.clear();
}

void GraphQLWSListener::shutdownAndWait() {
    cancelAllSubscriptions();
}

void GraphQLWSListener::sendText(const WebSocket& socket, const json& msg) {
    unique_lock lock(_sendMutex);
    socket.sendOneFrameText(msg.dump());
}

GraphQLWSInstanceListener::GraphQLWSInstanceListener(const Schema& schema)
    : _schema(schema) {

}

void GraphQLWSInstanceListener::onAfterCreate(const WebSocket& socket, const shared_ptr<const ParameterMap>&) {
    socket.setListener(make_shared<GraphQLWSListener>(_schema));
}

void GraphQLWSInstanceListener::onBeforeDestroy(const WebSocket& socket) {
    if (auto listener = dynamic_pointer_cast<GraphQLWSListener>(socket.getListener()))
        listener->shutdownAndWait();
}
