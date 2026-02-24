#pragma once

namespace ariane::graphql::internal {

template <std::ranges::input_range R>
auto to_vector(R&& r)
{
    using T = std::ranges::range_value_t<R>;
    return std::vector<T>(std::ranges::begin(r), std::ranges::end(r));
}

}