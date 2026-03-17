#include "graphql_http_handler.h"

#include <ariane/internal/utils/ranges.h>
#include <ariane/server/internal/GraphQLSSEBody.h>
#include <ariane/subscription.h>
#include <oatpp-websocket/Handshaker.hpp>
#include <oatpp/encoding/Url.hpp>
#include <oatpp/web/protocol/http/outgoing/BufferBody.hpp>
#include <oatpp/web/protocol/http/outgoing/ResponseFactory.hpp>

using namespace std;
using namespace ariane::graphql::server::internal;
using namespace ariane::graphql;
using namespace ariane::graphql::internal;
using namespace oatpp::web::protocol::http;
using namespace oatpp::websocket;
using namespace nlohmann;
using OutgoingResponse = GraphQLHttpController::OutgoingResponse;

GraphQLHttpController::GraphQLHttpController(const Schema& schema, const shared_ptr<ConnectionHandler>& wsHandler)
    : _schema(schema),
      _wsHandler(wsHandler) {}

shared_ptr<OutgoingResponse> GraphQLHttpController::handle(const shared_ptr<IncomingRequest>& request) {
    auto upgradeHeader = request->getHeader("Upgrade");
    if (upgradeHeader == "websocket") {
        auto subprotocol = request->getHeader("Sec-WebSocket-Protocol");
        if (!subprotocol)
            return jsonResponse(Status::CODE_400, {
                {"errors", {
                    {{"message", "No Sec-WebSocket-Protocol header provided"}}
                }}
            });

        auto elected = Subprotocol(subprotocol.getValue(""));
        if (!elected)
            return jsonResponse(Status::CODE_400, {
                {"errors", {
                    {{"message", "Unsupported WebSocket subprotocol"}}
                }}
            });

        auto response = Handshaker::serversideHandshake(request->getHeaders(), _wsHandler);
        response->putHeader("Sec-WebSocket-Protocol", elected.value());
        return response;
    }

    auto method = request->getStartingLine().method;

    if (method == "OPTIONS") {
        auto resp = outgoing::ResponseFactory::createResponse(Status::CODE_204, "");
        resp->putHeader("Access-Control-Allow-Origin", "*");
        resp->putHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        resp->putHeader("Access-Control-Allow-Headers", "Content-Type, Accept");
        return resp;
    }

    if (method == "POST") return handlePost(request);
    return handleGet(request);
}

static const vector<string> SupportedProtocols = {"graphql-transport-ws", "graphql-ws"};

optional<string> GraphQLHttpController::Subprotocol(const string& headerValue) const {
    auto offered = to_vector(
        headerValue
        | views::split(',')
        | views::transform([](const auto& subprotocol) -> string { return trim(subprotocol); })
    );

    return to_optional(SupportedProtocols, ranges::find_if(SupportedProtocols, [&](const auto& candidate) {
        return ranges::find(offered, candidate) != offered.end();
    }));
}

shared_ptr<OutgoingResponse> GraphQLHttpController::handlePost(const shared_ptr<IncomingRequest>& request) {
    auto bodyStr = request->readBodyToString();
    if (!bodyStr || bodyStr->empty()) {
        return jsonResponse(
            Status::CODE_400,
            {{"errors", {
                {{"message", "Empty request body"}}
            }}}
        );
    }

    json body;
    try {
        body = json::parse(*bodyStr);
    } catch (const json::exception& e) {
        return jsonResponse(
            Status::CODE_400,
            {{"errors", {
                {{"message", format("Invalid JSON: {}", e.what())}}
            }}
        });
    }

    auto query = body.value("query", "");
    auto variables = body.contains("variables") ? body["variables"] : json::object();
    auto operationName = body.value("operationName", "");

    auto acceptHeader = request->getHeader("Accept");
    if (acceptHeader && acceptHeader->find("text/event-stream") != string::npos) {
        return handleSSE(query, variables, operationName);
    }

    return jsonResponse(Status::CODE_200, executeQuery(query, variables, operationName));
}

shared_ptr<OutgoingResponse> GraphQLHttpController::handleGet(const shared_ptr<IncomingRequest>& request) {
    auto queryParam = request->getQueryParameter("query");
    if (!queryParam) {
        return jsonResponse(
            Status::CODE_400,
            {{"errors", {
                {{"message", "Missing 'query' parameter"}}
            }}
        });
    }
    queryParam = oatpp::encoding::Url::decode(queryParam);

    json variables = json::object();
    auto varsParam = request->getQueryParameter("variables");
    if (varsParam) {
        try {
            variables = json::parse(string(varsParam));
        } catch (...) {
        }
    }

    string operationName;
    auto opParam = request->getQueryParameter("operationName");
    if (opParam) operationName = opParam;

    auto acceptHeader = request->getHeader("Accept");
    if (acceptHeader && acceptHeader->find("text/event-stream")) {
        return handleSSE(queryParam, variables, operationName);
    }

    return jsonResponse(Status::CODE_200, executeQuery(queryParam, variables, operationName));
}

shared_ptr<OutgoingResponse> GraphQLHttpController::handleSSE(const string& query,
                                                              const json& variables,
                                                              const string& operationName) {
    SchemaResolveArgs args {
        .query = query,
        .variables = variables,
        .operationName = operationName
    };

    auto handle = IsSubscription(query)
        ? _schema.Subscribe(args)
        : SubscriptionHandle::SingleShot(_schema.Resolve(args).get());

    auto body = make_shared<GraphQLSSEBody>(std::move(handle));
    auto response = outgoing::Response::createShared(Status::CODE_200, body);
    response->putHeader("Access-Control-Allow-Origin", "*");
    return response;
}

json GraphQLHttpController::executeQuery(const string& query,
                                         const json& variables,
                                         const string& operationName) const {
    return Serialize(_schema.Resolve({
        .query = query,
        .variables = variables,
        .operationName = operationName
    }).get());
}

shared_ptr<OutgoingResponse> GraphQLHttpController::jsonResponse(Status status, const json& body) const {
    auto response = ResponseFactory::createResponse(status, body.dump());
    response->putHeader("Content-Type", "application/graphql-response+json");
    response->putHeader("Access-Control-Allow-Origin", "*");
    return response;
}
