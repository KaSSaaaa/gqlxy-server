#pragma once

#include <gqlxy/core/results.h>
#include <gqlxy/server/subscription.h>
#include <oatpp/web/protocol/http/outgoing/Body.hpp>

namespace gqlxy::server::internal {

class SseBody : public oatpp::web::protocol::http::outgoing::Body {
public:
    explicit SseBody(SubscriptionHandle&& handle) : _handle(std::move(handle)) {}

    void declareHeaders(oatpp::web::protocol::http::Headers& headers) noexcept override;
    oatpp::v_io_size Send(void* buffer, v_buff_size count);

    oatpp::v_io_size read(void* buffer, v_buff_size count, oatpp::async::Action&) override;

    p_char8 getKnownData() override;
    v_int64 getKnownSize() override;

protected:
    SubscriptionHandle _handle;
    bool _done = false;

    std::string ReadHandle();

    virtual std::string FormatEvent(const GraphQLResponse& result) = 0;
    virtual std::string FormatDone();

private:
    std::string _buffer;
    std::size_t _bufferOffset = 0;
};

}
