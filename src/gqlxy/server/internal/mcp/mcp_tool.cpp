#include <gqlxy/server/mcp/mcp_tool.h>

#include <gqlxy/core/results.h>
#include <gqlxy/server/schema.h>

using namespace std;
using namespace nlohmann;

namespace gqlxy::mcp {

static json ToInputSchema(const vector<McpToolArg>& args) {
    json properties = json::object();
    json required = json::array();

    for (const auto& arg : args) {
        json prop = {{"type", arg.jsonSchemaType}};
        if (arg.description) prop["description"] = *arg.description;
        properties[arg.name] = prop;
        if (arg.required) required.push_back(arg.name);
    }

    json schema = {
        {"type", "object"},
        {"properties", properties}
    };
    if (!required.empty()) schema["required"] = required;
    return schema;
}

json ToJson(const McpTool& tool) {
    json jsonTool = {
        {"name", tool.name},
        {"inputSchema", ToInputSchema(tool.args)}
    };
    if (tool.description) jsonTool["description"] = *tool.description;
    return jsonTool;
}

McpTool CreateGraphQLMcpTool(const CreateGraphQLMcpToolArgs& options) {
    return McpTool {
        .name = options.name,
        .description = options.description,
        .args = options.args,
        .handler = [&schema = options.schema, query = options.query, staticVars = options.variables](const json& callArgs) {
            json variables = staticVars;
            for (const auto& [key, value] : callArgs.items())
                variables[key] = value;
            auto result = schema.Resolve({.query = query, .variables = variables}).get();
            return ToolCallResult {.data = Serialize(result), .isError = false};
        }
    };
}

}
