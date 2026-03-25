#include "GraphQLController.h"

#include <gqlxy/internal/utils/optional.h>
#include <gqlxy/internal/utils/ranges.h>
#include <gqlxy/server/internal/GraphQLSSEBody.h>
#include <oatpp-websocket/Handshaker.hpp>
#include <oatpp/encoding/Url.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>

using namespace std;
using namespace gqlxy::server::internal;
using namespace gqlxy::internal;
using namespace oatpp;
using namespace oatpp::web::server;
using namespace oatpp::web::protocol::http;
using namespace oatpp::websocket;
using namespace oatpp::encoding;
using namespace oatpp::web::protocol::http::outgoing;
using namespace oatpp::data::stream;
using namespace oatpp::parser::json::mapping;
using namespace nlohmann;

GraphQLController::GraphQLController(const string& path,
                                     shared_ptr<ObjectMapper>& objectMapper,
                                     shared_ptr<ConnectionHandler>& wsHandler,
                                     Schema& schema)
    : ApiController(objectMapper),
      _path(path),
      _wsHandler(wsHandler),
      _schema(schema) {

}

shared_ptr<Response> GraphQLController::GQLGet(const shared_ptr<IncomingRequest>& request,
                                               const String& encodedQuery,
                                               const String& variables,
                                               const String& operationName) {
    return HandleRequest(request->getHeaders(), Url::decode(encodedQuery), variables, operationName);
}

shared_ptr<Response> GraphQLController::GQLPost(const shared_ptr<IncomingRequest>& request,
                                                const Object<RequestBody>& requestBody) {
    return HandleRequest(request->getHeaders(),
                         requestBody->query,
                         requestBody->variables,
                         requestBody->operationName);
}

json GraphQLController::Convert(const Any& variables) {
    return json::parse(::ObjectMapper().writeToString(variables).getValue("{}"));
}

shared_ptr<Response> GraphQLController::HandleRequest(const Headers& headers,
                                                      const String& query,
                                                      const Any& variables,
                                                      const String& operationName) {
    auto upgradeHeader = headers.get("Upgrade");
    if (upgradeHeader && upgradeHeader == "websocket") {
        auto subprotocol = Subprotocol(headers.get("Sec-WebSocket-Protocol"));
        if (!subprotocol.has_value())
            return createResponse(Status::CODE_400, json {
                {"errors", {
                    {{"message", "Unsupported WebSocket subprotocol"}}
                }}
            }.dump());
        auto response = Handshaker::serversideHandshake(headers, _wsHandler);
        response->putHeader("Sec-WebSocket-Protocol", subprotocol.value());
        return response;
    }

    if (!query || query->empty()) {
        return createResponse(
            Status::CODE_400,
            String(json {
                {"errors", {
                    {{"message", "Missing 'query' parameter"}}
                }
            }}.dump()));
    }

    auto acceptHeader = headers.get("Accept");
    if (acceptHeader && acceptHeader->find("text/event-stream") != string::npos) {
        return HandleSSE(query, variables, operationName);
    }

    return createResponse(Status::CODE_200, Serialize(_schema.Resolve({
        .query = query.getValue(""),
        .variables = Convert(variables),
        .operationName = operationName.getValue("")
    }).get()).dump());
}

static const vector<string> SupportedProtocols = {"graphql-transport-ws", "graphql-ws"};

optional<string> GraphQLController::Subprotocol(const string& headerValue) {
    auto offered = to_vector(headerValue
        | views::split(',')
        | views::transform([](const auto& subprotocol) -> string {
              return trim(subprotocol);
          }));

    return find_optional(SupportedProtocols, [&](const auto& candidate) {
        return ranges::find(offered, candidate) != offered.end();
    });
}

shared_ptr<Response> GraphQLController::HandleSSE(const String& query,
                                                  const Any& variables,
                                                  const String& operationName) {
    SchemaResolveArgs args {
        .query = query,
        .variables = Convert(variables),
        .operationName = operationName.getValue("")
    };

    auto handle = IsSubscription(query)
        ? _schema.Subscribe(args)
        : SubscriptionHandle::SingleShot(_schema.Resolve(args).get());

    auto body = make_shared<GraphQLSSEBody>(std::move(handle));
    auto response = Response::createShared(Status::CODE_200, body);
    response->putHeader("Access-Control-Allow-Origin", "*");
    return response;
}
