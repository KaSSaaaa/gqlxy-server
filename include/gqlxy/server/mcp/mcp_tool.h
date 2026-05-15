#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace gqlxy {
class Schema;
}

namespace gqlxy::mcp {

struct McpToolArg {
    std::string name;
    std::optional<std::string> description;
    std::string jsonSchemaType;
    bool required = false;
};

struct ToolCallResult {
    nlohmann::json data = {};
    bool isError = false;
};

struct McpTool {
    std::string name;
    std::optional<std::string> description;
    std::vector<McpToolArg> args = {};
    std::function<ToolCallResult(const nlohmann::json& args)> handler;
};

struct CreateGraphQLMcpToolArgs {
    Schema& schema;
    std::string name;
    std::optional<std::string> description = std::nullopt;
    std::vector<McpToolArg> args = {};
    std::string query;
    nlohmann::json variables = nlohmann::json::object();
};

McpTool CreateGraphQLMcpTool(const CreateGraphQLMcpToolArgs& options);
nlohmann::json ToJson(const McpTool& tool);

}
