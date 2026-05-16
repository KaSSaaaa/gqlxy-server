#pragma once

#include "sse_body.h"

#include <nlohmann/json.hpp>

namespace gqlxy::server::internal {

class McpSseBody : public SseBody {
public:
    McpSseBody(SubscriptionHandle&& handle, const nlohmann::json& rpcId);

protected:
    std::string FormatEvent(const GraphQLResponse& result) override;

private:
    nlohmann::json _rpcId;
};

}
