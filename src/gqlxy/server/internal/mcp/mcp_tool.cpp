#include <gqlxy/server/mcp/mcp_tool.h>

using namespace nlohmann;

namespace gqlxy::mcp {

json ToInputSchema(const McpTool& tool) {
    json properties = json::object();
    json required = json::array();

    for (const auto& arg : tool.args) {
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
        {"inputSchema", ToInputSchema(tool)}
    };
    if (tool.description) jsonTool["description"] = *tool.description;
    return jsonTool;
}

}
