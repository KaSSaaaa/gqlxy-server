#pragma once

#include <optional>
#include <string>

#include "TypeRef.h"

namespace ariane::graphql::internal {

struct InputValueDefinition {
    std::string name;
    std::optional<std::string> description;
    TypeRef type;
    std::optional<std::string> defaultValue;
};

}
