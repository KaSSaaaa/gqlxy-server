#pragma once

#include <optional>
#include <string>

#include "DeprecationInfo.h"

namespace ariane::graphql::internal {

struct EnumValueDefinition {
    std::string name;
    std::optional<std::string> description;
    std::optional<DeprecationInfo> deprecation;
};

}
