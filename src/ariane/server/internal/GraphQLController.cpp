#include "GraphQLController.h"

#include <ariane/server/internal/GraphQLSSEBody.h>
#include <oatpp-websocket/Handshaker.hpp>
#include <oatpp/encoding/Url.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>

using namespace std;
using namespace ariane::graphql::server::internal;
using namespace oatpp;
using namespace oatpp::web::server;
using namespace oatpp::web::protocol::http;
using namespace oatpp::websocket;
using namespace oatpp::encoding;
using namespace oatpp::web::protocol::http::outgoing;
using namespace oatpp::data::stream;
using namespace oatpp::parser::json::mapping;
using namespace nlohmann;

GraphQLController::GraphQLController(shared_ptr<ObjectMapper>& objectMapper,
                                     shared_ptr<ConnectionHandler>& wsHandler,
                                     Schema& schema)
    : ApiController(objectMapper),
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
        auto response = Handshaker::serversideHandshake(headers, _wsHandler);
        response->putHeader("Sec-WebSocket-Protocol", "graphql-transport-ws");
        //TODO handle ws subprotocol pick
        return response;
    }

    auto acceptHeader = headers.get("Accept");
    if (acceptHeader && acceptHeader->find("text/event-stream") != string::npos) {
        return HandleSSE(query, variables, operationName);
    }

    auto result = _schema.Resolve({
        .query = query.getValue(""),
        .variables = Convert(variables),
        .operationName = operationName.getValue("")
    }).get();

    json response;
    if (result.data.has_value()) {
        response["data"] = result.data.value();
    }
    if (result.errors.has_value()) {
        auto errors = json::array();
        for (const auto& e : result.errors.value()) {
            errors.push_back({{"message", e.message}});
        }
        response["errors"] = errors;
    }

    return createResponse(Status::CODE_200, response.dump());
}

shared_ptr<Response> GraphQLController::HandleSSE(const String& query,
                                                  const Any& variables,
                                                  const String& operationName) {
    auto handle = _schema.Subscribe({
        .query = query,
        .variables = Convert(variables),
        .operationName = operationName.getValue("")
    });

    auto response = Response::createShared(Status::CODE_200, make_shared<GraphQLSSEBody>(std::move(handle)));
    response->putHeader("Access-Control-Allow-Origin", "*");
    return response;
}
