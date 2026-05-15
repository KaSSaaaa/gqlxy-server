#include "mcp_tool_registry.h"

using namespace std;
using namespace nlohmann;

namespace gqlxy::mcp {

McpToolRegistry::McpToolRegistry(const vector<McpTool>& tools)
    : _tools(tools) {}

bool McpToolRegistry::IsEmpty() const {
    return _tools.empty();
}

vector<McpTool> McpToolRegistry::Tools() const {
    return _tools;
}

ToolCallResult McpToolRegistry::Call(const string& toolName, const json& args) const {
    auto it = ranges::find_if(_tools, [&](const auto& t) { return t.name == toolName; });
    if (it == _tools.end()) return {.isError = true};
    return it->handler(args);
}

}
