#pragma once

#include <algorithm>
#include <map>
#include <ranges>
#include <set>
#include <unordered_map>
#include <unordered_set>
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
auto to_unordered_set(R&& r) {
    using T = std::ranges::range_value_t<R>;
    return std::unordered_set<T>(std::ranges::begin(r), std::ranges::end(r));
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

template <std::ranges::input_range R>
auto to_string(R&& r) {
    return std::string(std::ranges::begin(r), std::ranges::end(r));
}

template <std::ranges::input_range R>
auto trim(R&& s) {
    static auto isSpace = [](auto c) { return std::isspace(c); };
    auto r = s
        | std::views::drop_while(isSpace)
        | std::views::reverse
        | std::views::drop_while(isSpace)
        | std::views::reverse;
    return std::string(r.begin(), r.end());
}

template <std::ranges::range R>
auto to_optional(R&& r, std::ranges::iterator_t<R> it)
    -> std::optional<std::ranges::range_value_t<R>>
{
    if (it == std::ranges::end(r)) return std::nullopt;
    return *it;
}

template <std::ranges::range R, typename Pred>
auto find_optional(R&& r, Pred&& pred)
    -> std::optional<std::ranges::range_value_t<R>>
{
    return to_optional(r, std::ranges::find_if(r, std::forward<Pred>(pred)));
}

template <typename T>
concept associative_range = std::ranges::input_range<T>
    && requires { typename std::remove_cvref_t<T>::mapped_type; };

template <std::ranges::input_range First, std::ranges::input_range... Rest>
    requires (!associative_range<First>)
          && (std::convertible_to<std::ranges::range_value_t<Rest>, std::ranges::range_value_t<First>> && ...)
auto concat(First&& first, Rest&&... rest) {
    using T = std::ranges::range_value_t<First>;
    std::vector<T> result;
    std::ranges::copy(first, std::inserter(result, std::ranges::end(result)));
    (std::ranges::copy(rest, std::inserter(result, std::ranges::end(result))), ...);
    return result;
}

template <associative_range First, associative_range... Rest>
    requires (std::same_as<typename std::remove_cvref_t<First>::key_type,    typename std::remove_cvref_t<Rest>::key_type> && ...)
          && (std::same_as<typename std::remove_cvref_t<First>::mapped_type, typename std::remove_cvref_t<Rest>::mapped_type> && ...)
auto concat(First&& first, Rest&&... rest) {
    using K = typename std::remove_cvref_t<First>::key_type;
    using V = typename std::remove_cvref_t<First>::mapped_type;
    std::unordered_map<K, V> result(std::ranges::begin(first), std::ranges::end(first));
    (result.insert(std::ranges::begin(rest), std::ranges::end(rest)), ...);
    return result;
}

template<std::ranges::input_range R, typename F>
    requires std::ranges::input_range<std::decay_t<std::invoke_result_t<F, std::ranges::range_reference_t<R>>>>
auto flat_map(R&& r, F&& f) {
    using Inner = std::decay_t<std::invoke_result_t<F, std::ranges::range_reference_t<R>>>;
    using T = std::ranges::range_value_t<Inner>;
    std::vector<T> result;
    for (auto&& x : r) {
        auto sub = f(x);
        result.insert(result.end(), std::ranges::begin(sub), std::ranges::end(sub));
    }
    return result;
}

}