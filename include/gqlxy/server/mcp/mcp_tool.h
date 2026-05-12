#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace gqlxy::mcp {

struct McpToolArg {
    std::string name;
    std::optional<std::string> description;
    std::string jsonSchemaType;
    bool required = false;
};

struct McpTool {
    std::string name;
    std::optional<std::string> description;
    std::vector<McpToolArg> args = {};
};

nlohmann::json ToJson(const McpTool& tool);

}
