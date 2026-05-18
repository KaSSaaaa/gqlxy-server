#include <gqlxy/server/mcp/mcp_tool.h>

#include <gqlxy/core/results.h>

using namespace std;
using namespace nlohmann;

namespace gqlxy::mcp {

static json ToInputSchema(const vector<McpToolArg>& args) {
    json properties = json::object();
    json required = json::array();

    for (const auto& arg : args) {
        json prop = {{"type", arg.jsonSchemaType}};
        if (arg.description) prop["description"] = *arg.description;
        if (arg.jsonSchemaItemType) prop["items"] = {{"type", *arg.jsonSchemaItemType}};
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

}
