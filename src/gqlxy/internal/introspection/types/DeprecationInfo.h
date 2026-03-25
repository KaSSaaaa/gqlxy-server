#pragma once

#include <optional>
#include <string>

namespace gqlxy::internal {

struct DeprecationInfo {
    bool isDeprecated = false;
    std::optional<std::string> deprecationReason;
};

}