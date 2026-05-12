#include "mcp_controller.h"

#include <nlohmann/json.hpp>
#include <oatpp/parser/json/mapping/ObjectMapper.hpp>

using namespace std;
using namespace gqlxy::mcp;
using namespace nlohmann;
using namespace oatpp;
using namespace oatpp::web::server;
using namespace oatpp::web::protocol::http;
using namespace oatpp::web::protocol::http::outgoing;

static constexpr auto McpVersion = "2025-11-25";

namespace gqlxy::server::internal {

McpController::McpController(
    const string& path, const shared_ptr<ObjectMapper>& objectMapper, McpToolRegistry& registry)
    : ApiController(objectMapper),
      _path(path),
      _registry(registry) {}

shared_ptr<Response> McpController::createJsonResponse(const Status& status, const json& response) {
    return createResponse(status, response.dump());
}

shared_ptr<Response> McpController::McpPost(const shared_ptr<IncomingRequest>& request) {
    json rpc;
    try {
        rpc = json::parse(request->readBodyToString().getValue(""));
    } catch (...) {
        return createJsonResponse(Status::CODE_400, {{"error", "invalid json"}});
    }

    auto dispatched = Dispatch(rpc);
    if (dispatched.is_null()) {
        auto res = createResponse(Status::CODE_202, "");
        res->putHeader("Access-Control-Allow-Origin", "*");
        return res;
    }
    auto res = createJsonResponse(Status::CODE_200, dispatched);
    res->putHeader("Content-Type", "application/json");
    res->putHeader("Access-Control-Allow-Origin", "*");
    return res;
}

json McpController::Dispatch(const json& rpc) {
    auto id = rpc.value("id", json {});
    auto method = rpc.value("method", "");
    auto params = rpc.value("params", json::object());

    if (method == "initialize") return OkResponse(id, HandleInitialize(params));
    if (method == "notifications/initialized") return json {};
    if (method == "tools/list") return OkResponse(id, HandleToolsList());
    if (method == "tools/call") return OkResponse(id, HandleToolsCall(params));

    return ErrorResponse(id, -32601, "Method not found: " + method);
}

json McpController::HandleInitialize(const json&) {
    return {
        {"protocolVersion", McpVersion},
        {"capabilities", {
            {"tools", json::object()}
        }},
        {"serverInfo", {
            {"name", "gqlxy-mcp"},
            {"version", "1.0.0"}
        }}
    };
}

json McpController::HandleToolsList() {
    json tools = json::array();
    for (const auto& tool : _registry.Tools())
        tools.push_back(ToJson(tool));
    return {
        {"tools", tools},
    };
}

json McpController::HandleToolsCall(const json& params) {
    auto toolName = params.value("name", "");
    auto toolArgs = params.value("arguments", json::object());

    auto result = _registry.Call(toolName, toolArgs);
    if (result.isError)
        return {
            {"isError", true},
            {"content", json::array({
                {
                    {"type", "text"},
                    {"text", format("Tool call failed: {}", toolName)}
                }
            })}
        };

    return {
        {"content", json::array({
            {
                {"type", "text"},
                {"text", result.data.dump()}
            }
        })}
    };
}

json McpController::OkResponse(const json& id, const json& result) {
    return McpResponse(id,
        {"result", result}
    );
}

json McpController::ErrorResponse(const json& id, int code, const string& message) {
    return McpResponse(id,
        {"error", {
            {"code", code},
            {"message", message}
        }}
    );
}

json McpController::McpResponse(const json& id, const pair<string, json>& data) {
    return {
        {"jsonrpc", "2.0"},
        {"id", id},
        data
    };
}
}
