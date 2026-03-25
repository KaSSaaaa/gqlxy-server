#pragma once

namespace gqlxy::internal {

template <typename TException = std::runtime_error>
void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw TException(message);
    }
}

}