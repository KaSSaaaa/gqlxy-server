#pragma once

#include <optional>
#include <string>
#include <vector>

#include "InputValueDefinition.h"
#include "DeprecationInfo.h"
#include "TypeRef.h"

namespace ariane::graphql::internal {

struct FieldDefinition {
    std::string name;
    std::optional<std::string> description;
    TypeRef type;
    std::vector<InputValueDefinition> args;
    std::optional<DeprecationInfo> deprecation;
};

}
