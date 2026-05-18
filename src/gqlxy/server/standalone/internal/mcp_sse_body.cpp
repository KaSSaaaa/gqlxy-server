#include "mcp_sse_body.h"

#include <gqlxy/core/results.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::server::internal;
using namespace nlohmann;

McpSseBody::McpSseBody(SubscriptionHandle&& handle, const json& rpcId)
    : SseBody(std::move(handle)),
      _rpcId(rpcId) {}

string McpSseBody::FormatEvent(const GraphQLResponse& result) {
    return format("event: message\ndata: {}\n\n", json {
        {"jsonrpc", "2.0"},
        {"id", _rpcId},
        {"result", {
            {"content", json::array({{
                {"type", "text"},
                {"text", Serialize(result).dump()}
            }})}
        }}
    }.dump());
}
