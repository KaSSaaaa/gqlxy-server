#pragma once

#include <gqlxy/server/mcp/mcp_policy.h>
#include <gqlxy/server/definitions/schema_definition.h>
#include <gqlxy/server/mcp/mcp_tool.h>
#include <vector>

namespace gqlxy {
class Schema;
}

namespace gqlxy::internal {

std::vector<mcp::McpTool> CreateMcpTools(Schema& schema, DefaultMcpPolicy policy);

}
