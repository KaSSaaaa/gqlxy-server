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
    while (_buffer.empty() && !_done) {
        auto result = _handle.Next();
        if (!result.has_value()) {
            _buffer = "event: complete\n\n";
            _done = true;
            break;
        }
        _buffer = "event: next\ndata: " + buildPayload(result.value()).dump() + "\n\n";
    }

    if (_buffer.empty()) return 0;

    auto size = std::min(static_cast<size_t>(count), _buffer.size());
    std::memcpy(buffer, _buffer.data(), size);
    _buffer.erase(0, size);
    return static_cast<v_io_size>(size);
}

p_char8 GraphQLSSEBody::getKnownData() {
    return nullptr;
}

v_int64 GraphQLSSEBody::getKnownSize() {
    return -1;
}

json GraphQLSSEBody::buildPayload(const ResolveResult& result) {
    json payload;
    if (result.data.has_value()) {
        payload["data"] = result.data.value();
    }
    if (result.errors.has_value()) {
        auto err = json::array();
        for (const auto& [message, path, locations] : result.errors.value()) {
            err.push_back({
                {"message", message},
                {"path", path},
                {"location", to_vector(locations | views::transform([](const auto& loc) -> json {
                    return {{"line", loc.line}, {"column", loc.column}};
                }))}
            });
        }
        payload["errors"] = err;
    }
    return payload;
}