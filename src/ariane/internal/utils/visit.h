#pragma once

namespace ariane::graphql::internal {

template <typename ... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

}