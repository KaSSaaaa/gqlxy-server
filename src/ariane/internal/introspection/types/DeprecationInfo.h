#pragma once

#include <optional>

namespace ariane::graphql::internal {

struct DeprecationInfo {
    bool isDeprecated = false;
    std::optional<std::string> deprecationReason;
};

}