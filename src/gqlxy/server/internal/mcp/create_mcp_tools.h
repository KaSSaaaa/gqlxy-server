#pragma once

#include <gqlxy/server/mcp/mcp_policy.h>
#include <gqlxy/server/definitions/schema_definition.h>
#include <gqlxy/server/mcp/mcp_tool.h>
#include <vector>

namespace gqlxy::internal {

std::vector<mcp::McpTool> CreateMcpTools(const SchemaDefinition& schema, DefaultMcpPolicy policy);

}
