#pragma once

#include <optional>
#include <string>

#include "deprecation_info.h"

namespace gqlxy::internal {

struct EnumValueDefinition {
    std::string name;
    std::optional<std::string> description;
    std::optional<DeprecationInfo> deprecation;
};

}
