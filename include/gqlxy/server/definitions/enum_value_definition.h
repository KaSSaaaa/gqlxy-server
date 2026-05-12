#pragma once

#include <optional>
#include <string>

#include "deprecation_info.h"

namespace gqlxy {

struct EnumValueDefinition {
    std::string name;
    std::optional<std::string> description = std::nullopt;
    std::optional<DeprecationInfo> deprecation = std::nullopt;
};

}
