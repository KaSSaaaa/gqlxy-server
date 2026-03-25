#pragma once

#include <better-enums/enum.h>

#include <optional>
#include <string>
#include <vector>

#include "EnumValueDefinition.h"
#include "FieldDefinition.h"
#include "InputValueDefinition.h"
#include <gqlxy/internal/ast/Directive.h>

namespace gqlxy::internal {

BETTER_ENUM(TypeKind, int, SCALAR, OBJECT, INTERFACE, UNION, ENUM, INPUT_OBJECT, LIST, NON_NULL);

struct TypeDefinition {
    TypeKind kind = TypeKind::OBJECT;
    std::string name;
    std::optional<std::string> description;

    std::vector<FieldDefinition> fields;
    std::vector<std::string> interfaces;
    std::vector<std::string> possibleTypes;
    std::vector<std::string> unionTypes;
    std::vector<EnumValueDefinition> enumValues;
    std::vector<InputValueDefinition> inputFields;
    std::vector<Directive> directives;
};

}
