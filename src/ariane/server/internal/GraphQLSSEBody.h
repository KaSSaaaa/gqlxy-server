#pragma once

#include <ariane/subscription.h>
#include <nlohmann/json_fwd.hpp>
#include <oatpp/web/protocol/http/outgoing/Body.hpp>

namespace ariane::graphql::server::internal {

class GraphQLSSEBody : public oatpp::web::protocol::http::outgoing::Body {
public:
    GraphQLSSEBody(SubscriptionHandle&& handle);

    void declareHeaders(oatpp::web::protocol::http::Headers& headers) noexcept override;
    oatpp::v_io_size read(void* buffer, v_buff_size count, oatpp::async::Action& action) override;
    p_char8 getKnownData() override;
    v_int64 getKnownSize() override;


private:
    SubscriptionHandle _handle;
    std::string _buffer;
    bool _done = false;

    static nlohmann::json buildPayload(const ResolveResult& result);

};

}
