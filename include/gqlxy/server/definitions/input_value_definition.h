#pragma once

#include "deprecation_info.h"
#include <gqlxy/core/parser/introspection/types/type_ref.h>
#include <optional>
#include <string>

namespace gqlxy {

struct InputValueDefinition {
    std::string name;
    std::optional<std::string> description = std::nullopt;
    parser::TypeRef type;
    std::optional<std::string> defaultValue = std::nullopt;
    std::optional<DeprecationInfo> deprecation = std::nullopt;
};

}
