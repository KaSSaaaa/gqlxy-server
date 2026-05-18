#pragma once

#include <gqlxy/core/parser/peg/parser/parse_type_ref.h>
#include <gqlxy/server/definitions/enum_value_definition.h>
#include <gqlxy/server/definitions/field_definition.h>
#include <gqlxy/server/definitions/input_value_definition.h>
#include <gqlxy/server/definitions/type_definition.h>
#include <memory>

namespace gqlxy {
struct SchemaDefinition;
}

namespace graphql::peg {
class ast_node;
}

namespace gqlxy::internal {

std::shared_ptr<SchemaDefinition> ParseSchemaDefinition(const std::string& typeDefs);
InputValueDefinition ParseInputValue(const graphql::peg::ast_node& node);
EnumValueDefinition ParseEnumValue(const graphql::peg::ast_node& node);
TypeDefinition ParseType(const graphql::peg::ast_node& node, const TypeKind& kind);
FieldDefinition ParseField(const graphql::peg::ast_node& node);
std::vector<FieldDefinition> ParseFields(const std::optional<graphql::peg::ast_node*>& node);
TypeDefinition ParseObjectType(const graphql::peg::ast_node& node);
TypeDefinition ParseInterfaceType(const graphql::peg::ast_node& node);
std::optional<TypeDefinition> ParseType(const graphql::peg::ast_node& node);

}