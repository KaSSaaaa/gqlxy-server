#pragma once

#include <gqlxy/server/internal/introspection/types/enum_value_definition.h>
#include <gqlxy/server/internal/introspection/types/field_definition.h>
#include <gqlxy/server/internal/introspection/types/input_value_definition.h>
#include <gqlxy/server/internal/introspection/types/type_definition.h>
#include <gqlxy/core/parser/peg/parser/parse_type_ref.h>
#include <memory>

namespace graphql::peg {
class ast_node;
}

namespace gqlxy::internal {
struct SchemaDefinition;

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