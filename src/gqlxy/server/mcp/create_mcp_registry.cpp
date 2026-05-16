#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/server/internal/mcp/create_mcp_tools.h>
#include <gqlxy/server/internal/mcp/mcp_tool_registry.h>
#include <gqlxy/server/mcp/create_mcp_registry.h>

using namespace std;
using namespace gqlxy::utils;

namespace gqlxy::mcp {

shared_ptr<McpToolRegistry> CreateMcpRegistry(Schema& schema, DefaultMcpPolicy policy) {
    return make_shared<McpToolRegistry>(internal::CreateMcpTools(schema, policy));
}

}
