#pragma once

namespace ariane::graphql::internal {

template <typename TException = std::runtime_error>
void expect(bool condition, const std::string& message) {
    if (!condition) {
        throw TException(message);
    }
}

}