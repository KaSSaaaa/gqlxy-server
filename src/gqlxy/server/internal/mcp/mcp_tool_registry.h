#pragma once

#include <gqlxy/server/mcp/mcp_tool.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace gqlxy {
class Schema;
}

namespace gqlxy::mcp {

struct ToolCallResult {
    nlohmann::json data = {};
    bool isError = false;
};

class McpToolRegistry {
public:
    McpToolRegistry(Schema& schema, const std::vector<McpTool>& tools);

    bool IsEmpty() const;
    std::vector<McpTool> Tools() const;
    ToolCallResult Call(const std::string& toolName, const nlohmann::json& args) const;

private:
    Schema& _schema;
    std::vector<McpTool> _tools;
};

}
