#pragma once

#include "sse_body.h"

namespace gqlxy::server::internal {

class GraphQLSSEBody : public SseBody {
public:
    explicit GraphQLSSEBody(SubscriptionHandle&& handle);

protected:
    std::string FormatEvent(const GraphQLResponse& result) override;
    std::string FormatDone() override;
};

}
