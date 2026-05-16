#include "sse_body.h"

using namespace std;
using namespace gqlxy::server::internal;
using namespace oatpp;
using namespace oatpp::web::protocol::http;
using namespace oatpp::async;

void SseBody::declareHeaders(Headers& headers) noexcept {
    headers.put("Content-Type", "text/event-stream");
    headers.put("Cache-Control", "no-cache");
    headers.put("X-Accel-Buffering", "no");
    headers.put(Header::CONNECTION, "keep-alive");
}

v_io_size SseBody::read(void* buffer, v_buff_size count, Action&) {
    auto frame = ReadHandle();
    if (frame.empty()) return 0;
    return Send(buffer, count, frame);
}

string SseBody::ReadHandle() {
    if (_done) return "";
    auto result = _handle.Next();
    if (!result.has_value()) {
        _done = true;
        return FormatDone();
    }
    return FormatEvent(*result);
}

v_io_size SseBody::Send(void* buffer, v_buff_size count, const string& value) {
    auto size = min(static_cast<v_io_size>(count), static_cast<v_io_size>(value.size()));
    memcpy(buffer, value.data(), size);
    return size;
}

p_char8 SseBody::getKnownData() {
    return nullptr;
}

v_int64 SseBody::getKnownSize() {
    return -1;
}