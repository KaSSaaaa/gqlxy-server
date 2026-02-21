#pragma once

#include <optional>
#include <utility>

namespace ariane::graphql {

template <typename T, typename F>
auto and_then(const std::optional<T>& opt, F&& f) -> decltype(f(*opt)) {
    if (opt) {
        return f(*opt);
    }
    return {};
}

}
