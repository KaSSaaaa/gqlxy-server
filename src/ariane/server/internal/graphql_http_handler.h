#pragma once

#include <ariane/schema.h>
#include <nlohmann/json.hpp>
#include <oatpp-websocket/ConnectionHandler.hpp>
#include <oatpp/web/server/HttpRequestHandler.hpp>

namespace ariane::graphql::server::internal {

class GraphQLHttpController : public oatpp::web::server::HttpRequestHandler {
public:
    GraphQLHttpController(const Schema& schema,
                       const std::shared_ptr<oatpp::websocket::ConnectionHandler>& wsHandler);

    std::shared_ptr<OutgoingResponse> handle(const std::shared_ptr<IncomingRequest>& request) override;

private:
    const Schema& _schema;
    std::shared_ptr<oatpp::websocket::ConnectionHandler> _wsHandler;

    std::shared_ptr<OutgoingResponse> handlePost(const std::shared_ptr<IncomingRequest>& request);
    std::shared_ptr<OutgoingResponse> handleGet(const std::shared_ptr<IncomingRequest>& request);
    std::shared_ptr<OutgoingResponse> handleSSE(const std::string& query,
                                                  const nlohmann::json& variables,
                                                  const std::string& operationName);

    nlohmann::json executeQuery(const std::string& query,
                                const nlohmann::json& variables,
                                const std::string& operationName) const;

    std::shared_ptr<OutgoingResponse> jsonResponse(oatpp::web::protocol::http::Status status,
                                                    const nlohmann::json& body) const;

    std::optional<std::string> Subprotocol(const std::string& headerValue) const;
};

}
