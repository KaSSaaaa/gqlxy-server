#include "graphql_sse_body.h"

#include <gqlxy/core/results.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::server::internal;

GraphQLSSEBody::GraphQLSSEBody(SubscriptionHandle&& handle) : SseBody(std::move(handle)) {}

string GraphQLSSEBody::FormatEvent(const GraphQLResponse& result) {
    return format("event: next\ndata: {}\n\n", Serialize(result).dump());
}

string GraphQLSSEBody::FormatDone() {
    return "event: complete\n\n";
}
