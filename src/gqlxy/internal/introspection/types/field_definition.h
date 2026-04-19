#pragma once

#include <optional>
#include <string>
#include <vector>

#include "deprecation_info.h"
#include "input_value_definition.h"
#include <gqlxy/parser/introspection/types/type_ref.h>

namespace gqlxy::internal {

struct FieldDefinition {
    std::string name;
    std::optional<std::string> description;
    parser::TypeRef type;
    std::vector<InputValueDefinition> args;
    std::optional<DeprecationInfo> deprecation;
};

}
