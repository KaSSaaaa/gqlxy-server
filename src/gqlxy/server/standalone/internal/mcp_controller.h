#pragma once

#include <gqlxy/server/internal/mcp/mcp_tool_registry.h>
#include <nlohmann/json.hpp>
#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/web/server/api/ApiController.hpp>

#include OATPP_CODEGEN_BEGIN(ApiController)

#include <oatpp/codegen/api_controller/base_define.hpp>

namespace gqlxy::server::internal {

class McpController : public oatpp::web::server::api::ApiController {
public:
    explicit McpController(
        const std::string& path, const std::shared_ptr<ObjectMapper>& objectMapper, mcp::McpToolRegistry& registry);

    ENDPOINT_INFO(McpPost) {
        info->addConsumes<String>("application/json");
        info->addResponse<String>(Status::CODE_200, "application/json");
    }
    ENDPOINT("POST", String(_path), McpPost, REQUEST(std::shared_ptr<IncomingRequest>, request));

    ENDPOINT_INFO(McpOptions) {
        info->addResponse(Status::CODE_204);
    }
    ENDPOINT("OPTIONS", String(_path), McpOptions) {
        auto response = createResponse(Status::CODE_204);
        response->putHeader("Access-Control-Allow-Origin", "*");
        response->putHeader("Access-Control-Allow-Methods", "POST, OPTIONS");
        response->putHeader("Access-Control-Allow-Headers", "Content-Type, Accept, Mcp-Session-Id");
        return response;
    }

private:
    std::string _path;
    mcp::McpToolRegistry& _registry;

    std::shared_ptr<oatpp::web::protocol::http::outgoing::Response>
    createJsonResponse(const Status& status, const nlohmann::json& response);

    nlohmann::json Dispatch(const nlohmann::json& rpc);
    nlohmann::json HandleInitialize(const nlohmann::json& params);
    nlohmann::json HandleToolsList();
    nlohmann::json HandleToolsCall(const nlohmann::json& params);

    static nlohmann::json OkResponse(const nlohmann::json& id, const nlohmann::json& result);
    static nlohmann::json ErrorResponse(const nlohmann::json& id, int code, const std::string& message);
    static nlohmann::json McpResponse(const nlohmann::json& id, const std::pair<std::string, nlohmann::json>& data);
};

}

#include OATPP_CODEGEN_END(ApiController)
