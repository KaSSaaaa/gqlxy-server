#pragma once

#include <gqlxy/server/mcp/mcp_tool.h>
#include <gqlxy/server/subscription.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace gqlxy::mcp {

class McpToolRegistry {
public:
    explicit McpToolRegistry(const std::vector<McpTool>& tools);

    bool IsEmpty() const;
    std::vector<McpTool> Tools() const;
    std::optional<SubscriptionHandle> Call(const std::string& toolName, const nlohmann::json& args) const;

private:
    std::vector<McpTool> _tools;
};

}
