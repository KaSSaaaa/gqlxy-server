#pragma once

#include <string>

namespace gqlxy {

template <typename>
inline constexpr bool is_optional = false;
template <typename T>
inline constexpr bool is_optional<std::optional<T>> = true;

template <typename T>
concept better_enum_value = requires(T e) {
    { std::string(e._to_string()) } -> std::convertible_to<std::string>;
};

template <typename T>
concept better_enum_entry = (!better_enum_value<T>) && requires(T e) {
    { std::string((+e)._to_string()) } -> std::convertible_to<std::string>;
};

}