#pragma once

#include <better-enums/enum.h>
#include <gqlxy/core/parser/ast/directive.h>
#include <gqlxy/server/definitions/enum_value_definition.h>
#include <gqlxy/server/definitions/field_definition.h>
#include <gqlxy/server/definitions/input_value_definition.h>
#include <optional>
#include <string>
#include <vector>

namespace gqlxy {

BETTER_ENUM(TypeKind, int, SCALAR, OBJECT, INTERFACE, UNION, ENUM, INPUT_OBJECT, LIST, NON_NULL);

struct TypeDefinition {
    TypeKind kind = TypeKind::OBJECT;
    std::string name;
    std::optional<std::string> description = std::nullopt;

    std::vector<FieldDefinition> fields = {};
    std::vector<std::string> interfaces = {};
    std::vector<std::string> possibleTypes = {};
    std::vector<std::string> unionTypes = {};
    std::vector<EnumValueDefinition> enumValues = {};
    std::vector<InputValueDefinition> inputFields = {};
    std::vector<parser::Directive> directives = {};
};

}
