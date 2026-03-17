#pragma once

#include "DeprecationInfo.h"
#include "TypeRef.h"
#include <optional>
#include <string>

namespace ariane::graphql::internal {

struct InputValueDefinition {
    std::string name;
    std::optional<std::string> description;
    TypeRef type;
    std::optional<std::string> defaultValue;
    std::optional<DeprecationInfo> deprecation;
};

}
