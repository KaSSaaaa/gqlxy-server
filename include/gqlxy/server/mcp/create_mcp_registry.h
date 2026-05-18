#pragma once

#include <gqlxy/server/mcp/mcp_policy.h>
#include <memory>

namespace gqlxy {
class Schema;
}

namespace gqlxy::mcp {
class McpToolRegistry;

std::shared_ptr<McpToolRegistry> CreateMcpRegistry(Schema& schema, DefaultMcpPolicy policy);

}
