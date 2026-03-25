#pragma once

namespace gqlxy::internal {

template <typename ... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

}