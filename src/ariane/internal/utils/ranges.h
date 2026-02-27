#pragma once

#include <map>
#include <ranges>
#include <set>
#include <unordered_map>
#include <vector>

namespace ariane::graphql::internal {

template <std::ranges::input_range R>
auto to_vector(R&& r) {
    using T = std::ranges::range_value_t<R>;
    return std::vector<T>(std::ranges::begin(r), std::ranges::end(r));
}

template <std::ranges::input_range R>
auto to_set(R&& r) {
    using T = std::ranges::range_value_t<R>;
    return std::set<T>(std::ranges::begin(r), std::ranges::end(r));
}

template <std::ranges::input_range R>
auto to_unordered_map(R&& r) {
    using T = std::ranges::range_value_t<R>;
    using TKey = T::first_type;
    using TValue = T::second_type;
    return std::unordered_map<TKey, TValue>(std::ranges::begin(r), std::ranges::end(r));
}

template <std::ranges::input_range R>
auto to_map(R&& r) {
    using T = std::ranges::range_value_t<R>;
    using TKey = T::first_type;
    using TValue = T::second_type;
    return std::map<TKey, TValue>(std::ranges::begin(r), std::ranges::end(r));
}

}