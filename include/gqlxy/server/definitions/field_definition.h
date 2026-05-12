#pragma once

#include "deprecation_info.h"
#include "input_value_definition.h"
#include <gqlxy/core/parser/ast/directive.h>
#include <gqlxy/core/parser/introspection/types/type_ref.h>
#include <optional>
#include <string>
#include <vector>

namespace gqlxy {

struct FieldDefinition {
    std::string name;
    std::optional<std::string> description = std::nullopt;
    parser::TypeRef type;
    std::vector<InputValueDefinition> args = {};
    std::optional<DeprecationInfo> deprecation = std::nullopt;
    std::vector<parser::Directive> directives = {};
};

}
