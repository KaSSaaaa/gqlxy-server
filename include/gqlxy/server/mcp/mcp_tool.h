#pragma once

#include <functional>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace gqlxy {
class Schema;
class SubscriptionHandle;
}

namespace gqlxy::mcp {

struct McpToolArg {
    std::string name;
    std::optional<std::string> description;
    std::string jsonSchemaType;
    std::optional<std::string> jsonSchemaItemType;
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
    std::function<SubscriptionHandle(const nlohmann::json& args)> handler;
};

struct CreateGraphQLMcpToolArgs {
    std::string name;
    std::optional<std::string> description = std::nullopt;
    std::vector<McpToolArg> args = {};
    std::function<SubscriptionHandle(const nlohmann::json& args)> handler;
};

nlohmann::json ToJson(const McpTool& tool);

}
