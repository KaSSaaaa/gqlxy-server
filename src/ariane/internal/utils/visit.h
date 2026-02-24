#pragma once

namespace ariane::graphql::internal {

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

}