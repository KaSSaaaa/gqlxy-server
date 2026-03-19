#include "GraphQLSSEBody.h"

#include <ariane/internal/utils/ranges.h>

#include <nlohmann/json.hpp>

using namespace std;
using namespace ariane::graphql::server::internal;
using namespace ariane::graphql::internal;
using namespace oatpp;
using namespace oatpp::web;
using namespace oatpp::async;
using namespace nlohmann;

GraphQLSSEBody::GraphQLSSEBody(SubscriptionHandle&& handle) : _handle(std::move(handle)) {}

void GraphQLSSEBody::declareHeaders(protocol::http::Headers& headers) noexcept {
    headers.put("Content-Type", "text/event-stream");
    headers.put("Cache-Control", "no-cache");
    headers.put("X-Accel-Buffering", "no");
    headers.put(protocol::http::Header::CONNECTION, "keep-alive");
}

v_io_size GraphQLSSEBody::read(void* buffer, v_buff_size count, Action&) {
    auto result = ReadHandle();
    if (result.empty()) return 0;
    return Send(buffer, count, result);
}

string GraphQLSSEBody::ReadHandle() {
    string buffer;
    while (buffer.empty() && !_done) {
        auto result = _handle.Next();
        if (!result.has_value()) {
            buffer = "event: complete\n\n";
            _done = true;
            break;
        }
        buffer = format("event: next\n"
                        "data: {}\n\n", Serialize(result.value()).dump());
    }
    return buffer;
}

v_io_size GraphQLSSEBody::Send(void* buffer, v_buff_size count, const std::string& value) {
    auto size = min(static_cast<v_io_size>(count), static_cast<v_io_size>(value.size()));
    memcpy(buffer, value.data(), size);
    return size;
}

p_char8 GraphQLSSEBody::getKnownData() {
    return nullptr;
}

v_int64 GraphQLSSEBody::getKnownSize() {
    return -1;
}