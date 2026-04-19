#pragma once

#include "deprecation_info.h"
#include <gqlxy/parser/introspection/types/type_ref.h>
#include <optional>
#include <string>

namespace gqlxy::internal {

struct InputValueDefinition {
    std::string name;
    std::optional<std::string> description;
    parser::TypeRef type;
    std::optional<std::string> defaultValue;
    std::optional<DeprecationInfo> deprecation;
};

}
