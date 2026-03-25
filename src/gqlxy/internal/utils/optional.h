#pragma once

#include <optional>

namespace gqlxy::internal {

template <typename T, typename F>
auto and_then(const std::optional<T>& opt, F&& f) -> decltype(f(*opt)) {
    if (opt) {
        return f(*opt);
    }
    return {};
}

template <typename T, typename F>
auto or_else(const std::optional<T>& opt, F&& f) -> std::optional<T> {
    if (opt) {
        return opt;
    }
    return f();
}

}
