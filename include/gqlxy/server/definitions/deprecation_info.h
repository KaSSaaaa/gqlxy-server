#pragma once

#include <optional>
#include <string>

namespace gqlxy {

struct DeprecationInfo {
    bool isDeprecated = false;
    std::optional<std::string> deprecationReason = std::nullopt;
};

}