#pragma once

#include <oatpp/core/macro/codegen.hpp>
#include <oatpp/core/Types.hpp>

#include OATPP_CODEGEN_BEGIN(DTO)

namespace gqlxy::server::internal {

class RequestBody : public oatpp::DTO {
public:
    DTO_INIT(RequestBody, DTO);

    DTO_FIELD(String, query);
    DTO_FIELD(Fields<Any>, variables);
    DTO_FIELD(String, operationName);
};

}

#include OATPP_CODEGEN_END(DTO)
