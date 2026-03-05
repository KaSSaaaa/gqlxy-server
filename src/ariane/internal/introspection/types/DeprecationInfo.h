#pragma once

#include <optional>
#include <string>

namespace ariane::graphql::internal {

struct DeprecationInfo {
    bool isDeprecated = false;
    std::optional<std::string> deprecationReason;
};

}