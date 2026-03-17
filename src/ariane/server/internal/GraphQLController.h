#pragma once

#include "RequestBody.h"
#include <ariane/schema.h>
#include <oatpp-websocket/ConnectionHandler.hpp>
#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/core/macro/component.hpp>
#include <oatpp/web/server/api/ApiController.hpp>

#include OATPP_CODEGEN_BEGIN(ApiController)

#include <oatpp/codegen/api_controller/base_define.hpp>

namespace ariane::graphql::server::internal {

class GraphQLController : public oatpp::web::server::api::ApiController {
public:
    explicit GraphQLController(
        OATPP_COMPONENT(std::shared_ptr<ObjectMapper>, objectMapper),
        OATPP_COMPONENT(std::shared_ptr<oatpp::websocket::ConnectionHandler>, wsHandler),
        OATPP_COMPONENT(Schema, schema));

    ENDPOINT_INFO(GQLGet) {
        info->addConsumes<Object<RequestBody>>("application/json");
        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("GET", "/api/graphql", GQLGet, REQUEST(std::shared_ptr<IncomingRequest>, request),
            QUERY(String, encodedQuery, "query", ""),
            QUERY(String, variables, "variables", ""),
            QUERY(String, operationName, "operationName", ""));

    ENDPOINT_INFO(GQLPost) {
        info->addConsumes<Object<RequestBody>>("application/json");
        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("POST", "/api/graphql", GQLPost, REQUEST(std::shared_ptr<IncomingRequest>, request),
             BODY_DTO(Object<RequestBody>, requestBody));

    ENDPOINT_INFO(GQLOptions) {
        info->addResponse(Status::CODE_204);
    }
    ENDPOINT("OPTIONS", "/api/graphql", GQLOptions) {
        auto response = createResponse(Status::CODE_204);
        response->putHeader("Access-Control-Allow-Origin", "*");
        response->putHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        response->putHeader("Access-Control-Allow-Headers", "Content-Type, Accept");
        return response;
    }

private:
    using __ControllerType = GraphQLController;
    std::shared_ptr<oatpp::websocket::ConnectionHandler> _wsHandler;
    Schema& _schema;

    nlohmann::json Convert(const oatpp::Any& variables);

    std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> HandleRequest(
        const oatpp::web::protocol::http::Headers& headers,
        const String& query,
        const oatpp::Any& variables,
        const String& operationName);

    std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> HandleSSE(
        const String& query,
        const oatpp::Any& variables,
        const String& operationName);
};

}

#include OATPP_CODEGEN_END(ApiController)