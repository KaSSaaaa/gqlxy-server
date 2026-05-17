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
    if (_bufferOffset >= _buffer.size()) {
        _buffer = ReadHandle();
        _bufferOffset = 0;
    }
    if (_buffer.empty()) return 0;
    return Send(buffer, count);
}

v_io_size SseBody::Send(void* buffer, v_buff_size count) {
    auto sent = min(static_cast<v_io_size>(count), static_cast<v_io_size>(_buffer.size() - _bufferOffset));
    memcpy(buffer, _buffer.data() + _bufferOffset, sent);
    _bufferOffset += sent;
    return sent;
}

string SseBody::ReadHandle() {
    string buffer;
    while (buffer.empty() && !_done) {
        auto result = _handle.Next();
        if (!result.has_value()) {
            buffer = FormatDone();
            _done = true;
        } else {
            buffer = FormatEvent(*result);
        }
    }
    return buffer;
}

string SseBody::FormatDone() {
    return "";
}

p_char8 SseBody::getKnownData() {
    return nullptr;
}

v_int64 SseBody::getKnownSize() {
    return -1;
}